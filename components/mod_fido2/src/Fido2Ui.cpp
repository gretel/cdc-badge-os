/**
 * \file
 * \brief FIDO2 UI views and user-presence approval workflow.
 */

#include "mod_fido2/Fido2Ui.h"
#include "mod_fido2/ctaphid.h"
#include "mod_fido2/fido2.h"
#include "mod_fido2/fido2_storage.h"
#include "cdc_core/KeyFingerprint.h"
#include "cdc_core/PinManager.h"
#include "cdc_hal/IDisplay.h"
#include "cdc_log.h"
#include "cdc_ui/I18n.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_views/ConfirmView.h"
#include "cdc_views/ContextMenuView.h"
#include "cdc_views/InfoView.h"
#include "cdc_views/ListView.h"
#include "cdc_views/PinEntryView.h"
#include "cdc_views/ToastView.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <strings.h>

static const char* TAG = "FIDO2_UI";

namespace cdc::mod_fido2 {

/** \brief Module-specific i18n string offsets. */
static uint16_t s_strIdBase = 0;
static constexpr uint16_t STR_WEB_AUTHN = 0;
static constexpr uint16_t STR_DETAILS = 1;
static constexpr uint16_t STR_FIDO2_KEY = 2;
static constexpr uint16_t STR_SIGN_IN_TO = 3;
static constexpr uint16_t STR_REGISTER_KEY = 4;
static constexpr uint16_t STR_SIGN_IN = 5;
static constexpr uint16_t STR_USE_DEVICE = 6;
static constexpr uint16_t STR_NO_ENTRIES = 7;
static constexpr uint16_t STR_OVERWRITE_KEY = 8;
static constexpr uint16_t STR_OVERWRITE_WARNING = 9;
static constexpr uint16_t STR_COUNT = 10;

/**
 * \brief Resolves module-localized string by offset.
 * \param offset Module string-table offset.
 * \return Translated string pointer.
 */
static const char* mstr(uint16_t offset) {
    return ui::tr(s_strIdBase + offset);
}

/**
 * \brief Registers FIDO2 UI translations.
 */
static void registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("mod_fido2", STR_COUNT);
    if (s_strIdBase == 0) {
        LOG_E(TAG, "Failed to register i18n strings");
        return;
    }

    // English
    i18n.registerTranslation(s_strIdBase + STR_WEB_AUTHN, ui::Language::EN, "WebAuthn");
    i18n.registerTranslation(s_strIdBase + STR_DETAILS, ui::Language::EN, "Details");
    i18n.registerTranslation(s_strIdBase + STR_FIDO2_KEY, ui::Language::EN, "FIDO2 Key");
    i18n.registerTranslation(s_strIdBase + STR_SIGN_IN_TO, ui::Language::EN, "Sign in to");
    i18n.registerTranslation(s_strIdBase + STR_REGISTER_KEY, ui::Language::EN, "Register Key");
    i18n.registerTranslation(s_strIdBase + STR_SIGN_IN, ui::Language::EN, "Sign In");
    i18n.registerTranslation(s_strIdBase + STR_USE_DEVICE, ui::Language::EN, "Use this device?");
    i18n.registerTranslation(s_strIdBase + STR_NO_ENTRIES, ui::Language::EN, "No entries");
    i18n.registerTranslation(s_strIdBase + STR_OVERWRITE_KEY, ui::Language::EN, "OVERWRITE KEY!");
    i18n.registerTranslation(s_strIdBase + STR_OVERWRITE_WARNING, ui::Language::EN, "Overwrite existing key?");

