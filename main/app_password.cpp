#include "app_password.h"

#if FEATURE_PASSWORD

#include "app_globals.h"
#include "app_render.h"
#include "i18n.h"
#include "password_store.h"
#include <esp_attr.h>

#include <cstring>
#include <cstdio>
#include <strings.h>

// Static buffers for list item labels (must persist) - in PSRAM to save DRAM
EXT_RAM_BSS_ATTR static char g_password_labels[PASSWORD_MAX_ENTRIES][VIEW_MAX_TEXT_LEN];
EXT_RAM_BSS_ATTR static char g_password_names[PASSWORD_MAX_ENTRIES][PASSWORD_NAME_LEN];

static void populate_password_list(void) {
    g_password_count = 0;
    uint16_t slots[PASSWORD_MAX_ENTRIES];
    uint16_t count = password_store_list_slots(slots, PASSWORD_MAX_ENTRIES);

    for (uint16_t i = 0; i < count && g_password_count < PASSWORD_MAX_ENTRIES; i++) {
        password_meta_t meta;
        if (!password_store_get_meta(slots[i], &meta)) {
            continue;
        }

        strncpy(g_password_names[g_password_count], meta.name, PASSWORD_NAME_LEN - 1);
        g_password_names[g_password_count][PASSWORD_NAME_LEN - 1] = '\0';

        if (strlen(meta.username) > 0) {
            snprintf(g_password_labels[g_password_count], sizeof(g_password_labels[g_password_count]),
                     "%.30s (%.30s)", meta.name, meta.username);
        } else {
            snprintf(g_password_labels[g_password_count], sizeof(g_password_labels[g_password_count]),
                     "%.30s", meta.name);
        }

        g_password_slots[g_password_count] = slots[i];
        g_password_items[g_password_count].label = g_password_labels[g_password_count];
        g_password_items[g_password_count].icon = VIEW_LIST_ICON_NONE;
        g_password_items[g_password_count].icon_disabled = false;
        g_password_count++;
    }

    // Sort alphabetically by name (case-insensitive)
    for (uint16_t i = 1; i < g_password_count; i++) {
        uint16_t j = i;
        while (j > 0 && strcasecmp(g_password_names[j - 1], g_password_names[j]) > 0) {
            // Swap names
            char tmp_name[PASSWORD_NAME_LEN];
            memcpy(tmp_name, g_password_names[j - 1], sizeof(tmp_name));
            memcpy(g_password_names[j - 1], g_password_names[j], sizeof(g_password_names[j - 1]));
            memcpy(g_password_names[j], tmp_name, sizeof(g_password_names[j]));

            // Swap labels
            char tmp_label[VIEW_MAX_TEXT_LEN];
            memcpy(tmp_label, g_password_labels[j - 1], sizeof(tmp_label));
            memcpy(g_password_labels[j - 1], g_password_labels[j], sizeof(g_password_labels[j - 1]));
            memcpy(g_password_labels[j], tmp_label, sizeof(g_password_labels[j]));

            // Swap slots
            uint16_t tmp_slot = g_password_slots[j - 1];
            g_password_slots[j - 1] = g_password_slots[j];
            g_password_slots[j] = tmp_slot;

            j--;
        }
    }

    if (g_password_count > 0) {
        view_list_screen_init(&g_password_list, i18n_str(STR_PASSWORDS), g_password_items, g_password_count);
    }
}

void go_to_password_list(void) {
    populate_password_list();
    view_list_screen_init(&g_password_list, i18n_str(STR_PASSWORDS), g_password_items, g_password_count);
    g_password_list.hint = i18n_str(STR_HINT_LIST_MENU);
    g_app_state = APP_STATE_PASSWORD_LIST;
    render_current_state(false);
}

void password_show_detail(uint16_t slot) {
    g_password_selected_slot = slot;

    password_meta_t meta;
    char password[PASSWORD_MAX_LEN + 1];
    char notes[PASSWORD_NOTES_LEN + 1];

    if (!password_store_get_meta(slot, &meta) ||
        !password_store_get_secret(slot, password, sizeof(password), notes, sizeof(notes))) {
        view_toast_error("Load failed", 1500);
        return;
    }

    snprintf(g_password_detail_text, sizeof(g_password_detail_text),
             "Name:\n%.64s\n\n"
             "Username:\n%.64s\n\n"
             "URL:\n%.64s\n\n"
             "Password:\n%.96s\n\n"
             "Notes:\n%.128s\n\n"
             "%s",
             meta.name,
             strlen(meta.username) > 0 ? meta.username : "(none)",
             strlen(meta.url) > 0 ? meta.url : "(none)",
             password,
             strlen(notes) > 0 ? notes : "(none)",
             i18n_str(STR_HINT_PASS_DETAIL));

    memset(password, 0, sizeof(password));
    memset(notes, 0, sizeof(notes));

    view_info_screen_init(&g_info_view, i18n_str(STR_PASSWORD_DETAIL), g_password_detail_text);
    g_app_state = APP_STATE_PASSWORD_DETAIL;
    render_current_state(false);
}

