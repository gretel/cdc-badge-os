#include "mod_vcard/VcardModule.h"
#include "mod_vcard/VcardWizard.h"
#include "mod_vcard/ble_vcard.h"
#include "mod_vcard/vcard_store.h"
#include "serial_cmd/ICommandRegistry.h"
#include "serial_cmd/Console.h"
#include "cdc_core/ModuleRegistry.h"
#include "cdc_ui/I18n.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_views/ListView.h"
#include "cdc_views/InfoView.h"
#include "cdc_views/ConfirmView.h"
#include "cdc_views/QRCodeView.h"
#include "cdc_views/ToastView.h"
#include "cdc_log.h"
#include <cstring>
#include <cstdio>

static const char* TAG = "VCARD";

namespace cdc::mod_vcard {

/**
 * \brief Module-specific translation string IDs.
 */
static uint16_t s_strIdBase = 0;
static constexpr uint16_t STR_VCARD = 0;
static constexpr uint16_t STR_MY_VCARD = 1;
static constexpr uint16_t STR_NEARBY = 2;
static constexpr uint16_t STR_SCAN = 3;
static constexpr uint16_t STR_STOP_SCAN = 4;
static constexpr uint16_t STR_ADVERTISING = 5;
static constexpr uint16_t STR_STOP_ADV = 6;
static constexpr uint16_t STR_EXCHANGE = 7;
static constexpr uint16_t STR_NO_PEERS = 8;
static constexpr uint16_t STR_SCANNING = 9;
static constexpr uint16_t STR_EXCHANGE_REQ = 10;
static constexpr uint16_t STR_ACCEPT = 11;
static constexpr uint16_t STR_DECLINE = 12;
static constexpr uint16_t STR_EXCHANGE_OK = 13;
static constexpr uint16_t STR_EXCHANGE_FAIL = 14;
static constexpr uint16_t STR_CONNECTING = 15;
static constexpr uint16_t STR_EDIT_MY_VCARD = 16;
static constexpr uint16_t STR_NO_VCARD = 17;
static constexpr uint16_t STR_SAVED = 18;
static constexpr uint16_t STR_GIVEN_NAME = 19;
static constexpr uint16_t STR_FAMILY_NAME = 20;
static constexpr uint16_t STR_FORMATTED_NAME = 21;
static constexpr uint16_t STR_ORGANIZATION = 22;
static constexpr uint16_t STR_POSITION = 23;
static constexpr uint16_t STR_EMAIL = 24;
static constexpr uint16_t STR_TEL_CELL = 25;
static constexpr uint16_t STR_TEL_HOME = 26;
static constexpr uint16_t STR_TEL_WORK = 27;
static constexpr uint16_t STR_URL = 28;
static constexpr uint16_t STR_TELEGRAM = 29;
static constexpr uint16_t STR_SIGNAL = 30;
static constexpr uint16_t STR_MATRIX = 31;
static constexpr uint16_t STR_THREEMA = 32;
static constexpr uint16_t STR_SOCIAL_PROFILE = 33;
static constexpr uint16_t STR_NOTE = 34;
static constexpr uint16_t STR_COUNT = 35;

/**
 * \brief Wizard step title offsets in the same order as VcardWizard::WizardStepId.
 */
static const uint16_t s_wizardStepOffsets[16] = {
    STR_GIVEN_NAME,
    STR_FAMILY_NAME,
    STR_FORMATTED_NAME,
    STR_ORGANIZATION,
    STR_POSITION,
    STR_EMAIL,
    STR_TEL_CELL,
    STR_TEL_HOME,
    STR_TEL_WORK,
    STR_URL,
    STR_TELEGRAM,
    STR_SIGNAL,
    STR_MATRIX,
    STR_THREEMA,
    STR_SOCIAL_PROFILE,
    STR_NOTE,
};

/**
 * \brief Resolves module-localized string by offset.
 * \param offset Module string-table offset.
 * \return Translated string pointer.
 */
static const char* mstr(uint16_t offset) {
    return ui::tr(s_strIdBase + offset);
}

/**
 * \brief Registers vCard module translations.
 */
static void registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("mod_vcard", STR_COUNT);
    if (s_strIdBase == 0) {
        LOG_E(TAG, "Failed to register i18n strings");
        return;
    }