    // German (ASCII)
    i18n.registerTranslation(s_strIdBase + STR_WEB_AUTHN, ui::Language::DE, "WebAuthn");
    i18n.registerTranslation(s_strIdBase + STR_DETAILS, ui::Language::DE, "Details");
    i18n.registerTranslation(s_strIdBase + STR_FIDO2_KEY, ui::Language::DE, "FIDO2 Key");
    i18n.registerTranslation(s_strIdBase + STR_SIGN_IN_TO, ui::Language::DE, "Anmelden bei");
    i18n.registerTranslation(s_strIdBase + STR_REGISTER_KEY, ui::Language::DE, "Schluessel registrieren");
    i18n.registerTranslation(s_strIdBase + STR_SIGN_IN, ui::Language::DE, "Anmelden");
    i18n.registerTranslation(s_strIdBase + STR_USE_DEVICE, ui::Language::DE, "Dieses Geraet nutzen?");
    i18n.registerTranslation(s_strIdBase + STR_NO_ENTRIES, ui::Language::DE, "Keine Eintraege");
    i18n.registerTranslation(s_strIdBase + STR_OVERWRITE_KEY, ui::Language::DE, "SCHLUESSEL UEBERSCHREIBEN!");
    i18n.registerTranslation(s_strIdBase + STR_OVERWRITE_WARNING, ui::Language::DE, "Schluessel ueberschreiben?");
}

/** \brief FIDO2 UI view and list state. */
static ui::ListView* s_listView = nullptr;
static ui::InfoView* s_detailView = nullptr;
static ui::InfoView* s_promptView = nullptr;
static ui::PinEntryView* s_pinEntry = nullptr;

static ui::ListItem s_listItems[FIDO2_MAX_CREDENTIALS];
static char s_labels[FIDO2_MAX_CREDENTIALS][100];
static uint8_t s_sortMap[FIDO2_MAX_CREDENTIALS];
static uint8_t s_listCount = 0;

/** \brief User-presence prompt state shared across callback and UI flow. */
static SemaphoreHandle_t s_promptSem = nullptr;
static volatile fido2_user_presence_result_t s_promptResult = FIDO2_UP_PENDING;
static char s_promptRpId[FIDO2_RP_ID_MAX_LEN] = {};
static fido2_action_t s_promptAction = FIDO2_ACTION_AUTHENTICATE;
static uint8_t s_promptReturnDepth = 0;
static ui::IView* s_promptReturnView = nullptr;
static bool s_promptWasLocked = false;
static bool s_promptBacklightWasOn = false;
static volatile bool s_promptActive = false;  // Race condition guard

/** \brief Pre-confirm modal state for overwrite warning. */
static SemaphoreHandle_t s_overwriteSem = nullptr;
static volatile bool s_overwriteApproved = false;

static void onOverwriteConfirm(void* /*ud*/) {
    s_overwriteApproved = true;
    if (s_overwriteSem) xSemaphoreGive(s_overwriteSem);
}
static void onOverwriteCancel(void* /*ud*/) {
    s_overwriteApproved = false;
    if (s_overwriteSem) xSemaphoreGive(s_overwriteSem);
}

/**
 * \brief Null-safe ASCII case-insensitive comparison.
 * \param a First string.
 * \param b Second string.
 * \return Compare result (`<0`, `0`, `>0`).
 */
static int strcasecmp_safe(const char* a, const char* b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return strcasecmp(a, b);
}

/**
 * \brief Rebuilds credential list view from current storage contents.
 */
