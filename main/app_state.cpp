#include "app_state.h"

#include "app_menus.h"
#include "app_render.h"

#include "cdc_time.h"
#include "gui.h"
#include "i18n.h"
#include "pin_storage.h"

// Security lockout: 1 minute lockout after too many failed PIN attempts
#define LOCKOUT_DURATION_MS (60 * 1000)  // 1 minute

// Support 4-6 digit PINs, require Y to confirm
void go_to_pin_entry(void) {
    view_pin_entry_init(&g_pin_entry, i18n_str(STR_ENTER_PIN), PIN_MAX_LEN, 3);
    g_app_state = APP_STATE_PIN_ENTRY;
    render_current_state(false);
}

void go_to_main_menu(void) {
    build_main_menu();
    view_list_screen_init(&g_main_menu, i18n_str(STR_MAIN_MENU), g_menu_items, g_menu_item_count);
    g_app_state = APP_STATE_MAIN_MENU;
    g_last_activity_ms = millis();  // Start autolock timer
    render_current_state(false);
}

void go_to_settings_menu(void) {
    build_settings_menu();
    view_list_screen_init(&g_settings_menu, i18n_str(STR_SETTINGS), g_settings_items, SETTINGS_IDX_COUNT);
    g_app_state = APP_STATE_SETTINGS_MENU;
    render_current_state(false);
}

void go_to_badge_texts_menu(void) {
    build_badge_texts_menu();
    view_list_screen_init(&g_badge_texts_menu, i18n_str(STR_BADGE_TEXTS), g_badge_texts_items, BADGE_IDX_COUNT);
    g_app_state = APP_STATE_BADGE_TEXTS_MENU;
    render_current_state(false);
}

void go_to_lock_screen(void) {
    // Turn off backlight when entering lock screen (unless forced on)
    if (!g_backlight_forced_on) {
        gui_backlight_off();
    }
    update_lock_screen_data();
    g_app_state = APP_STATE_LOCK_SCREEN;
    g_lock_screen_entered_ms = millis();  // Track when we entered lock screen
    g_last_activity_ms = 0;  // Reset autolock timer (re-starts on unlock)
    render_current_state(false);
}

#if !DEBUG_MODE
void go_to_lockout(void) {
    // Set lockout end time (1 minute from now)
    g_lockout_end_ms = millis() + LOCKOUT_DURATION_MS;

    // Turn off backlight
    if (!g_backlight_forced_on) {
        gui_backlight_off();
    }

    g_app_state = APP_STATE_LOCKOUT;
    render_current_state(false);
}
#endif