    i18n.registerTranslation(s_strIdBase + STR_VCARD,            ui::Language::EN, "vCards");
    i18n.registerTranslation(s_strIdBase + STR_MY_VCARD,         ui::Language::EN, "My vCard");
    i18n.registerTranslation(s_strIdBase + STR_NEARBY,           ui::Language::EN, "Nearby");
    i18n.registerTranslation(s_strIdBase + STR_SCAN,             ui::Language::EN, "Start Scan");
    i18n.registerTranslation(s_strIdBase + STR_STOP_SCAN,        ui::Language::EN, "Stop Scan");
    i18n.registerTranslation(s_strIdBase + STR_ADVERTISING,      ui::Language::EN, "Start Advertising");
    i18n.registerTranslation(s_strIdBase + STR_STOP_ADV,         ui::Language::EN, "Stop Advertising");
    i18n.registerTranslation(s_strIdBase + STR_EXCHANGE,         ui::Language::EN, "Exchange");
    i18n.registerTranslation(s_strIdBase + STR_NO_PEERS,         ui::Language::EN, "No peers found");
    i18n.registerTranslation(s_strIdBase + STR_SCANNING,         ui::Language::EN, "Scanning...");
    i18n.registerTranslation(s_strIdBase + STR_EXCHANGE_REQ,     ui::Language::EN, "Exchange Request");
    i18n.registerTranslation(s_strIdBase + STR_ACCEPT,           ui::Language::EN, "Accept");
    i18n.registerTranslation(s_strIdBase + STR_DECLINE,          ui::Language::EN, "Decline");
    i18n.registerTranslation(s_strIdBase + STR_EXCHANGE_OK,      ui::Language::EN, "Exchange successful");
    i18n.registerTranslation(s_strIdBase + STR_EXCHANGE_FAIL,    ui::Language::EN, "Exchange failed");
    i18n.registerTranslation(s_strIdBase + STR_CONNECTING,       ui::Language::EN, "Connecting...");
    i18n.registerTranslation(s_strIdBase + STR_EDIT_MY_VCARD,    ui::Language::EN, "Edit my vCard");
    i18n.registerTranslation(s_strIdBase + STR_NO_VCARD,         ui::Language::EN, "No vCard set");
    i18n.registerTranslation(s_strIdBase + STR_SAVED,            ui::Language::EN, "Saved");
    i18n.registerTranslation(s_strIdBase + STR_GIVEN_NAME,       ui::Language::EN, "First name");
    i18n.registerTranslation(s_strIdBase + STR_FAMILY_NAME,      ui::Language::EN, "Last name");
    i18n.registerTranslation(s_strIdBase + STR_FORMATTED_NAME,   ui::Language::EN, "Display name");
    i18n.registerTranslation(s_strIdBase + STR_ORGANIZATION,     ui::Language::EN, "Organization");
    i18n.registerTranslation(s_strIdBase + STR_POSITION,         ui::Language::EN, "Position");
    i18n.registerTranslation(s_strIdBase + STR_EMAIL,            ui::Language::EN, "Email");
    i18n.registerTranslation(s_strIdBase + STR_TEL_CELL,         ui::Language::EN, "Phone (Mobile)");
    i18n.registerTranslation(s_strIdBase + STR_TEL_HOME,         ui::Language::EN, "Phone (Home)");
    i18n.registerTranslation(s_strIdBase + STR_TEL_WORK,         ui::Language::EN, "Phone (Work)");
    i18n.registerTranslation(s_strIdBase + STR_URL,              ui::Language::EN, "Website");
    i18n.registerTranslation(s_strIdBase + STR_TELEGRAM,         ui::Language::EN, "Telegram");
    i18n.registerTranslation(s_strIdBase + STR_SIGNAL,           ui::Language::EN, "Signal");
    i18n.registerTranslation(s_strIdBase + STR_MATRIX,           ui::Language::EN, "Matrix");
    i18n.registerTranslation(s_strIdBase + STR_THREEMA,          ui::Language::EN, "Threema");
    i18n.registerTranslation(s_strIdBase + STR_SOCIAL_PROFILE,   ui::Language::EN, "Social Profile");
    i18n.registerTranslation(s_strIdBase + STR_NOTE,             ui::Language::EN, "Note");

