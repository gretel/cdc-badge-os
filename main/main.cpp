/**
 * CDC Badge OS v0.5 - Modular Rewrite
 * USB CDC + Serial + Display + UI
 */

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "cdc_log.h"

#include "cdc_core/ServiceRegistry.h"
#include "cdc_core/EventBus.h"
#include "cdc_core/ModuleRegistry.h"
#include "cdc_core/TropicStorage.h"
#include "cdc_core/AttestationKeyService.h"
#include "modules_init.gen.h"  // Auto-generated module registrations
#include "usb_badge/usb_cdc.h"
#include "serial_cmd/SerialCmd.h"
#include "cdc_hal/IDisplay.h"
#include "cdc_hal/II2cBus.h"
#include "cdc_hal/IKeypad.h"
#include "cdc_hal/IPowerManager.h"
#include "cdc_hal/ISecureElement.h"
#include "cdc_hal/ISleepController.h"
#include "cdc_hal/IWifiController.h"
#include "cdc_hal/IBluetoothController.h"
#include "cdc_hal/hw_config.h"
#include "cdc_hal/IRtc.h"
#include "cdc_os_ui/AppUi.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include "esp_timer.h"

static const char* TAG = "BOOT";

using namespace cdc::core;

// HAL instances
static cdc::hal::II2cBus* s_i2cBus = nullptr;
static cdc::hal::IKeypad* s_keypad = nullptr;
static cdc::hal::IPowerManager* s_powerManager = nullptr;
static cdc::hal::ISecureElement* s_secureElement = nullptr;
static cdc::hal::ISleepController* s_sleepController = nullptr;
static cdc::core::AttestationKeyService s_attestationService;