static void rebuildList() {
    s_listCount = 0;
    uint8_t count = fido2_get_credential_count();
    if (count == 0) {
        // Show "No entries" placeholder
        s_listItems[0].label = mstr(STR_NO_ENTRIES);
        s_listItems[0].userData = nullptr;
        s_listItems[0].icon = 0;
        s_listItems[0].iconDisabled = true;
        if (s_listView) {
            s_listView->init(mstr(STR_WEB_AUTHN), s_listItems, 1);
            s_listView->setHint(ui::tr(ui::StringId::HINT_BACK));
        }
        return;
    }

    for (uint8_t i = 0; i < count && i < FIDO2_MAX_CREDENTIALS; i++) {
        s_sortMap[i] = i;
        fido2_credential_info_t info = {};
        if (fido2_get_credential_info(i, &info)) {
            if (strlen(info.user_name) > 0) {
                snprintf(s_labels[i], sizeof(s_labels[i]),
                         "%.45s (%.45s)", info.rp_id, info.user_name);
            } else {
                snprintf(s_labels[i], sizeof(s_labels[i]),
                         "%.45s", info.rp_id);
            }
        } else {
            s_labels[i][0] = '\0';
        }
    }

    std::sort(s_sortMap, s_sortMap + count, [](uint8_t a, uint8_t b) {
        return strcasecmp_safe(s_labels[a], s_labels[b]) < 0;
    });

    for (uint8_t display = 0; display < count; display++) {
        uint8_t store_idx = s_sortMap[display];
        s_listItems[display].label = s_labels[store_idx];
        s_listItems[display].userData = reinterpret_cast<void*>(static_cast<uintptr_t>(display));
        s_listItems[display].icon = 0;
        s_listItems[display].iconDisabled = false;
        s_listCount++;
    }

    if (s_listView) {
        s_listView->init(mstr(STR_WEB_AUTHN), s_listItems, s_listCount);
        s_listView->setHint(ui::tr(ui::StringId::HINT_LIST_MENU));
    }
}

/**
 * \brief Shows detailed view for selected credential.
 * \param display_index Display-order index.
 */
static void showDetail(uint16_t display_index) {
    uint8_t count = fido2_get_credential_count();
    if (display_index >= count) return;

    uint8_t store_index = s_sortMap[display_index];
    fido2_credential_info_t info = {};
    if (!fido2_get_credential_info(store_index, &info)) {
        return;
    }

    const char* key_type = (strncmp(info.rp_id, "ssh:", 4) == 0) ? "SSH" : "WebAuthn";
    const char* algo_name = (info.curve == CDC_CURVE_ED25519) ? "Ed25519" : "P-256";

    char fingerprint[KEY_FINGERPRINT_MAX_LEN] = {};
    uint8_t pubkey[64] = {};
    size_t pubkey_len = (info.curve == CDC_CURVE_ED25519) ? 32 : 64;
    if (fido2_storage_get_pubkey(info.slot, pubkey)) {
        key_fingerprint_from_pubkey(pubkey, pubkey_len, fingerprint, sizeof(fingerprint));
    } else {
        strlcpy(fingerprint, "(no key)", sizeof(fingerprint));
    }

    static char detail_text[384];
    uint16_t slot_phys = static_cast<uint16_t>(fido2_storage_ecc_start() + info.slot);
    snprintf(detail_text, sizeof(detail_text),
             "Relying Party:\n%s\n\n"
             "Type: %s  Algo: %s\n"
             "User: %s\n"
             "Slot: %d\n"
             "Sign count: %lu\n"
             "Resident: %s\n\n"
             "Fingerprint:\n%s\n\n"
             "%s",
             info.rp_id,
             key_type, algo_name,
             strlen(info.user_name) > 0 ? info.user_name : "(none)",
             slot_phys,
             info.sign_count,
             info.resident_key ? ui::tr(ui::StringId::YES) : ui::tr(ui::StringId::NO),
             fingerprint,
             ui::tr(ui::StringId::HINT_BACK));

    if (!s_detailView) {
        s_detailView = new ui::InfoView();
    }
    s_detailView->init(mstr(STR_FIDO2_KEY), detail_text);
    ui::ViewStack::instance().push(s_detailView);
}

/**
 * \brief Deletes selected credential and refreshes list.
 * \param display_index Display-order index.
 */
static void handleDelete(uint16_t display_index) {
    uint8_t count = fido2_get_credential_count();
    if (display_index >= count) return;

    uint8_t store_index = s_sortMap[display_index];
    fido2_credential_info_t info = {};
    if (!fido2_get_credential_info(store_index, &info)) {
        return;
    }

    if (fido2_delete_credential(info.slot)) {
        rebuildList();
        if (s_listView) {
            s_listView->init(mstr(STR_WEB_AUTHN), s_listItems, s_listCount);
            s_listView->setHint(ui::tr(ui::StringId::HINT_LIST_MENU));
        }
    }
}