void password_wizard_start(void) {
    memset(&g_password_wizard, 0, sizeof(g_password_wizard));
    g_password_wizard.edit_mode = false;

    view_t9_input_init(&g_t9_input, "Entry Name", "");
    g_app_state = APP_STATE_PASSWORD_ADD_NAME;
    render_current_state(false);
}

void password_wizard_edit(uint16_t slot) {
    password_meta_t meta;
    char password[PASSWORD_MAX_LEN + 1];
    char notes[PASSWORD_NOTES_LEN + 1];

    if (!password_store_get_meta(slot, &meta) ||
        !password_store_get_secret(slot, password, sizeof(password), notes, sizeof(notes))) {
        view_toast_error("Load failed", 1500);
        return;
    }

    memset(&g_password_wizard, 0, sizeof(g_password_wizard));
    strncpy(g_password_wizard.name, meta.name, sizeof(g_password_wizard.name) - 1);
    strncpy(g_password_wizard.username, meta.username, sizeof(g_password_wizard.username) - 1);
    strncpy(g_password_wizard.url, meta.url, sizeof(g_password_wizard.url) - 1);
    strncpy(g_password_wizard.password, password, sizeof(g_password_wizard.password) - 1);
    strncpy(g_password_wizard.notes, notes, sizeof(g_password_wizard.notes) - 1);
    g_password_wizard.edit_mode = true;
    g_password_wizard.edit_slot = slot;

    memset(password, 0, sizeof(password));
    memset(notes, 0, sizeof(notes));

    view_t9_input_init(&g_t9_input, "Entry Name", g_password_wizard.name);
    g_app_state = APP_STATE_PASSWORD_ADD_NAME;
    render_current_state(false);
}

void password_wizard_next_from_name(void) {
    strncpy(g_password_wizard.name, g_t9_input.buffer, sizeof(g_password_wizard.name) - 1);
    view_t9_input_init(&g_t9_input, "Username", g_password_wizard.username);
    g_app_state = APP_STATE_PASSWORD_ADD_USERNAME;
    render_current_state(false);
}

void password_wizard_next_from_username(void) {
    strncpy(g_password_wizard.username, g_t9_input.buffer, sizeof(g_password_wizard.username) - 1);
    view_t9_input_init(&g_t9_input, "URL", g_password_wizard.url);
    g_app_state = APP_STATE_PASSWORD_ADD_URL;
    render_current_state(false);
}

void password_wizard_next_from_url(void) {
    strncpy(g_password_wizard.url, g_t9_input.buffer, sizeof(g_password_wizard.url) - 1);
    view_t9_input_init(&g_t9_input, "Password", g_password_wizard.password);
    g_app_state = APP_STATE_PASSWORD_ADD_PASSWORD;
    render_current_state(false);
}

void password_wizard_next_from_password(void) {
    strncpy(g_password_wizard.password, g_t9_input.buffer, sizeof(g_password_wizard.password) - 1);
    view_t9_input_init(&g_t9_input, "Notes", g_password_wizard.notes);
    g_app_state = APP_STATE_PASSWORD_ADD_NOTES;
    render_current_state(false);
}

void password_wizard_finish(void) {
    strncpy(g_password_wizard.notes, g_t9_input.buffer, sizeof(g_password_wizard.notes) - 1);

    if (strlen(g_password_wizard.name) == 0) {
        view_toast_error("Name required", 1500);
        go_to_password_list();
        return;
    }
    if (strlen(g_password_wizard.password) == 0) {
        view_toast_error("Password required", 1500);
        go_to_password_list();
        return;
    }

    password_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    strncpy(meta.name, g_password_wizard.name, sizeof(meta.name) - 1);
    strncpy(meta.username, g_password_wizard.username, sizeof(meta.username) - 1);
    strncpy(meta.url, g_password_wizard.url, sizeof(meta.url) - 1);

    bool ok = false;
    if (g_password_wizard.edit_mode) {
        ok = password_store_update(g_password_wizard.edit_slot, &meta,
                                   g_password_wizard.password, g_password_wizard.notes);
    } else {
        uint16_t slot = 0;
        ok = password_store_add(&meta, g_password_wizard.password, g_password_wizard.notes, &slot);
    }

    if (ok) {
        view_toast_show(i18n_str(STR_SAVED), 1500);
    } else {
        view_toast_error(i18n_str(STR_SAVE_FAILED), 1500);
    }

    memset(&g_password_wizard, 0, sizeof(g_password_wizard));
    go_to_password_list();
}

#endif // FEATURE_PASSWORD