    i18n.registerTranslation(s_strIdBase + STR_VCARD,            ui::Language::DE, "vCards");
    i18n.registerTranslation(s_strIdBase + STR_MY_VCARD,         ui::Language::DE, "Meine vCard");
    i18n.registerTranslation(s_strIdBase + STR_NEARBY,           ui::Language::DE, "In der Naehe");
    i18n.registerTranslation(s_strIdBase + STR_SCAN,             ui::Language::DE, "Scan starten");
    i18n.registerTranslation(s_strIdBase + STR_STOP_SCAN,        ui::Language::DE, "Scan stoppen");
    i18n.registerTranslation(s_strIdBase + STR_ADVERTISING,      ui::Language::DE, "Werbung starten");
    i18n.registerTranslation(s_strIdBase + STR_STOP_ADV,         ui::Language::DE, "Werbung stoppen");
    i18n.registerTranslation(s_strIdBase + STR_EXCHANGE,         ui::Language::DE, "Tauschen");
    i18n.registerTranslation(s_strIdBase + STR_NO_PEERS,         ui::Language::DE, "Keine Geraete gefunden");
    i18n.registerTranslation(s_strIdBase + STR_SCANNING,         ui::Language::DE, "Scanne...");
    i18n.registerTranslation(s_strIdBase + STR_EXCHANGE_REQ,     ui::Language::DE, "Tauschanfrage");
    i18n.registerTranslation(s_strIdBase + STR_ACCEPT,           ui::Language::DE, "Annehmen");
    i18n.registerTranslation(s_strIdBase + STR_DECLINE,          ui::Language::DE, "Ablehnen");
    i18n.registerTranslation(s_strIdBase + STR_EXCHANGE_OK,      ui::Language::DE, "Tausch erfolgreich");
    i18n.registerTranslation(s_strIdBase + STR_EXCHANGE_FAIL,    ui::Language::DE, "Tausch fehlgeschlagen");
    i18n.registerTranslation(s_strIdBase + STR_CONNECTING,       ui::Language::DE, "Verbinde...");
    i18n.registerTranslation(s_strIdBase + STR_EDIT_MY_VCARD,    ui::Language::DE, "Meine vCard bearbeiten");
    i18n.registerTranslation(s_strIdBase + STR_NO_VCARD,         ui::Language::DE, "Keine vCard gesetzt");
    i18n.registerTranslation(s_strIdBase + STR_SAVED,            ui::Language::DE, "Gespeichert");
    i18n.registerTranslation(s_strIdBase + STR_GIVEN_NAME,       ui::Language::DE, "Vorname");
    i18n.registerTranslation(s_strIdBase + STR_FAMILY_NAME,      ui::Language::DE, "Nachname");
    i18n.registerTranslation(s_strIdBase + STR_FORMATTED_NAME,   ui::Language::DE, "Anzeigename");
    i18n.registerTranslation(s_strIdBase + STR_ORGANIZATION,     ui::Language::DE, "Organisation");
    i18n.registerTranslation(s_strIdBase + STR_POSITION,         ui::Language::DE, "Position");
    i18n.registerTranslation(s_strIdBase + STR_EMAIL,            ui::Language::DE, "Email");
    i18n.registerTranslation(s_strIdBase + STR_TEL_CELL,         ui::Language::DE, "Tel (Mobil)");
    i18n.registerTranslation(s_strIdBase + STR_TEL_HOME,         ui::Language::DE, "Tel (Privat)");
    i18n.registerTranslation(s_strIdBase + STR_TEL_WORK,         ui::Language::DE, "Tel (Arbeit)");
    i18n.registerTranslation(s_strIdBase + STR_URL,              ui::Language::DE, "Webseite");
    i18n.registerTranslation(s_strIdBase + STR_TELEGRAM,         ui::Language::DE, "Telegram");
    i18n.registerTranslation(s_strIdBase + STR_SIGNAL,           ui::Language::DE, "Signal");
    i18n.registerTranslation(s_strIdBase + STR_MATRIX,           ui::Language::DE, "Matrix");
    i18n.registerTranslation(s_strIdBase + STR_THREEMA,          ui::Language::DE, "Threema");
    i18n.registerTranslation(s_strIdBase + STR_SOCIAL_PROFILE,   ui::Language::DE, "Social Profile");
    i18n.registerTranslation(s_strIdBase + STR_NOTE,             ui::Language::DE, "Notiz");
}

/**
 * \brief View instances used by vCard module UI flow.
 */
static ui::ListView s_mainMenu;
static ui::ListView s_peerList;
static ui::InfoView s_consentView;
static bool s_viewsInitialized = false;