/**
 * \brief List selection callback opening credential detail view.
 * \param index Selected row index.
 * \param userData Optional callback context (unused).
 */
static void onListSelect(uint16_t index, void* userData) {
    (void)userData;
    showDetail(index);
}

/**
 * \brief List menu callback opening context actions for selected credential.
 * \param index Selected row index.
 * \param userData Optional callback context (unused).
 */
static void onListMenu(uint16_t index, void* userData) {
    (void)userData;
    if (s_listCount == 0) return;

    static ui::ContextMenuItem items[3];
    uint16_t sel = s_listView ? s_listView->getSelection() : index;
    if (sel >= s_listCount) {
        sel = 0;
    }
    items[0] = {mstr(STR_DETAILS), []() { showDetail(s_listView ? s_listView->getSelection() : 0); }};
    items[1] = {ui::tr(ui::StringId::DELETE), []() { handleDelete(s_listView ? s_listView->getSelection() : 0); }};
    items[2] = {ui::tr(ui::StringId::CANCEL), []() {}};
    static char s_contextTitle[FIDO2_RP_ID_MAX_LEN] = {};
    const uint8_t store_idx = s_sortMap[sel];
    fido2_credential_info_t info = {};
    const char* title = s_labels[store_idx];
    if (fido2_get_credential_info(store_idx, &info)) {
        strncpy(s_contextTitle, info.rp_id, sizeof(s_contextTitle) - 1);
        s_contextTitle[sizeof(s_contextTitle) - 1] = '\0';
        title = s_contextTitle;
    }
    showContextMenu(title, items, 3);
}

/**
 * \brief Restores view stack to pre-prompt depth.
 */
static void restoreView() {
    auto& stack = ui::ViewStack::instance();
    while (stack.depth() > s_promptReturnDepth) {
        stack.pop();
    }
}

/**
 * \brief Completes user-presence prompt flow with result handling.
 * \param result Final user-presence result.
 */
static void promptComplete(fido2_user_presence_result_t result) {
    s_promptActive = false;

    if (result == FIDO2_UP_APPROVED) {
        ui::showToastSuccess(ui::tr(ui::StringId::OK), 2000);
    } else {
        ui::showToastError(ui::tr(ui::StringId::FAILED), 2000);
    }

    auto& stack = ui::ViewStack::instance();
    stack.releaseExclusive(s_promptView);

    restoreView();

    if (result == FIDO2_UP_APPROVED &&
        (s_promptAction == FIDO2_ACTION_REGISTER || s_promptAction == FIDO2_ACTION_OVERWRITE) &&
        s_promptReturnView == s_listView) {
        rebuildList();
    }

    auto* display = cdc::hal::getDisplayInstance();
    if (display && !s_promptBacklightWasOn) {
        display->backlightOff();
    }

    stack.render();

    s_promptResult = result;
    if (s_promptSem) {
        xSemaphoreGive(s_promptSem);
    }
}

/**
 * \brief PIN verification callback for locked-screen approval flow.
 * \param pin Entered PIN string.
 * \return `true` when badge PIN is valid.
 */
static bool onPinVerify(const char* pin) {
    return cdc::core::PinManager::instance().verifyBadgePin(pin);
}

/**
 * \brief PIN success callback approving user presence.
 */
static void onPinSuccess() {
    auto& stack = ui::ViewStack::instance();
    stack.releaseExclusive(s_pinEntry);
    stack.acquireExclusive(s_promptView);
    promptComplete(FIDO2_UP_APPROVED);
}

/**
 * \brief PIN cancel callback denying user presence.
 */
static void onPinCancel() {
    auto& stack = ui::ViewStack::instance();
    stack.releaseExclusive(s_pinEntry);
    stack.acquireExclusive(s_promptView);
    promptComplete(FIDO2_UP_DENIED);
}

/**
 * \brief PIN failure callback handling lockout vs retry messaging.
 * \param lockedOut `true` when retries are exhausted.
 */
