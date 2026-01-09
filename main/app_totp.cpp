#include "app_totp.h"

#include "app_globals.h"
#include "app_render.h"

#if FEATURE_TOTP
#include "base32.h"
#include "i18n.h"

#include <cstring>

// Static buffers for list item labels (must persist)
static char g_totp_labels[TOTP_MAX_ACCOUNTS][72];

static void populate_totp_list(void) {
    uint8_t count = totp_store_count();
    for (uint8_t i = 0; i < count && i < TOTP_MAX_ACCOUNTS; i++) {
        totp_account_info_t info;
        if (totp_store_get_info(i, &info)) {
            // Format: "Name (Issuer)" or just "Name" - truncate safely
            if (strlen(info.issuer) > 0) {
                snprintf(g_totp_labels[i], sizeof(g_totp_labels[i]),
                         "%.30s (%.30s)", info.name, info.issuer);
            } else {
                snprintf(g_totp_labels[i], sizeof(g_totp_labels[i]),
                         "%.30s", info.name);
            }
            g_totp_items[i].label = g_totp_labels[i];
        }
    }
    if (count > 0) {
        view_list_screen_init(&g_totp_list, i18n_str(STR_TOTP_CODES), g_totp_items, count);
    }
}

void go_to_totp_list(void) {
    populate_totp_list();
    uint8_t count = totp_store_count();
    // Always show list (even when empty) so user can access context menu to add new codes
    view_list_screen_init(&g_totp_list, i18n_str(STR_TOTP_CODES), g_totp_items, count);
    g_totp_list.hint = i18n_str(STR_HINT_LIST_MENU);  // Show [3] Menu hint
    g_app_state = APP_STATE_TOTP_LIST;
    render_current_state(false);
}

void show_totp_code(uint8_t index) {
    g_totp_selected_index = index;
    char code[12];
    int8_t remaining = totp_store_generate_code(index, code);

    totp_account_info_t info;
    totp_store_get_info(index, &info);

    // Initialize TOTP code view
    view_totp_code_init(&g_totp_code,
                        info.issuer[0] ? info.issuer : NULL,
                        info.name,
                        remaining >= 0 ? code : NULL,
                        info.digits,
                        info.period,
                        remaining,
                        i18n_str(STR_HINT_TOTP_CODE));

    g_app_state = APP_STATE_TOTP_CODE;
    render_current_state(false);
}

// TOTP Add/Edit Wizard helpers
void totp_wizard_start(void) {
    // Reset wizard data for new entry
    memset(&g_totp_wizard, 0, sizeof(g_totp_wizard));
    g_totp_wizard.digits = 6;     // Default
    g_totp_wizard.algorithm = 0;  // SHA-1 default
    g_totp_wizard.period = 30;    // Default
    g_totp_wizard.edit_mode = false;

    // Start with name input
    view_t9_input_init(&g_t9_input, "Account Name", "");
    g_app_state = APP_STATE_TOTP_ADD_NAME;
    render_current_state(false);
}

void totp_wizard_edit(uint8_t index) {
    // Pre-fill wizard with existing data (including secret from TROPIC01)
    totp_account_t account;
    if (!totp_store_get(index, &account)) {
        view_toast_error("Load failed", 1500);
        return;
    }

    memset(&g_totp_wizard, 0, sizeof(g_totp_wizard));
    strncpy(g_totp_wizard.name, account.name, sizeof(g_totp_wizard.name) - 1);
    strncpy(g_totp_wizard.issuer, account.issuer, sizeof(g_totp_wizard.issuer) - 1);
    // Encode secret back to base32 for editing
    base32_encode(account.secret, account.secretLen, g_totp_wizard.secret, sizeof(g_totp_wizard.secret));
    g_totp_wizard.digits = account.digits;
    g_totp_wizard.algorithm = account.algorithm;
    g_totp_wizard.period = account.period;
    g_totp_wizard.edit_mode = true;
    g_totp_wizard.edit_index = index;

    // Clear secret from RAM after encoding
    memset(account.secret, 0, sizeof(account.secret));

    // Start with name input (pre-filled)
    view_t9_input_init(&g_t9_input, "Account Name", g_totp_wizard.name);
    g_app_state = APP_STATE_TOTP_ADD_NAME;
    render_current_state(false);
}

void totp_wizard_next_from_name(void) {
    // Save name and go to secret
    strncpy(g_totp_wizard.name, g_t9_input.buffer, sizeof(g_totp_wizard.name) - 1);
    // Pre-fill secret if editing (already loaded from chip)
    view_t9_input_init(&g_t9_input, "Secret (Base32)", g_totp_wizard.secret);
    g_app_state = APP_STATE_TOTP_ADD_SECRET;
    render_current_state(false);
}

void totp_wizard_next_from_secret(void) {
    // Save secret and go to issuer
    strncpy(g_totp_wizard.secret, g_t9_input.buffer, sizeof(g_totp_wizard.secret) - 1);
    // Pre-fill issuer if editing
    view_t9_input_init(&g_t9_input, "Issuer (optional)", g_totp_wizard.issuer);
    g_app_state = APP_STATE_TOTP_ADD_ISSUER;
    render_current_state(false);
}

void totp_wizard_next_from_issuer(void) {
    // Save issuer and go to digits selection
    strncpy(g_totp_wizard.issuer, g_t9_input.buffer, sizeof(g_totp_wizard.issuer) - 1);
    view_list_screen_init(&g_totp_digits_menu, "Digits", g_totp_digits_items, 3);
    g_app_state = APP_STATE_TOTP_ADD_DIGITS;
    render_current_state(false);
}

void totp_wizard_next_from_digits(uint8_t selection) {
    // Save digits (0=6 digits, 1=4 digits, 2=8 digits)
    static const uint8_t digit_map[] = {6, 4, 8};
    g_totp_wizard.digits = digit_map[selection % 3];
    view_list_screen_init(&g_totp_algo_menu, "Algorithm", g_totp_algo_items, 3);
    g_app_state = APP_STATE_TOTP_ADD_ALGO;
    render_current_state(false);
}

void totp_wizard_next_from_algo(uint8_t selection) {
    // Save algorithm (0=SHA-1, 1=SHA-256, 2=SHA-512)
    g_totp_wizard.algorithm = selection % 3;
    view_list_screen_init(&g_totp_period_menu, "Period", g_totp_period_items, 2);
    g_app_state = APP_STATE_TOTP_ADD_PERIOD;
    render_current_state(false);
}

void totp_wizard_finish(uint8_t selection) {
    // Save period (0=30s, 1=60s)
    g_totp_wizard.period = (selection == 0) ? 30 : 60;

    // Validate required fields
    if (strlen(g_totp_wizard.name) == 0) {
        view_toast_error("Name required", 1500);
        go_to_totp_list();
        return;
    }
    if (strlen(g_totp_wizard.secret) == 0) {
        view_toast_error("Secret required", 1500);
        go_to_totp_list();
        return;
    }

    // Add to store
    int8_t result = totp_store_add(
        g_totp_wizard.name,
        strlen(g_totp_wizard.issuer) > 0 ? g_totp_wizard.issuer : NULL,
        g_totp_wizard.secret,
        g_totp_wizard.digits,
        g_totp_wizard.period,
        0  // SHA-1 default
    );

    if (result >= 0) {
        view_toast_show(i18n_str(STR_SAVED), 1500);
    } else {
        view_toast_error(i18n_str(STR_SAVE_FAILED), 1500);
    }

    // Return to list
    go_to_totp_list();
}
#endif