/**
 * \brief Peer discovery storage used for UI list rendering.
 */
static constexpr uint16_t MAX_UI_PEERS = 16;
static vcard_peer_t s_uiPeers[MAX_UI_PEERS] = {};
static uint16_t s_uiPeerCount = 0;
static ui::ListItem s_peerItems[MAX_UI_PEERS + 1] = {};
static char s_peerLabels[MAX_UI_PEERS][48] = {};

/**
 * \brief Main-menu item identifiers.
 */
enum MainMenuItem {
    MENU_MY_VCARD = 0,
    MENU_EDIT_MY_VCARD,
    MENU_NEARBY,
    MENU_SCAN_TOGGLE,
    MENU_ADV_TOGGLE,
    MENU_COUNT
};
static ui::ListItem s_mainMenuItems[MENU_COUNT] = {};

static void rebuildMainMenu();
static void onMainMenuSelect(uint16_t index, void* userData);
static void rebuildPeerList();
static void onPeerSelect(uint16_t index, void* userData);

/**
 * \brief Handles user acceptance of incoming vCard transfer consent.
 * \param userData Optional callback context (unused).
 * \return void
 */
static void onConsentAccept(void* userData) {
    (void)userData;
    ble_vcard_respond_consent(true);
    ui::ViewStack::instance().pop();
    ui::showToastInfo("Waiting for vCard...");
}

/**
 * \brief Handles user decline of incoming vCard exchange request.
 * \param userData Optional callback context (unused).
 */
static void onConsentDecline(void* userData) {
    (void)userData;
    ble_vcard_respond_consent(false);
    ui::ViewStack::instance().pop();
}

/**
 * \brief Displays consent prompt for incoming exchange request.
 * \param peerName Remote peer display name.
 */
static void onConsentRequest(const char* peerName) {
    static char promptText[256];
    snprintf(promptText, sizeof(promptText),
             "%s\n\n%s\nmoechte vCard tauschen\n\n[Y] %s\n[N] %s",
             mstr(STR_EXCHANGE_REQ),
             peerName,
             mstr(STR_ACCEPT),
             mstr(STR_DECLINE));

    s_consentView.init(mstr(STR_EXCHANGE_REQ), promptText);
    s_consentView.setYesNoCallbacks(onConsentAccept, onConsentDecline, nullptr);
    ui::ViewStack::instance().push(&s_consentView);
}

/**
 * \brief Handles completion callback for exchange operations.
 * \param success `true` when exchange succeeded.
 * \param error Optional error text on failure.
 */
static void onExchangeComplete(bool success, const char* error) {
    if (success) {
        ui::showToastSuccess(mstr(STR_EXCHANGE_OK));
    } else {
        if (error && error[0]) {
            ui::showToastError(error);
        } else {
            ui::showToastError(mstr(STR_EXCHANGE_FAIL));
        }
    }
}

/**
 * \brief Rebuilds vCard main menu items from current BLE state.
 */
static void rebuildMainMenu() {
    bool scanning = ble_vcard_is_scan_active();
    bool advertising = ble_vcard_is_adv_active();

    s_mainMenuItems[MENU_MY_VCARD]      = {mstr(STR_MY_VCARD),         0, false, nullptr};
    s_mainMenuItems[MENU_EDIT_MY_VCARD] = {mstr(STR_EDIT_MY_VCARD),    0, false, nullptr};
    s_mainMenuItems[MENU_NEARBY]        = {mstr(STR_NEARBY),           0, false, nullptr};
    s_mainMenuItems[MENU_SCAN_TOGGLE]   = {
        scanning ? mstr(STR_STOP_SCAN) : mstr(STR_SCAN),
        0, false, nullptr
    };
    s_mainMenuItems[MENU_ADV_TOGGLE]    = {
        advertising ? mstr(STR_STOP_ADV) : mstr(STR_ADVERTISING),
        0, false, nullptr
    };

    s_mainMenu.init(mstr(STR_VCARD), s_mainMenuItems, MENU_COUNT);
}

/**
 * \brief Handles main-menu actions for local and nearby vCard operations.
 * \param index Selected menu index.
 * \param userData Optional callback context (unused).
 */