static void onPinFailure(bool lockedOut) {
    if (lockedOut) {
        ui::showToastError(ui::tr(ui::StringId::TOO_MANY_ATTEMPTS), 2000);
        auto& stack = ui::ViewStack::instance();
        stack.releaseExclusive(s_pinEntry);
        stack.acquireExclusive(s_promptView);
        promptComplete(FIDO2_UP_DENIED);
    } else {
        ui::showToastError(ui::tr(ui::StringId::WRONG_PIN), 1000);
    }
}

/**
 * \brief Prompt approve callback; optionally triggers PIN entry on lock screen.
 * \param userData Optional callback context (unused).
 */
static void onPromptApprove(void* userData) {
    (void)userData;
    if (fido2_is_pin_verified()) {
        promptComplete(FIDO2_UP_APPROVED);
        return;
    }

    if (s_promptWasLocked && cdc::core::PinManager::instance().isPinSet()) {
        if (!s_pinEntry) {
            s_pinEntry = new ui::PinEntryView();
            s_pinEntry->init(ui::tr(ui::StringId::ENTER_PIN),
                             cdc::core::PinManager::BADGE_PIN_MAX, 3);
            s_pinEntry->setMinLength(cdc::core::PinManager::BADGE_PIN_MIN);
            s_pinEntry->setOnVerify(onPinVerify);
            s_pinEntry->setOnSuccess(onPinSuccess);
            s_pinEntry->setOnCancel(onPinCancel);
            s_pinEntry->setOnFailure(onPinFailure);
            s_pinEntry->setShowMessages(false);
        } else {
            s_pinEntry->init(ui::tr(ui::StringId::ENTER_PIN),
                             cdc::core::PinManager::BADGE_PIN_MAX, 3);
            s_pinEntry->setMinLength(cdc::core::PinManager::BADGE_PIN_MIN);
            s_pinEntry->setShowMessages(false);
        }
        {
            auto& stack = ui::ViewStack::instance();
            stack.releaseExclusive(s_promptView);
            stack.acquireExclusive(s_pinEntry);
            stack.push(s_pinEntry);
        }
        return;
    }

    promptComplete(FIDO2_UP_APPROVED);
}

/**
 * \brief Prompt deny callback.
 * \param userData Optional callback context (unused).
 */
static void onPromptDeny(void* userData) {
    (void)userData;
    fido2_set_pin_verified(false);
    promptComplete(FIDO2_UP_DENIED);
}

/**
 * \brief Initializes FIDO2 UI resources and list views.
 */
void fido2_ui_init() {
    registerStrings();
    if (!s_listView) {
        s_listView = new ui::ListView();
        s_listView->setOnSelect(onListSelect);
        s_listView->setOnMenu(onListMenu);
    }
    rebuildList();

    if (!s_promptSem) {
        s_promptSem = xSemaphoreCreateBinary();
    }
}

/**
 * \brief Returns FIDO2 credential list view.
 * \return Pointer to list view instance.
 */
cdc::ui::IView* fido2_ui_get_list_view() {
    if (!s_listView) {
        fido2_ui_init();
    }
    rebuildList();
    return s_listView;
}

/**
 * \brief Returns localized module label for menus.
 * \return Label string.
 */
const char* fido2_ui_get_label() {
    return mstr(STR_WEB_AUTHN);
}

/**
 * \brief User-presence callback used by FIDO2 core for approval prompts.
 * \param rp_id Relying-party identifier.
 * \param action Requested action type.
 * \param user_name Optional user-name hint.
 * \return Final presence decision.
 */
