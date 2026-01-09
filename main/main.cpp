// CDC Badge - Main Entry Point with Demo App
// Demonstrates all view components

#include "app_globals.h"

#include "app_input.h"
#include "app_menus.h"
#include "app_power.h"
#include "app_render.h"
#include "app_state.h"
#include "app_system.h"
#include "app_totp.h"

#include "badge_settings.h"
#include "cdc_log.h"
#include "cdc_rtc.h"
#include "cdc_time.h"
#include "gui.h"
#include "hw_config.h"
#include "i18n.h"
#include "i2c_bus.h"
#include "ntp_sync.h"
#include "pin_expander.h"
#include "power_management.h"
#include "serial_cmd.h"

#include "driver/rtc_io.h"
#include "esp_heap_caps.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "psa/crypto.h"

#if FEATURE_TOTP
#include "totp_store.h"
#endif

// App version (fallback if not defined via build flags)
#ifndef APP_VERSION
#define APP_VERSION "v0.3"
#endif

// Auto-lock: return to lock screen after inactivity
#define AUTOLOCK_TIMEOUT_MS (3 * 60 * 1000)  // 3 minutes

// Deep sleep: track N key hold time for 5s deep sleep
#define DEEP_SLEEP_HOLD_MS 5000

// T9 abort: track N key hold time for 2s abort
#define T9_ABORT_HOLD_MS 2000

// Light sleep: wait on lock screen before sleeping
#define LIGHT_SLEEP_DELAY_MS 120000  // Wait 120s on lock screen before sleeping

// Track key-hold times in main loop
static uint32_t g_n_key_press_start = 0;
static uint32_t g_lock_n_press_start = 0;

// ============================================================================
// Main Entry Point
// ============================================================================