extern "C" void app_main(void)
{
    // === STAGE 0: Hardware Minimum ===
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // === STAGE 1: Core Services ===
    // Initialize EventBus
    if (!EventBus::instance().init()) {
        // Can't log yet, USB not ready
        return;
    }

    // Initialize USB CDC (before logging)
    if (!usb_cdc_init()) {
        // USB failed, can't output
        return;
    }

    // Initialize logging system (needs USB CDC)
    log_init();

    // Now we can log!
    LOG_I(TAG, "=================================");
    LOG_I(TAG, "CDC Badge OS v0.5");
    LOG_I(TAG, "Modular Rewrite");
    LOG_I(TAG, "=================================");

    LOG_I(TAG, "EventBus ready");
    LOG_I(TAG, "USB CDC ready");
    LOG_I(TAG, "ServiceRegistry ready (capacity: %u)", ServiceRegistry::MAX_SERVICES);

    // === TIME VALIDATION ===
    cdc::hal::IRtc* rtc = cdc::hal::getRtcInstance();
    if (rtc) {
        rtc->init();
    }
    if (!rtc || !rtc->isTimeSet()) {
        LOG_W(TAG, "System time not set");
    }

    // === I2C BUS INITIALIZATION ===
    // Check if waking from deep sleep - deinit RTC GPIO first
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT1) {
        LOG_D(TAG, "Woke from deep sleep, deinit RTC GPIO");
        rtc_gpio_deinit(EXP_IRQ_PIN);
    }

    LOG_I(TAG, "Initializing I2C bus...");
    s_i2cBus = cdc::hal::getI2cBus0();
    if (s_i2cBus && s_i2cBus->init()) {
        LOG_I(TAG, "I2C bus ready");
    } else {
        LOG_E(TAG, "I2C bus init failed!");
    }

    // === POWER MANAGEMENT INITIALIZATION ===
    LOG_I(TAG, "Initializing Power Management...");
    s_powerManager = cdc::hal::getPowerManagerInstance();
    if (s_powerManager && s_powerManager->init() && s_powerManager->start()) {
        LOG_I(TAG, "Power Management ready (BQ25895)");
        LOG_I(TAG, "Battery: %d%% (%dmV)", s_powerManager->getBatteryPercent(),
              s_powerManager->getBatteryVoltage());
    } else {
        LOG_E(TAG, "Power Management init failed!");
    }

    // === SLEEP CONTROLLER INITIALIZATION ===
    LOG_I(TAG, "Initializing Sleep Controller...");
    s_sleepController = cdc::hal::getSleepControllerInstance();
    if (s_sleepController && s_sleepController->init() && s_sleepController->start()) {
        LOG_I(TAG, "Sleep Controller ready (interval: %lus)",
              (unsigned long)s_sleepController->getLightSleepInterval());
    } else {
        LOG_E(TAG, "Sleep Controller init failed!");
    }

    // === WIFI CONTROLLER INITIALIZATION ===
    LOG_I(TAG, "Initializing WiFi Controller...");
    cdc::hal::IWifiController* wifiController = cdc::hal::getWifiControllerInstance();
    if (wifiController && wifiController->init() && wifiController->start()) {
        LOG_I(TAG, "WiFi Controller ready");
    } else {
        LOG_E(TAG, "WiFi Controller init failed!");
    }

    // === BLUETOOTH CONTROLLER INITIALIZATION ===
    LOG_I(TAG, "Initializing Bluetooth Controller...");
    cdc::hal::IBluetoothController* btController = cdc::hal::getBluetoothControllerInstance();
    if (btController && btController->init() && btController->start()) {
        LOG_I(TAG, "Bluetooth Controller ready");
    } else {
        LOG_E(TAG, "Bluetooth Controller init failed!");
    }

    // === KEYPAD INITIALIZATION ===
    LOG_I(TAG, "Initializing Keypad...");
    s_keypad = cdc::hal::getKeypadInstance();
    if (s_keypad && s_keypad->init()) {
        LOG_I(TAG, "Keypad ready (12 keys)");
    } else {
        LOG_E(TAG, "Keypad init failed!");
    }

    // === SECURE ELEMENT INITIALIZATION ===
    LOG_I(TAG, "Initializing Secure Element...");
    s_secureElement = cdc::hal::getSecureElementInstance();
    if (s_secureElement && s_secureElement->init() && s_secureElement->start()) {
        if (s_secureElement->sessionStart()) {
            LOG_I(TAG, "Secure Element ready (TROPIC01, session active)");
        } else {
            LOG_W(TAG, "Secure Element initialized but session start failed");
        }
    } else {
        LOG_E(TAG, "Secure Element init failed!");
    }

    // === ATTESTATION KEY SERVICE ===
    s_attestationService.setSecureElement(s_secureElement);
    s_attestationService.init();
    s_attestationService.start();
    ServiceRegistry::instance().registerService("attestation_key", &s_attestationService);

    // === TROPIC STORAGE CACHE ===
    auto& tropicStorage = cdc::core::TropicStorage::instance();
    tropicStorage.setSecureElement(s_secureElement);
    tropicStorage.init();
    tropicStorage.start();
    ServiceRegistry::instance().registerService("tropic_storage", &tropicStorage);
    LOG_I(TAG, "TropicStorage cache ready");

    // Initialize Serial Command Interface
    cdc::serial::SerialCmd::init();
    LOG_I(TAG, "Serial Command Interface ready");

    // === DISPLAY INITIALIZATION ===
    LOG_I(TAG, "Initializing Display...");
    cdc::hal::IDisplay* display = cdc::hal::getDisplayInstance();
    if (display && display->init() && display->start()) {
        LOG_I(TAG, "Display ready (%ux%u)", display->getWidth(), display->getHeight());

        // Show boot splash screen
        display->showSplash();
        LOG_I(TAG, "Splash screen done");
    } else {
        LOG_E(TAG, "Display init failed!");
    }

    // === UI INITIALIZATION ===
    LOG_I(TAG, "Initializing UI...");
    cdc::ui::UiDeps deps;
    deps.display = display;
    deps.keypad = s_keypad;
    deps.power = s_powerManager;
    deps.sleep = s_sleepController;
    deps.secureElement = s_secureElement;
    cdc::ui::ui_init(deps);

    // === MODULE INITIALIZATION ===
    // Register all modules (configured in main/CMakeLists.txt)
    modules_register_all();

    // Run all registered module initializers
    LOG_I(TAG, "Initializing modules...");
    cdc::core::ModuleRegistry::instance().runAllInitializers();

    // Rebuild UI menus with module items
    cdc::ui::ui_on_modules_ready();

    // Finalize USB configuration (re-enumerate if early debug, or start if deferred)
    usb_cdc_start();

    LOG_I(TAG, "=================================");
    LOG_I(TAG, "System ready. Entering main loop.");
    LOG_I(TAG, "=================================");

    // === Main Loop ===
    while (true) {
        EventBus::instance().process();
        cdc::serial::SerialCmd::process();

        // Update power manager (handles charger IRQs)
        if (s_powerManager) {
            s_powerManager->update();
        }

        uint32_t nowMs = esp_timer_get_time() / 1000;
        cdc::ui::ui_process(nowMs);
        s_attestationService.onTick(nowMs);

        // Dispatch tick to all modules
        cdc::core::ModuleRegistry::instance().dispatchTick(nowMs);

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