fido2_user_presence_result_t fido2_ui_user_presence_callback(
    const char* rp_id,
    fido2_action_t action,
    const char* user_name
) {
    (void)user_name;

    const char* actionStr = (action == FIDO2_ACTION_SELECT) ? "SELECT" :
                            (action == FIDO2_ACTION_REGISTER) ? "REGISTER" :
                            (action == FIDO2_ACTION_OVERWRITE) ? "OVERWRITE" : "AUTH";
    LOG_I(TAG, "User presence: action=%s, rp='%s', strBase=%u, promptActive=%d",
          actionStr, rp_id ? rp_id : "(null)", s_strIdBase, s_promptActive ? 1 : 0);

    // Browser-discovery probes never touch the UI: they are protocol-only
    // pings to ask "is a device there?" and must not influence presence state.
    if (action == FIDO2_ACTION_SELECT && rp_id &&
        (strcmp(rp_id, ".dummy") == 0 || strcmp(rp_id, "make.me.blink") == 0)) {
        LOG_I(TAG, "Auto-approving browser probe '%s'", rp_id);
        return FIDO2_UP_APPROVED;
    }

    // A new presence request while another prompt is already active is rejected.
    // The currently displayed prompt continues to await its own decision; the
    // new request is denied so that no implicit approval can occur.
    if (s_promptActive) {
        LOG_W(TAG, "Presence request while prompt active (action=%s rp='%s') -> deny new",
              actionStr, rp_id ? rp_id : "(null)");
        return FIDO2_UP_DENIED;
    }

    // Ensure strings are registered (safety check)
    if (s_strIdBase == 0) {
        LOG_W(TAG, "Strings not registered, registering now");
        registerStrings();
    }

    if (!s_promptSem) {
        s_promptSem = xSemaphoreCreateBinary();
        if (!s_promptSem) {
            return FIDO2_UP_DENIED;
        }
    }

    if (action == FIDO2_ACTION_OVERWRITE) {
        if (!s_overwriteSem) {
            s_overwriteSem = xSemaphoreCreateBinary();
            if (!s_overwriteSem) {
                return FIDO2_UP_DENIED;
            }
        }
        while (xSemaphoreTake(s_overwriteSem, 0) == pdTRUE) {}

        auto* display = cdc::hal::getDisplayInstance();
        bool backlightWasOn = display && display->isBacklightOn();
        if (display && !backlightWasOn) {
            display->backlightOn();
        }

        const char* confirm_msg = mstr(STR_OVERWRITE_WARNING);

        s_overwriteApproved = false;
        ui::showConfirm(confirm_msg, onOverwriteConfirm, onOverwriteCancel,
                        ui::ConfirmView::Icon::WARNING);

        const uint32_t cid = ctaphid_get_current_cid();
        const TickType_t poll = pdMS_TO_TICKS(100);
        TickType_t remaining = pdMS_TO_TICKS(30000);
        bool approved = false;
        bool timedOut = true;
        while (remaining > 0) {
            TickType_t wait = remaining < poll ? remaining : poll;
            if (xSemaphoreTake(s_overwriteSem, wait) == pdTRUE) {
                approved = s_overwriteApproved;
                timedOut = false;
                break;
            }
            ctaphid_send_keepalive(cid, CTAPHID_STATUS_UPNEEDED);
            remaining -= wait;
        }
        if (timedOut) {
            LOG_W(TAG, "Overwrite confirm timeout");
        }

        if (!approved) {
            if (display && !backlightWasOn) {
                display->backlightOff();
            }
            LOG_I(TAG, "Overwrite denied/timeout - aborting registration");
            return FIDO2_UP_DENIED;
        }
        LOG_I(TAG, "Overwrite acknowledged, proceeding to user-presence prompt");
        action = FIDO2_ACTION_REGISTER;
    }

    strncpy(s_promptRpId, rp_id ? rp_id : "Unknown", sizeof(s_promptRpId) - 1);
    s_promptRpId[sizeof(s_promptRpId) - 1] = '\0';
    s_promptAction = action;
    s_promptResult = FIDO2_UP_PENDING;
    s_promptActive = true;  // Mark prompt as active for race condition detection

    while (xSemaphoreTake(s_promptSem, 0) == pdTRUE) {
        LOG_W(TAG, "Drained stale semaphore");
    }

    if (!s_promptView) {
        s_promptView = new ui::InfoView();
    }

    auto& stack = ui::ViewStack::instance();
    if (!stack.acquireExclusive(s_promptView)) {
        LOG_W(TAG, "Could not acquire ViewStack lock for FIDO2 prompt");
        s_promptActive = false;
        return FIDO2_UP_DENIED;
    }
    s_promptReturnDepth = stack.depth();
    s_promptReturnView = stack.current();
    s_promptWasLocked = (s_promptReturnDepth <= 1) && (action != FIDO2_ACTION_SELECT);

    auto* display = cdc::hal::getDisplayInstance();
    if (display) {
        s_promptBacklightWasOn = display->isBacklightOn();
        if (!s_promptBacklightWasOn) {
            display->backlightOn();
        }
    }

    const char* headline = nullptr;
    if (action == FIDO2_ACTION_SELECT) {
        headline = mstr(STR_USE_DEVICE);
    } else if (action == FIDO2_ACTION_REGISTER) {
        headline = mstr(STR_REGISTER_KEY);
    } else if (action == FIDO2_ACTION_OVERWRITE) {
        headline = mstr(STR_OVERWRITE_KEY);
    } else {
        headline = mstr(STR_SIGN_IN);
    }

    static char prompt_text[260];
    if (action == FIDO2_ACTION_SELECT) {
        snprintf(prompt_text, sizeof(prompt_text),
                 "%s\n\n%s",
                 headline,
                 ui::tr(ui::StringId::HINT_APPROVE_DENY));
    } else if (action == FIDO2_ACTION_OVERWRITE) {
        snprintf(prompt_text, sizeof(prompt_text),
                 "!!! %s !!!\n\n%s\n\n%s\n\n%s",
                 headline,
                 s_promptRpId,
                 mstr(STR_OVERWRITE_WARNING),
                 ui::tr(ui::StringId::HINT_APPROVE_DENY));
    } else {
        snprintf(prompt_text, sizeof(prompt_text),
                 "%s\n\n%s\n\n%s",
                 headline,
                 s_promptRpId,
                 ui::tr(ui::StringId::HINT_APPROVE_DENY));
    }

    const char* title = mstr(STR_WEB_AUTHN);
    LOG_I(TAG, "Prompt title='%s', text='%.50s...'", title ? title : "(null)", prompt_text);

    s_promptView->init(title, prompt_text);
    s_promptView->setYesNoCallbacks(onPromptApprove, onPromptDeny, nullptr);

    stack.push(s_promptView);
    stack.resetInactivityTimer();
    stack.render();

    const uint32_t up_cid = ctaphid_get_current_cid();
    const TickType_t up_poll = pdMS_TO_TICKS(100);
    TickType_t up_remaining = pdMS_TO_TICKS(30000);
    bool up_done = false;
    while (up_remaining > 0) {
        TickType_t wait = up_remaining < up_poll ? up_remaining : up_poll;
        if (xSemaphoreTake(s_promptSem, wait) == pdTRUE) {
            up_done = true;
            break;
        }
        ctaphid_send_keepalive(up_cid, CTAPHID_STATUS_UPNEEDED);
        up_remaining -= wait;
    }
    if (up_done) {
        fido2_user_presence_result_t result = s_promptResult;
        s_promptActive = false;
        if (result == FIDO2_UP_PENDING) {
            LOG_W(TAG, "Semaphore signalled with PENDING result -> deny");
            result = FIDO2_UP_DENIED;
        }
        return result;
    }

    LOG_W(TAG, "User presence timeout");
    s_promptActive = false;
    stack.releaseExclusive(s_promptView);
    restoreView();

    if (display && !s_promptBacklightWasOn) {
        display->backlightOff();
    }

    stack.render();
    return FIDO2_UP_TIMEOUT;
}

bool fido2_ui_abort_prompt() {
    if (!s_promptActive) {
        return false;
    }
    LOG_W(TAG, "Prompt aborted externally");
    promptComplete(FIDO2_UP_DENIED);
    return true;
}

} // namespace cdc::mod_fido2