static void onMainMenuSelect(uint16_t index, void* userData) {
    (void)userData;

    switch (index) {
        case MENU_MY_VCARD: {
            static char vcardText[VCARD_MAX_LEN + 1];
            size_t len = vcard_store_get_own(vcardText, sizeof(vcardText));
            if (len > 0) {
                static ui::InfoView infoView;
                infoView.init(mstr(STR_MY_VCARD), vcardText);
                ui::ViewStack::instance().push(&infoView);
            } else {
                ui::showToastInfo(mstr(STR_NO_VCARD));
            }
            break;
        }

        case MENU_EDIT_MY_VCARD:
            if (vcard_store_has_own()) {
                VcardWizard::edit(&s_mainMenu);
            } else {
                VcardWizard::start(&s_mainMenu);
            }
            break;

        case MENU_NEARBY:
            rebuildPeerList();
            ui::ViewStack::instance().push(&s_peerList);
            break;

        case MENU_SCAN_TOGGLE:
            if (ble_vcard_is_scan_active()) {
                ble_vcard_set_scan_enabled(false);
                ui::showToastInfo("Scan stopped");
            } else {
                ble_vcard_set_scan_enabled(true);
                ui::showToastInfo(mstr(STR_SCANNING));
            }
            rebuildMainMenu();
            break;

        case MENU_ADV_TOGGLE:
            if (ble_vcard_is_adv_active()) {
                ble_vcard_set_adv_enabled(false);
                ui::showToastInfo("Advertising stopped");
            } else {
                ble_vcard_set_adv_enabled(true);
                ui::showToastInfo("Advertising started");
            }
            rebuildMainMenu();
            break;
    }
}

/**
 * \brief Rebuilds nearby-peer list from BLE discovery cache.
 */
static void rebuildPeerList() {
    s_uiPeerCount = ble_vcard_get_peers(s_uiPeers, MAX_UI_PEERS);

    if (s_uiPeerCount == 0) {
        s_peerItems[0] = {mstr(STR_NO_PEERS), 0, true, nullptr};
        s_peerList.init(mstr(STR_NEARBY), s_peerItems, 1);
        return;
    }

    for (uint16_t i = 0; i < s_uiPeerCount; i++) {
        snprintf(s_peerLabels[i], sizeof(s_peerLabels[i]),
                 "%s (%ddBm)", s_uiPeers[i].name, s_uiPeers[i].rssi);
        s_peerItems[i] = {s_peerLabels[i], 0, false, reinterpret_cast<void*>(static_cast<uintptr_t>(i))};
    }

    s_peerList.init(mstr(STR_NEARBY), s_peerItems, s_uiPeerCount);
}

/**
 * \brief Starts exchange with selected nearby peer.
 * \param index Selected peer index.
 * \param userData Optional callback context (unused).
 */
static void onPeerSelect(uint16_t index, void* userData) {
    (void)userData;

    if (index >= s_uiPeerCount) return;

    vcard_peer_t& peer = s_uiPeers[index];

    if (ble_vcard_exchange_with(peer.addr, peer.addr_type)) {
        ui::showToastInfo(mstr(STR_CONNECTING));
    } else {
        ui::showToastError("Exchange failed");
    }
}

// ============================================================================
// Lock-screen quick action: show own vCard as a QR code.
// ============================================================================

/**
 * \brief Returns the localized label for the lock-screen quick action.
 */
static const char* getMyVcardLockscreenLabel() {
    return mstr(STR_MY_VCARD);
}

/**
 * \brief Lock-screen quick action: shows the own vCard as a QR code.
 *        Falls back to a toast when no vCard has been configured yet.
 */
static void onMyVcardLockscreenSelect() {
    static char s_qrBuf[VCARD_MAX_LEN + 1];
    size_t len = vcard_store_get_own(s_qrBuf, sizeof(s_qrBuf));
    if (len == 0) {
        ui::showToastError(mstr(STR_NO_VCARD));
        return;
    }

    static vcard_data_t s_parsed;
    static char s_qrTitle[96];
    static char s_qrSubtitle[96];

    memset(&s_parsed, 0, sizeof(s_parsed));
    vcard_parse_to_struct(s_qrBuf, &s_parsed);

    if (s_parsed.formatted_name[0]) {
        snprintf(s_qrTitle, sizeof(s_qrTitle), "%s", s_parsed.formatted_name);
    } else if (s_parsed.given_name[0] || s_parsed.family_name[0]) {
        snprintf(s_qrTitle, sizeof(s_qrTitle), "%s %s",
                 s_parsed.given_name, s_parsed.family_name);
    } else {
        snprintf(s_qrTitle, sizeof(s_qrTitle), "%s", mstr(STR_MY_VCARD));
    }

    const char* sub = s_parsed.organization[0] ? s_parsed.organization
                    : s_parsed.title[0]        ? s_parsed.title
                    : s_parsed.email[0]        ? s_parsed.email
                    : "";
    snprintf(s_qrSubtitle, sizeof(s_qrSubtitle), "%s", sub);

    ui::showQRCode(s_qrBuf, s_qrTitle, s_qrSubtitle[0] ? s_qrSubtitle : nullptr);
}