extern "C" void app_main(void) {
    // Wait for serial to connect
    vTaskDelay(pdMS_TO_TICKS(1000));

    log_init();
    LOG_I("MAIN", "=== CDC Badge ===");

    // Initialize NVS
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_err = nvs_flash_init();
    }
    if (nvs_err != ESP_OK) {
        LOG_E("MAIN", "NVS init failed (err=%d)", nvs_err);
    }

    // Load badge settings from NVS
    badge_settings_load();

    // Initialize i18n (language setting)
    i18n_init();

    // Initialize PSA Crypto (required by libtropic)
    psa_status_t psa_status = psa_crypto_init();
    if (psa_status != PSA_SUCCESS) {
        LOG_E("MAIN", "PSA crypto init failed: %d", psa_status);
    }

    // Initialize I2C buses
    LOG_I("MAIN", "Initializing I2C buses...");
    esp_err_t i2c_err = i2c_bus_init();
    if (i2c_err == ESP_OK) {
        g_hw_status.i2c_ok = true;
        LOG_I("MAIN", "I2C OK");
    } else {
        LOG_E("MAIN", "I2C bus init failed: %d", i2c_err);
    }

    // Initialize power management
    LOG_I("MAIN", "Initializing power management...");
    if (power_management_init()) {
        g_hw_status.power_ok = true;
        LOG_I("MAIN", "BQ25895 OK");
    } else {
        LOG_W("MAIN", "BQ25895 init failed");
    }

    // If waking from deep sleep, deinit RTC GPIO first
    // (EXT1 wakeup may leave GPIO in RTC mode)
    if (g_in_deep_sleep_mode) {
        LOG_D("MAIN", "Deinit RTC GPIO after deep sleep wakeup");
        rtc_gpio_deinit(EXP_IRQ_PIN);
    }

    // Initialize keypad
    LOG_I("MAIN", "Initializing keypad...");
    if (pin_expander_init()) {
        g_hw_status.keypad_ok = true;
        LOG_I("MAIN", "TCA9535 OK");
    } else {
        LOG_W("MAIN", "TCA9535 init failed");
    }

    // Initialize RTC
    LOG_I("MAIN", "Initializing RTC...");
    cdc_rtc_init();

    // Initialize serial command interface
    LOG_I("MAIN", "Initializing serial commands...");
    serial_cmd_init();
    serial_cmd_set_text_callback(on_text_change);
    serial_cmd_set_time_callback(on_time_change);

    // Initialize GUI
    gui_init();

    // Check if waking from deep sleep
    if (g_in_deep_sleep_mode) {
        LOG_I("MAIN", "Woke from deep sleep - continuing to lock screen");
        g_in_deep_sleep_mode = false;  // Clear flag, continue normal boot
    }

    // Show splash screen (normal boot or after deep sleep wake)
    gui_show_splash();

    // Initialize system (TROPIC01) - splash visible during this
    system_init();

    // Initialize lock screen and show it
    // Backlight off on lock screen initially
    gui_backlight_off();
    g_backlight_forced_on = false;
    update_lock_screen_data();
    g_app_state = APP_STATE_LOCK_SCREEN;
    g_lock_screen_entered_ms = millis();  // Track when we entered lock screen
    render_current_state(false);  // Full refresh on boot

    // Wait for display to finish rendering before entering light sleep
    vTaskDelay(pdMS_TO_TICKS(3000));  // E-Paper full refresh takes ~2-3s

    LOG_I("MAIN", "Ready. Press [Y] to unlock");

    while (true) {
        // Handle power button and charger IRQs
        power_management_process();

        // Process serial commands
        serial_cmd_process();

        // Auto-lock: return to lock screen after inactivity
        // Only exempt: lock screen and lockout (already locked)
        if (g_app_state != APP_STATE_LOCK_SCREEN &&
            g_app_state != APP_STATE_LOCKOUT &&
            g_last_activity_ms > 0) {
            uint32_t elapsed = millis() - g_last_activity_ms;
            if (elapsed >= AUTOLOCK_TIMEOUT_MS) {
                LOG_I("LOCK", "Auto-lock: elapsed=%lu, last=%lu, now=%lu",
                      (unsigned long)elapsed, (unsigned long)g_last_activity_ms, (unsigned long)millis());
                go_to_lock_screen();
            }
        }

#if FEATURE_TOTP
        // TOTP code state: update countdown every second
        if (g_app_state == APP_STATE_TOTP_CODE) {
            static uint32_t last_totp_update = 0;
            if (millis() - last_totp_update >= 1000) {
                last_totp_update = millis();
                // Regenerate code and update view
                char code[12];
                int8_t remaining = totp_store_generate_code(g_totp_selected_index, code);
                view_totp_code_update(&g_totp_code, remaining >= 0 ? code : NULL, remaining);
                render_current_state(true);  // Partial refresh
            }
        }
#endif

        // WiFi scan completion check
        if (g_app_state == APP_STATE_WIFI_SCAN) {
            if (wifi_manager_scan_complete()) {
                uint8_t count = wifi_manager_get_network_count();
                if (count == 0) {
                    view_toast_error(i18n_str(STR_WIFI_NO_NETWORKS), 2000);
                    wifi_manager_deinit();
                    go_to_main_menu();
                } else {
                    // Build network list
                    for (uint8_t i = 0; i < count && i < WIFI_MAX_NETWORKS; i++) {
                        wifi_network_t net;
                        if (wifi_manager_get_network(i, &net)) {
                            static char net_labels[WIFI_MAX_NETWORKS][48];
                            snprintf(net_labels[i], sizeof(net_labels[i]), "%s %ddBm%s",
                                    net.ssid, net.rssi,
                                    net.auth_mode == WIFI_AUTH_OPEN ? "" : " *");
                            g_wifi_items[i] = {net_labels[i], ""};
                        }
                    }
                    view_list_screen_init(&g_wifi_list, "WiFi", g_wifi_items, count);
                    g_app_state = APP_STATE_WIFI_LIST;
                    render_current_state(false);
                }
            } else if (millis() - g_wifi_scan_start > 15000) {
                // Scan timeout
                view_toast_error(i18n_str(STR_TIMEOUT), 1500);
                wifi_manager_deinit();
                go_to_main_menu();
            }
        }

        // WiFi connection completion check
        if (g_app_state == APP_STATE_WIFI_CONNECTING) {
            wifi_state_t state = wifi_manager_get_state();
            if (state == WIFI_STATE_CONNECTED) {
                // Save config with last IP
                wifi_config_stored_t config = {};
                strncpy(config.ssid, g_wifi_wizard.ssid, WIFI_SSID_MAX_LEN);
                strncpy(config.password, g_wifi_wizard.password, WIFI_PASSWORD_MAX_LEN);
                config.auth_mode = g_wifi_wizard.auth_mode;
                config.use_dhcp = g_wifi_wizard.use_dhcp;
                config.last_ip = wifi_manager_get_ip();
                wifi_manager_save_config(&config);

                view_toast_success(i18n_str(STR_WIFI_CONNECTED), 1500);
                // Keep connected for Tools > WiFi > Details
                build_tools_wifi_menu();
                view_list_screen_init(&g_tools_wifi_menu, i18n_str(STR_WIFI_MENU), g_tools_wifi_items, 3);
                g_app_state = APP_STATE_TOOLS_WIFI_MENU;
                render_current_state(false);
            } else if (state == WIFI_STATE_FAILED) {
                view_toast_error(i18n_str(STR_WIFI_FAILED), 1500);
                wifi_manager_deinit();
                go_to_main_menu();
            } else if (millis() - g_wifi_connect_start > 20000) {
                // Connection timeout
                view_toast_error(i18n_str(STR_TIMEOUT), 1500);
                wifi_manager_disconnect();
                wifi_manager_deinit();
                go_to_main_menu();
            }
        }

        // NTP sync state machine
        if (g_app_state == APP_STATE_TOOLS_NTP_SYNC) {
            if (g_ntp_sync_phase == 1) {
                // Phase 1: Waiting for WiFi connection
                wifi_state_t state = wifi_manager_get_state();
                if (state == WIFI_STATE_CONNECTED) {
                    // WiFi connected - start NTP sync
                    uint32_t ntp_server = wifi_manager_get_ntp_server();
                    ntp_sync_start(ntp_server);
                    g_ntp_sync_phase = 2;
                    view_info_screen_init(&g_info_view, i18n_str(STR_NTP_SYNC), i18n_str(STR_NTP_SYNCING));
                    render_current_state(false);
                } else if (state == WIFI_STATE_FAILED) {
                    // WiFi connection failed
                    view_toast_error(i18n_str(STR_WIFI_FAILED), 1500);
                    wifi_manager_deinit();
                    g_ntp_sync_phase = 0;
                    build_tools_menu();
                    view_list_screen_init(&g_tools_menu, i18n_str(STR_TOOLS), g_tools_items, 2);
                    g_app_state = APP_STATE_TOOLS_MENU;
                    render_current_state(false);
                } else if (millis() - g_wifi_connect_start > 15000) {
                    // WiFi timeout
                    view_toast_error(i18n_str(STR_TIMEOUT), 1500);
                    wifi_manager_deinit();
                    g_ntp_sync_phase = 0;
                    build_tools_menu();
                    view_list_screen_init(&g_tools_menu, i18n_str(STR_TOOLS), g_tools_items, 2);
                    g_app_state = APP_STATE_TOOLS_MENU;
                    render_current_state(false);
                }
            } else if (g_ntp_sync_phase == 2) {
                // Phase 2: NTP sync in progress
                ntp_state_t ntp_state = ntp_sync_get_state();
                if (ntp_state == NTP_STATE_SUCCESS) {
                    // Success!
                    view_toast_success(i18n_str(STR_NTP_SUCCESS), 1500);
                    ntp_sync_stop();
                    wifi_manager_deinit();
                    g_ntp_sync_phase = 0;
                    build_tools_menu();
                    view_list_screen_init(&g_tools_menu, i18n_str(STR_TOOLS), g_tools_items, 2);
                    g_app_state = APP_STATE_TOOLS_MENU;
                    render_current_state(false);
                } else if (ntp_state == NTP_STATE_FAILED || ntp_sync_timed_out()) {
                    // NTP failed or timed out
                    view_toast_error(i18n_str(STR_NTP_FAILED), 1500);
                    ntp_sync_stop();
                    wifi_manager_deinit();
                    g_ntp_sync_phase = 0;
                    build_tools_menu();
                    view_list_screen_init(&g_tools_menu, i18n_str(STR_TOOLS), g_tools_items, 2);
                    g_app_state = APP_STATE_TOOLS_MENU;
                    render_current_state(false);
                }
            }
        }

        // Lockout state: countdown and auto-return to lock screen
        if (g_app_state == APP_STATE_LOCKOUT) {
            // Check if lockout has expired
            if (millis() >= g_lockout_end_ms) {
                go_to_lock_screen();
            } else {
                // Update countdown every second
                static uint32_t last_update = 0;
                if (millis() - last_update >= 1000) {
                    last_update = millis();
                    render_current_state(true);  // Update countdown display
                }
                // Enter light sleep during lockout to save power
                // Wake up every second to update countdown
                enter_light_sleep();
            }
        }

        // Lock screen: use light sleep for power saving
        if (g_app_state == APP_STATE_LOCK_SCREEN) {
            // Check for USB status change and update display immediately
            static bool last_usb_status = false;
            bool current_usb_status = power_is_usb_connected();
            if (current_usb_status != last_usb_status) {
                last_usb_status = current_usb_status;
                LOG_D("USB", "Status changed: %s", current_usb_status ? "connected" : "disconnected");
                update_lock_screen_data();
                render_current_state(true);
            }

            // Check for N long-press (5s) -> deep sleep
            if (pin_expander_is_key_down('N')) {
                if (g_lock_n_press_start == 0) {
                    g_lock_n_press_start = millis();
                } else if (millis() - g_lock_n_press_start >= DEEP_SLEEP_HOLD_MS) {
                    g_lock_n_press_start = 0;
                    enter_deep_sleep();  // Does not return
                }
            } else {
                g_lock_n_press_start = 0;
            }

            // Enter light sleep after 5s idle on lock screen (saves power)
            // Skip light sleep when USB is connected (keep device responsive)
            uint32_t time_on_lock = millis() - g_lock_screen_entered_ms;
            bool usb_connected = power_is_usb_connected();
            if (!pin_expander_has_key() && !pin_expander_any_key_down() &&
                !usb_connected && time_on_lock >= LIGHT_SLEEP_DELAY_MS) {
                // Show light sleep indicator
                g_lock_screen.show_light_sleep_icon = true;
                render_current_state(true);

                LOG_D("SLEEP", "Entering light sleep...");
                fflush(stdout);
                enter_light_sleep();
                // Longer delay for I2C/USB to stabilize after sleep
                vTaskDelay(pdMS_TO_TICKS(200));

                // Check wakeup cause - only reset timer and hide icon if user pressed a key
                esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
                if (cause == ESP_SLEEP_WAKEUP_GPIO) {
                    LOG_D("SLEEP", "Woke up (key press)");
                    g_lock_screen.show_light_sleep_icon = false;  // Hide on key press
                    g_lock_screen_entered_ms = millis();  // User interaction - reset timer
                    update_lock_screen_data();  // Refresh USB/battery status
                    render_current_state(true);  // Refresh display immediately
                } else {
                    LOG_D("SLEEP", "Woke up (timer)");
                    // Timer wakeup - keep icon, don't reset timer, go back to sleep after clock update
                }
                fflush(stdout);
            }

            // After wakeup: check if clock needs update
            if (cdc_rtc_is_time_set()) {
                struct tm timeinfo;
                cdc_rtc_get_time(&timeinfo);
                if (timeinfo.tm_min != g_last_minute) {
                    g_last_minute = timeinfo.tm_min;
                    update_lock_screen_data();
                    render_current_state(true);
                    // Wait for partial refresh to complete before sleeping again
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
            }
        }

        // Update T9 state (for cursor timeout)
        // T9 input states - update display and check for 2s N abort
        bool is_t9_state = (g_app_state == APP_STATE_T9_DEMO ||
                            g_app_state == APP_STATE_BADGE_EDIT_NAME ||
                            g_app_state == APP_STATE_BADGE_EDIT_INFO ||
                            g_app_state == APP_STATE_BADGE_EDIT_INFO2 ||
                            g_app_state == APP_STATE_TOTP_ADD_NAME ||
                            g_app_state == APP_STATE_TOTP_ADD_SECRET ||
                            g_app_state == APP_STATE_TOTP_ADD_ISSUER);
        if (is_t9_state) {
            view_t9_input_update(&g_t9_input);

            // Check for N long-press abort (2s)
            if (pin_expander_is_key_down('N')) {
                if (g_n_key_press_start == 0) {
                    g_n_key_press_start = millis();
                } else if (millis() - g_n_key_press_start >= T9_ABORT_HOLD_MS) {
                    g_n_key_press_start = 0;
                    // Abort without saving - return to appropriate list
                    if (g_app_state == APP_STATE_T9_DEMO) {
                        go_to_main_menu();
                    } else if (g_app_state == APP_STATE_TOTP_ADD_NAME ||
                               g_app_state == APP_STATE_TOTP_ADD_SECRET ||
                               g_app_state == APP_STATE_TOTP_ADD_ISSUER) {
                        go_to_totp_list();
                    } else {
                        go_to_badge_texts_menu();
                    }
                }
            } else {
                g_n_key_press_start = 0;
            }
        }

        // Check for keypad input
        char key = pin_expander_get_key();
        if (key != 'x') {
            handle_key(key);
        }

        vTaskDelay(pdMS_TO_TICKS(20));  // 50Hz loop
    }
}
