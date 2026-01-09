#include "app_fido.h"

#include "app_globals.h"
#include "app_render.h"

#if FEATURE_FIDO2
#include "cdc_log.h"
#include "cdc_time.h"
#include "gui.h"
#include "i18n.h"

#include <cstring>

// Static buffers for list item labels (must persist)
static char g_fido_labels[FIDO2_MAX_CREDENTIALS][100];
static char g_fido_shortcuts[FIDO2_MAX_CREDENTIALS][4];

static void populate_fido_list(void) {
    uint8_t count = fido2_get_credential_count();
    for (uint8_t i = 0; i < count && i < FIDO2_MAX_CREDENTIALS; i++) {
        fido2_credential_info_t info;
        if (fido2_get_credential_info(i, &info)) {
            // Format: "rp_id (user)" or just "rp_id" - truncate safely
            if (strlen(info.user_name) > 0) {
                snprintf(g_fido_labels[i], sizeof(g_fido_labels[i]),
                         "%.45s (%.45s)", info.rp_id, info.user_name);
            } else {
                snprintf(g_fido_labels[i], sizeof(g_fido_labels[i]),
                         "%.45s", info.rp_id);
            }
            snprintf(g_fido_shortcuts[i], sizeof(g_fido_shortcuts[i]), "%d", i + 1);
            g_fido_items[i].label = g_fido_labels[i];
            g_fido_items[i].shortcut = g_fido_shortcuts[i];
        }
    }
    if (count > 0) {
        view_list_screen_init(&g_fido_list, i18n_str(STR_FIDO2_KEYS), g_fido_items, count);
    }
}

void go_to_fido_list(void) {
    populate_fido_list();
    uint8_t count = fido2_get_credential_count();
    // Always show list (even when empty) like TOTP - allows context menu access
    view_list_screen_init(&g_fido_list, i18n_str(STR_FIDO2_KEYS), g_fido_items, count);
    g_fido_list.hint = i18n_str(STR_HINT_LIST_MENU);  // Show [3] Menu hint
    g_app_state = APP_STATE_FIDO_LIST;
    render_current_state(false);
}

void show_fido_detail(uint8_t index) {
    g_fido_selected_index = index;
    fido2_credential_info_t info;
    if (fido2_get_credential_info(index, &info)) {
        snprintf(g_fido_detail_text, sizeof(g_fido_detail_text),
                 "Relying Party:\n%s\n\n"
                 "User: %s\n"
                 "Slot: %d\n"
                 "Sign count: %lu\n"
                 "Resident: %s\n\n"
                 "%s",
                 info.rp_id,
                 strlen(info.user_name) > 0 ? info.user_name : "(none)",
                 info.slot,
                 info.sign_count,
                 info.resident_key ? "Yes" : "No",
                 i18n_str(STR_HINT_BACK));
        view_info_screen_init(&g_info_view, "FIDO2 Key", g_fido_detail_text);
        g_app_state = APP_STATE_FIDO_DETAIL;
        render_current_state(false);
    }
}

// FIDO2 user presence callback - called from USB task context
// Simple flow: Show request → Y/N → (optional PIN) → result
fido2_user_presence_result_t fido2_user_presence_callback(
    const char *rp_id,
    fido2_action_t action,
    const char *user_name
) {
    (void)user_name;
    LOG_I("FIDO2", "User presence: %s @ %s",
          action == FIDO2_ACTION_REGISTER ? "register" : "auth", rp_id ? rp_id : "?");

    // Store request info
    strncpy(g_fido_prompt_rp_id, rp_id ? rp_id : "Unknown", sizeof(g_fido_prompt_rp_id) - 1);
    g_fido_prompt_action = action;
    g_fido_prompt_result = FIDO2_UP_PENDING;

    // Drain stale semaphore signals
    while (xSemaphoreTake(g_fido_prompt_sem, 0) == pdTRUE) {
        LOG_W("FIDO2", "Drained stale semaphore");
    }

    // Save current state for return
    g_fido_return_state = g_app_state;
    g_fido_prompt_action = action;
    // Need PIN only if on lock screen AND not a browser probe (SELECT never needs PIN)
    g_fido_was_locked = (g_app_state == APP_STATE_LOCK_SCREEN) && (action != FIDO2_ACTION_SELECT);

    // Wake up display
    gui_backlight_on();

    // Show simple prompt: "Approve? Y/N"
    // IMPORTANT: static buffer because view_info_screen stores pointer, not copy
    const char *headline;
    if (action == FIDO2_ACTION_SELECT) {
        headline = i18n_str(STR_USE_DEVICE);  // Browser probe - just asking if user wants this authenticator
    } else if (action == FIDO2_ACTION_REGISTER) {
        headline = i18n_str(STR_REGISTER_KEY);
    } else {
        headline = i18n_str(STR_SIGN_IN);
    }
    static char prompt_text[200];
    if (action == FIDO2_ACTION_SELECT) {
        // Browser probe - don't show the fake RP ID (.dummy, make.me.blink)
        snprintf(prompt_text, sizeof(prompt_text),
                 "%s\n\n%s",
                 headline,
                 i18n_str(STR_HINT_APPROVE_DENY));
    } else {
        snprintf(prompt_text, sizeof(prompt_text),
                 "%s\n\n%s\n\n%s",
                 headline,
                 g_fido_prompt_rp_id,
                 i18n_str(STR_HINT_APPROVE_DENY));
    }

    view_info_screen_init(&g_info_view, i18n_str(STR_FIDO2_REQUEST), prompt_text);
    g_app_state = APP_STATE_FIDO_PROMPT;
    g_last_activity_ms = millis();  // Reset autolock timer - active FIDO2 request
    render_current_state(false);

    // Wait for result (30s timeout)
    if (xSemaphoreTake(g_fido_prompt_sem, pdMS_TO_TICKS(30000)) == pdTRUE) {
        LOG_I("FIDO2", "Result: %d", g_fido_prompt_result);
        return g_fido_prompt_result;
    }

    // Timeout
    LOG_W("FIDO2", "Timeout");
    g_app_state = g_fido_return_state;

    // Turn off backlight when returning to lock screen (unless forced on)
    if (g_fido_return_state == APP_STATE_LOCK_SCREEN && !g_backlight_forced_on) {
        gui_backlight_off();
    }

    render_current_state(false);
    return FIDO2_UP_TIMEOUT;
}

void fido2_prompt_complete(fido2_user_presence_result_t result) {
    // Show feedback toast (2s as user requested)
    if (result == FIDO2_UP_APPROVED) {
        view_toast_success(i18n_str(STR_APPROVE), 2000);
    } else {
        view_toast_error(i18n_str(STR_DENY), 2000);
    }

    // Return to previous state
    g_app_state = g_fido_return_state;

    // If a registration was approved while on FIDO list, rebuild the list
    if (result == FIDO2_UP_APPROVED &&
        g_fido_prompt_action == FIDO2_ACTION_REGISTER &&
        g_fido_return_state == APP_STATE_FIDO_LIST) {
        LOG_I("FIDO2", "New key registered - refreshing list");
        populate_fido_list();
    }

    // Turn off backlight when returning to lock screen (unless forced on)
    if (g_fido_return_state == APP_STATE_LOCK_SCREEN && !g_backlight_forced_on) {
        gui_backlight_off();
    }

    render_current_state(false);

    // Signal semaphore AFTER screen is restored
    g_fido_prompt_result = result;
    xSemaphoreGive(g_fido_prompt_sem);
}
#endif