// ============================================================================
// Serial Commands (VCARD_SET / VCARD_GET / VCARD_DELETE)
// ============================================================================

static char s_vcardBuf[VCARD_MAX_LEN + 64];
static int  s_vcardBufPos = 0;
static bool s_vcardInputMode = false;

/**
 * \brief Intercepts multiline vCard paste input, accumulates lines, and stops at `"---"`.
 * \param line Incoming text line.
 * \return `true` to keep interception active, `false` to stop interception.
 */
static bool vcardLineInterceptor(const char* line) {
    if (!s_vcardInputMode) return false;

    using Console = serial::Console;

    if (strncmp(line, "---", 3) == 0) {
        s_vcardBuf[s_vcardBufPos] = '\0';

        char err[64] = {};
        if (vcard_store_set_own(s_vcardBuf, static_cast<size_t>(s_vcardBufPos), err, sizeof(err))) {
            Console::printf("OK: vCard updated\r\n");
        } else {
            Console::printf("ERROR: %s\r\n", err[0] ? err : "Invalid vCard");
        }

        s_vcardInputMode = false;
        s_vcardBufPos = 0;
        memset(s_vcardBuf, 0, sizeof(s_vcardBuf));
        serial::getCommandRegistry().setLineInterceptor(nullptr);
        Console::showPrompt();
        return true;
    }

    size_t lineLen = strlen(line);
    if (s_vcardBufPos + static_cast<int>(lineLen) + 2 < static_cast<int>(sizeof(s_vcardBuf))) {
        memcpy(s_vcardBuf + s_vcardBufPos, line, lineLen);
        s_vcardBufPos += static_cast<int>(lineLen);
        s_vcardBuf[s_vcardBufPos++] = '\n';
    } else {
        Console::printf("ERROR: vCard too large\r\n");
        s_vcardInputMode = false;
        s_vcardBufPos = 0;
        memset(s_vcardBuf, 0, sizeof(s_vcardBuf));
        serial::getCommandRegistry().setLineInterceptor(nullptr);
        Console::showPrompt();
    }
    return true;
}

/**
 * \brief Serial command entering multiline vCard paste mode.
 * \param args Unused command arguments.
 */
static void cmdVcardSet(const char* args) {
    (void)args;
    serial::Console::printf("Paste vCard 4.0, end with '---' on a new line:\r\n");
    s_vcardBufPos = 0;
    s_vcardInputMode = true;
    serial::getCommandRegistry().setLineInterceptor(vcardLineInterceptor);
}

/**
 * \brief Serial command printing stored vCard or template.
 * \param args Unused command arguments.
 */
static void cmdVcardGet(const char* args) {
    (void)args;
    using Console = serial::Console;

    char out[VCARD_MAX_LEN + 1];
    size_t len = vcard_store_get_own(out, sizeof(out));

    if (len == 0) {
        Console::printf("BEGIN:VCARD\r\n");
        Console::printf("VERSION:4.0\r\n");
        Console::printf("N:;;\r\n");
        Console::printf("FN:\r\n");
        Console::printf("NOTE:\r\n");
        Console::printf("TEL;TYPE=HOME:\r\n");
        Console::printf("TEL;TYPE=CELL:\r\n");
        Console::printf("TEL;TYPE=WORK:\r\n");
        Console::printf("EMAIL:\r\n");
        Console::printf("URL:\r\n");
        Console::printf("ORG:\r\n");
        Console::printf("TITLE:\r\n");
        Console::printf("X-SOCIALPROFILE:\r\n");
        Console::printf("IMPP:telegram:\r\n");
        Console::printf("IMPP:signal:\r\n");
        Console::printf("IMPP:matrix:\r\n");
        Console::printf("IMPP:threema:\r\n");
        Console::printf("END:VCARD\r\n");
        return;
    }

    char* line = out;
    char* next;
    while ((next = strchr(line, '\n')) != nullptr) {
        *next = '\0';
        Console::printf("%s\r\n", line);
        line = next + 1;
    }
    if (*line) {
        Console::printf("%s\r\n", line);
    }
}

