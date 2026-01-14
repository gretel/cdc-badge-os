#include "app_fido.h"

#include "app_globals.h"
#include "app_render.h"

#if FEATURE_FIDO2
#include "cdc_log.h"
#include "cdc_time.h"
#include "gui.h"
#include "i18n.h"
#include "tropic01.h"

#include <esp_attr.h>
#include <cstring>
#include <algorithm>

// Static buffers for list item labels (must persist) - in PSRAM to save DRAM
EXT_RAM_BSS_ATTR static char g_fido_labels[FIDO2_MAX_CREDENTIALS][100];

// Comparison helper for sorting - case-insensitive string compare
static int strcasecmp_safe(const char *a, const char *b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return strcasecmp(a, b);
}

void build_fido_context_menu(void) {
    g_fido_context_items_count = 0;

    // Standard items
    g_fido_context_items_buf[g_fido_context_items_count++] = {"Details", 1};
    g_fido_context_items_buf[g_fido_context_items_count++] = {"Delete", 2};

    // Cancel is always last
    g_fido_context_items_buf[g_fido_context_items_count++] = {"Cancel", 0};
}

static void populate_fido_list(void) {
    uint8_t count = fido2_get_credential_count();
    if (count == 0) return;

    // Build labels and initialize sort map
    for (uint8_t i = 0; i < count && i < FIDO2_MAX_CREDENTIALS; i++) {
        g_fido_sort_map[i] = i;  // Initialize: display index = store index
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
        }
    }

    // Sort the sort map by label (case-insensitive)
    std::sort(g_fido_sort_map, g_fido_sort_map + count, [](uint8_t a, uint8_t b) {
        return strcasecmp_safe(g_fido_labels[a], g_fido_labels[b]) < 0;
    });

    // Reorder items according to sorted map
    for (uint8_t display_idx = 0; display_idx < count; display_idx++) {
        uint8_t store_idx = g_fido_sort_map[display_idx];
        g_fido_items[display_idx].label = g_fido_labels[store_idx];
    }

    view_list_screen_init(&g_fido_list, i18n_str(STR_FIDO2_KEYS), g_fido_items, count);
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

void show_fido_detail(uint8_t display_index) {
    g_fido_selected_index = display_index;
    // Convert display index to store index via sort map
    uint8_t store_index = g_fido_sort_map[display_index];

    fido2_credential_info_t info;
    if (fido2_get_credential_info(store_index, &info)) {
        // Determine key type: SSH keys have "ssh:" prefix in rp_id
        const char *key_type = (strncmp(info.rp_id, "ssh:", 4) == 0) ? "SSH" : "WebAuthn";
        const char *algo_name = (info.curve == CDC_CURVE_ED25519) ? "Ed25519" : "P-256";

        snprintf(g_fido_detail_text, sizeof(g_fido_detail_text),
                 "Relying Party:\n%s\n\n"
                 "Type: %s  Algo: %s\n"
                 "User: %s\n"
                 "Slot: %d\n"
                 "Sign count: %lu\n"
                 "Resident: %s\n\n"
                 "%s",
                 info.rp_id,
                 key_type, algo_name,
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