/**
 * \brief Serial command deleting stored local vCard.
 * \param args Unused command arguments.
 */
static void cmdVcardDelete(const char* args) {
    (void)args;
    if (vcard_store_clear_own()) {
        serial::Console::printf("OK: vCard deleted\r\n");
    } else {
        serial::Console::printf("ERROR: Failed to delete vCard\r\n");
    }
}

/**
 * \brief Registers serial commands exposed by vCard module.
 */
static void registerSerialCommands() {
    auto& reg = serial::getCommandRegistry();
    reg.registerCommand({"VCARD_SET",    "Set own vCard (multiline paste)", cmdVcardSet,    "vcard", false});
    reg.registerCommand({"VCARD_GET",    "Show own vCard",                  cmdVcardGet,    "vcard", false});
    reg.registerCommand({"VCARD_DELETE", "Delete own vCard",                cmdVcardDelete, "vcard", false});
}

// ============================================================================
// Module Implementation
// ============================================================================

/**
 * \brief Returns singleton vCard module instance.
 * \return Module singleton reference.
 */
VcardModule& VcardModule::instance() {
    static VcardModule inst;
    return inst;
}

/**
 * \brief Initializes module UI strings, serial commands, and BLE service hooks.
 * \return `true` if initialization succeeded.
 */
bool VcardModule::init() {
    LOG_I(TAG, "Initializing vCard module");
    registerStrings();
    registerSerialCommands();

    VcardWizard::configure(mstr, s_wizardStepOffsets, STR_SAVED, STR_EXCHANGE_FAIL);

    if (!ble_vcard_init()) {
        LOG_W(TAG, "BLE vCard init failed (BLE might not be available)");
    }

    ble_vcard_set_consent_callback(onConsentRequest);
    ble_vcard_set_exchange_complete_callback(onExchangeComplete);

    ble_vcard_set_receive_enabled(true);

    core::ModuleRegistry::instance().registerModule(this);
    state_ = core::ServiceState::INITIALIZED;
    return true;
}

/**
 * \brief Starts vCard module service.
 * \return `true` if start transition succeeded.
 */
bool VcardModule::start() {
    if (state_ != core::ServiceState::INITIALIZED && state_ != core::ServiceState::STOPPED) {
        return false;
    }
    state_ = core::ServiceState::STARTED;
    return true;
}

/**
 * \brief Stops vCard BLE service and module runtime.
 */
void VcardModule::stop() {
    ble_vcard_deinit();
    state_ = core::ServiceState::STOPPED;
}

/**
 * \brief Provides tools-menu entry for vCard module.
 * \param items Output array for menu items.
 * \param maxItems Maximum writable entries.
 * \return Number of populated menu items.
 */
uint8_t VcardModule::getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;

    items[0] = {
        mstr(STR_VCARD),
        110,
        []() -> ui::IView* {
            if (!s_viewsInitialized) {
                s_mainMenu.setOnSelect(onMainMenuSelect);
                s_peerList.setOnSelect(onPeerSelect);
                s_viewsInitialized = true;
            }
            rebuildMainMenu();
            return &s_mainMenu;
        },
        nullptr,
        getName(),
        core::MenuLocation::TOOLS_MENU,
        nullptr
    };

    return 1;
}

/**
 * \brief Provides the lock-screen quick action that shows the owner vCard as a QR code.
 * \param items Output array for context items.
 * \param maxItems Maximum writable entries.
 * \return Number of populated entries.
 */
uint8_t VcardModule::getLockScreenContextItems(core::LockScreenContextItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;
    items[0] = {
        getMyVcardLockscreenLabel,
        onMyVcardLockscreenSelect,
        60,
        nullptr,
    };
    return 1;
}

/**
 * \brief Periodic vCard module tick forwarding BLE state machine.
 * \param nowMs Current uptime in milliseconds.
 */
void VcardModule::onTick(uint32_t nowMs) {
    ble_vcard_tick(nowMs);
}

} // namespace cdc::mod_vcard

/**
 * \brief Registers vCard module initializer in global module registry.
 */
extern "C" void mod_vcard_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& module = cdc::mod_vcard::VcardModule::instance();
        if (module.init()) {
            module.start();
        }
    });
}
