#include "mod_gpg/GpgModule.h"
#include "mod_gpg/GpgStorage.h"
#include "cdc_core/ModuleRegistry.h"
#include "cdc_core/UsbManager.h"
#include "cdc_ui/I18n.h"
#include "mod_gpg/openpgp/ccid.h"
#include "mod_gpg/openpgp/openpgp.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_views/ListView.h"
#include "cdc_views/T9InputView.h"
#include "cdc_views/InfoView.h"
#include "cdc_views/QRCodeView.h"
#include "cdc_views/ConfirmView.h"
#include "cdc_views/ToastView.h"
#include "cdc_os_ui/views/PinChangeView.h"
#include "cdc_core/PinManager.h"
#include "cdc_core/pin_storage_c.h"
#include "serial_cmd/ICommandRegistry.h"
#include "serial_cmd/Console.h"
#include "mod_gpg/gpg.h"
#include "cdc_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_attr.h"
#include <new>
#include <cstring>
#include <cstdio>
#include <cctype>

static const char* TAG = "GPG";

namespace cdc::mod_gpg {

static uint16_t s_strIdBase = 0;
static constexpr uint16_t STR_GPG = 0;
static constexpr uint16_t STR_STATUS = 1;
static constexpr uint16_t STR_GENERATE = 2;
static constexpr uint16_t STR_EXPORT = 3;
static constexpr uint16_t STR_RESET = 4;
static constexpr uint16_t STR_SETTINGS = 5;
static constexpr uint16_t STR_USER_PIN = 6;
static constexpr uint16_t STR_ADMIN_PIN = 7;
static constexpr uint16_t STR_SLOT_ERROR = 8;
static constexpr uint16_t STR_NAME = 9;
static constexpr uint16_t STR_EMAIL = 10;
static constexpr uint16_t STR_CURVE = 11;
static constexpr uint16_t STR_CURVE_ED25519 = 12;
static constexpr uint16_t STR_CURVE_P256 = 13;
static constexpr uint16_t STR_NO_KEY = 14;
static constexpr uint16_t STR_CONFIRM_RESET = 15;
static constexpr uint16_t STR_EXPORT_TITLE = 16;
static constexpr uint16_t STR_COUNT = 17;

/**
 * \brief Resolves module-localized string by offset.
 * \param offset Module string-table offset.
 * \return Translated string pointer.
 */
static const char* mstr(uint16_t offset) {
    return ui::tr(s_strIdBase + offset);
}

/**
 * \brief Registers GPG module translations.
 */
static void registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("mod_gpg", STR_COUNT);
    if (s_strIdBase == 0) {
        LOG_E(TAG, "Failed to register i18n strings");
        return;
    }

    i18n.registerTranslation(s_strIdBase + STR_GPG, ui::Language::EN, "GPG");
    i18n.registerTranslation(s_strIdBase + STR_STATUS, ui::Language::EN, "Status");
    i18n.registerTranslation(s_strIdBase + STR_GENERATE, ui::Language::EN, "Generate Keys");
    i18n.registerTranslation(s_strIdBase + STR_EXPORT, ui::Language::EN, "Export Public");
    i18n.registerTranslation(s_strIdBase + STR_RESET, ui::Language::EN, "Reset");
    i18n.registerTranslation(s_strIdBase + STR_SETTINGS, ui::Language::EN, "Settings");
    i18n.registerTranslation(s_strIdBase + STR_USER_PIN, ui::Language::EN, "User PIN");
    i18n.registerTranslation(s_strIdBase + STR_ADMIN_PIN, ui::Language::EN, "Admin PIN");
    i18n.registerTranslation(s_strIdBase + STR_SLOT_ERROR, ui::Language::EN, "Slot map error");
    i18n.registerTranslation(s_strIdBase + STR_NAME, ui::Language::EN, "Name");
    i18n.registerTranslation(s_strIdBase + STR_EMAIL, ui::Language::EN, "Email (optional)");
    i18n.registerTranslation(s_strIdBase + STR_CURVE, ui::Language::EN, "Curve");
    i18n.registerTranslation(s_strIdBase + STR_CURVE_ED25519, ui::Language::EN, "Ed25519");
    i18n.registerTranslation(s_strIdBase + STR_CURVE_P256, ui::Language::EN, "P-256");
    i18n.registerTranslation(s_strIdBase + STR_NO_KEY, ui::Language::EN, "No key configured");
    i18n.registerTranslation(s_strIdBase + STR_CONFIRM_RESET, ui::Language::EN, "Reset all GPG keys?");
    i18n.registerTranslation(s_strIdBase + STR_EXPORT_TITLE, ui::Language::EN, "GPG Public Key");

    i18n.registerTranslation(s_strIdBase + STR_GPG, ui::Language::DE, "GPG");
    i18n.registerTranslation(s_strIdBase + STR_STATUS, ui::Language::DE, "Status");
    i18n.registerTranslation(s_strIdBase + STR_GENERATE, ui::Language::DE, "Keys erzeugen");
    i18n.registerTranslation(s_strIdBase + STR_EXPORT, ui::Language::DE, "Public exportieren");
    i18n.registerTranslation(s_strIdBase + STR_RESET, ui::Language::DE, "Zuruecksetzen");
    i18n.registerTranslation(s_strIdBase + STR_SETTINGS, ui::Language::DE, "Einstellungen");
    i18n.registerTranslation(s_strIdBase + STR_USER_PIN, ui::Language::DE, "User PIN");
    i18n.registerTranslation(s_strIdBase + STR_ADMIN_PIN, ui::Language::DE, "Admin PIN");
    i18n.registerTranslation(s_strIdBase + STR_SLOT_ERROR, ui::Language::DE, "Slot-Map Fehler");
    i18n.registerTranslation(s_strIdBase + STR_NAME, ui::Language::DE, "Name");
    i18n.registerTranslation(s_strIdBase + STR_EMAIL, ui::Language::DE, "Email (optional)");
    i18n.registerTranslation(s_strIdBase + STR_CURVE, ui::Language::DE, "Kurve");
    i18n.registerTranslation(s_strIdBase + STR_CURVE_ED25519, ui::Language::DE, "Ed25519");
    i18n.registerTranslation(s_strIdBase + STR_CURVE_P256, ui::Language::DE, "P-256");
    i18n.registerTranslation(s_strIdBase + STR_NO_KEY, ui::Language::DE, "Kein Key konfiguriert");
    i18n.registerTranslation(s_strIdBase + STR_CONFIRM_RESET, ui::Language::DE, "Alle GPG Keys loeschen?");
    i18n.registerTranslation(s_strIdBase + STR_EXPORT_TITLE, ui::Language::DE, "GPG Public Key");
}

static constexpr const char* CMD_MODULE = "gpg";
static bool s_commandsRegistered = false;

static void cmd_gpg_status(const char* args);
static void cmd_gpg_generate(const char* args);
static void cmd_gpg_export(const char* args);
static void cmd_gpg_reset(const char* args);

/**
 * \brief Registers serial commands exposed by GPG module.
 */
static void registerCommands() {
    if (s_commandsRegistered) return;
    s_commandsRegistered = true;
    auto& registry = cdc::serial::getCommandRegistry();
    registry.registerCommand({"GPG_STATUS", "Show GPG status", cmd_gpg_status, CMD_MODULE, true});
    registry.registerCommand({"GPG_GENERATE", "Generate GPG keys", cmd_gpg_generate, CMD_MODULE, true});
    registry.registerCommand({"GPG_EXPORT", "Export public keys", cmd_gpg_export, CMD_MODULE, true});
    registry.registerCommand({"GPG_RESET", "Reset GPG keys", cmd_gpg_reset, CMD_MODULE, true});
}

/**
 * \brief Serial command printing current GPG key status.
 * \param args Unused command arguments.
 */
static void cmd_gpg_status(const char* args) {
    (void)args;
    gpg_status_t status = {};
    if (!gpg_get_status(&status)) {
        cdc::serial::Console::printf("ERROR: No key configured\r\n");
        return;
    }
    cdc::serial::Console::printf("User-ID: %s\r\n", status.user_id);
    cdc::serial::Console::printf("Curve: %s\r\n",
                                 status.curve == CDC_CURVE_ED25519 ? "Ed25519" : "P-256");
    cdc::serial::Console::printf("Created: %lu\r\n", static_cast<unsigned long>(status.created_at));
    cdc::serial::Console::printf("Sign Count: %lu\r\n", static_cast<unsigned long>(status.sign_count));
}

/**
 * \brief Serial command generating GPG key with selected curve and user-id.
 * \param args Command arguments (`<curve> <user_id>`).
 */
static void cmd_gpg_generate(const char* args) {
    char curveBuf[8] = {};
    char userId[GPG_USER_ID_MAX] = {};

    const char* p = args;
    while (p && *p && std::isspace(static_cast<unsigned char>(*p))) p++;
    if (!p || !*p) {
        cdc::serial::Console::printf("Usage: GPG_GENERATE <curve> <user_id>\r\n");
        return;
    }

    size_t i = 0;
    while (p[i] && !std::isspace(static_cast<unsigned char>(p[i])) && i + 1 < sizeof(curveBuf)) {
        curveBuf[i] = p[i];
        i++;
    }
    curveBuf[i] = '\0';
    p += i;
    while (p && *p && std::isspace(static_cast<unsigned char>(*p))) p++;
    if (!p || !*p) {
        cdc::serial::Console::printf("Usage: GPG_GENERATE <curve> <user_id>\r\n");
        return;
    }
    strncpy(userId, p, sizeof(userId) - 1);

    uint8_t curve = (atoi(curveBuf) == 2) ? CDC_CURVE_P256 : CDC_CURVE_ED25519;
    gpg_set_pending_user_id(userId);
    bool ok = gpg_generate_key(curve);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}

/**
 * \brief Serial command exporting GPG public key in PEM format.
 * \param args Unused command arguments.
 */
static void cmd_gpg_export(const char* args) {
    (void)args;
    char pem_buf[2048];
    size_t out_len = 0;
    if (!gpg_export_pubkey_pem(pem_buf, sizeof(pem_buf), &out_len)) {
        cdc::serial::Console::printf("ERROR\r\n");
        return;
    }
    cdc::serial::Console::printf("%s\r\n", pem_buf);
}

/**
 * \brief Serial command resetting GPG key material.
 * \param args Unused command arguments.
 */
static char s_reset_token[7] = {};
static uint64_t s_reset_token_ts_us = 0;
static constexpr uint64_t RESET_TOKEN_TIMEOUT_US = 30ULL * 1000ULL * 1000ULL;

static void cmd_gpg_reset(const char* args) {
    const uint64_t now = static_cast<uint64_t>(esp_timer_get_time());
    const bool token_active = (s_reset_token[0] != '\0') &&
                              ((now - s_reset_token_ts_us) < RESET_TOKEN_TIMEOUT_US);

    if (args && *args && token_active && strcmp(args, s_reset_token) == 0) {
        memset(s_reset_token, 0, sizeof(s_reset_token));
        s_reset_token_ts_us = 0;
        bool ok = gpg_reset();
        cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
        return;
    }

    uint8_t r[3];
    esp_fill_random(r, sizeof(r));
    snprintf(s_reset_token, sizeof(s_reset_token), "%02X%02X%02X", r[0], r[1], r[2]);
    s_reset_token_ts_us = now;
    cdc::serial::Console::printf(
        "WARNING: this wipes ALL GPG keys (SIG/DEC/AUT), the DEC backup, and PINs.\r\n"
        "Confirm within 30s: GPG_RESET %s\r\n",
        s_reset_token);
}

static ui::ListView s_menuView;
static ui::ListView s_settingsView;
static ui::PinChangeView s_pinChangeView;
static ui::T9InputView s_t9Input;
static ui::ListView s_curveView;
static ui::InfoView s_infoView;
static ui::QRCodeView s_qrView;
static bool s_viewsInitialized = false;
static ui::ListItem s_menuItems[] = {
    { nullptr, 0, false, nullptr },
    { nullptr, 0, false, nullptr },
    { nullptr, 0, false, nullptr },
    { nullptr, 0, false, nullptr },
    { nullptr, 0, false, nullptr },
};
static ui::ListItem s_settingsItems[] = {
    { nullptr, 0, false, nullptr },
    { nullptr, 0, false, nullptr },
};

struct WizardState {
    char name[64];
    char email[64];
    uint8_t curve;
};

static WizardState s_wizard = {};

static void showStatus();
static void wizardStart();
static void showExport();
static void confirmReset();
static void showSettings();
static void onSettingsSelect(uint16_t index, void*);

/**
 * \brief Handles GPG main-menu selections.
 * \param index Selected menu index.
 * \param userData Optional callback context (unused).
 */
static void onMenuSelect(uint16_t index, void*) {
    // Menu shape depends on whether keys exist (see rebuildMenu): when keys are
    // already present, the "Generate" entry is hidden so subsequent indices
    // shift up by one.
    const bool hasKeys = openpgp_has_any_key();
    if (index == 0) { showStatus(); return; }
    if (!hasKeys && index == 1) { wizardStart(); return; }
    const uint16_t shift = hasKeys ? 0 : 1;
    switch (index - shift) {
        case 1: showExport(); break;
        case 2: showSettings(); break;
        case 3: confirmReset(); break;
        default: break;
    }
}

/**
 * \brief Rebuilds GPG main menu labels.
 */
static void rebuildMenu() {
    s_menuItems[0].label = mstr(STR_STATUS);
    uint8_t count = 1;
    if (!openpgp_has_any_key()) {
        s_menuItems[count++].label = mstr(STR_GENERATE);
    }
    s_menuItems[count++].label = mstr(STR_EXPORT);
    s_menuItems[count++].label = mstr(STR_SETTINGS);
    s_menuItems[count++].label = mstr(STR_RESET);
    s_menuView.init(mstr(STR_GPG), s_menuItems, count);
}

/**
 * \brief Verifies OpenPGP PW1 using persistent pin-storage backend.
 * \param pin Candidate PW1 value.
 * \return `true` when valid.
 */
static bool gpg_verify_pw1(const char* pin) {
    return pin_storage_openpgp_verify_pw1(pin);
}

/**
 * \brief Verifies OpenPGP PW3 using persistent pin-storage backend.
 * \param pin Candidate PW3 value.
 * \return `true` when valid.
 */
static bool gpg_verify_pw3(const char* pin) {
    return pin_storage_openpgp_verify_pw3(pin);
}

/**
 * \brief Changes OpenPGP PW1 value.
 * \param oldPin Ignored old PIN parameter from generic callback signature.
 * \param newPin New PW1 value.
 * \return `true` on success.
 */
static bool gpg_change_pw1(const char*, const char* newPin) {
    return pin_storage_openpgp_change_pw1(newPin);
}

/**
 * \brief Changes OpenPGP PW3 value.
 * \param oldPin Ignored old PIN parameter from generic callback signature.
 * \param newPin New PW3 value.
 * \return `true` on success.
 */
static bool gpg_change_pw3(const char*, const char* newPin) {
    return pin_storage_openpgp_change_pw3(newPin);
}

/**
 * \brief Returns remaining retries for OpenPGP PW1.
 * \return Retry counter.
 */
static uint8_t gpg_retries_pw1() {
    return pin_storage_openpgp_pw1_retries();
}

/**
 * \brief Returns remaining retries for OpenPGP PW3.
 * \return Retry counter.
 */
static uint8_t gpg_retries_pw3() {
    return pin_storage_openpgp_pw3_retries();
}

/**
 * \brief Returns whether OpenPGP PW1 is blocked.
 * \return `true` when blocked.
 */
static bool gpg_blocked_pw1() {
    return pin_storage_openpgp_pw1_blocked();
}

/**
 * \brief Returns whether OpenPGP PW3 is blocked.
 * \return `true` when blocked.
 */
static bool gpg_blocked_pw3() {
    return pin_storage_openpgp_pw3_blocked();
}

/**
 * \brief Pin-change completion callback returning to previous view.
 * \param changed Result flag (unused).
 */
static void onGpgPinComplete(bool) {
    ui::ViewStack::instance().pop();
}

/**
 * \brief Handles settings-menu selection for PW1/PW3 change flow.
 * \param index Selected settings row.
 * \param userData Optional callback context (unused).
 */
static void onSettingsSelect(uint16_t index, void*) {
    s_pinChangeView.setOnComplete(onGpgPinComplete);
    s_pinChangeView.setTitle(index == 0 ? mstr(STR_USER_PIN) : mstr(STR_ADMIN_PIN));
    if (index == 0) {
        s_pinChangeView.setVerifyCallback(gpg_verify_pw1);
        s_pinChangeView.setChangeCallback(gpg_change_pw1);
        s_pinChangeView.setRetriesCallback(gpg_retries_pw1);
        s_pinChangeView.setBlockedCallback(gpg_blocked_pw1);
        s_pinChangeView.init(cdc::core::PinManager::PW1_MIN, cdc::core::PinManager::PIN_MAX);
    } else {
        s_pinChangeView.setVerifyCallback(gpg_verify_pw3);
        s_pinChangeView.setChangeCallback(gpg_change_pw3);
        s_pinChangeView.setRetriesCallback(gpg_retries_pw3);
        s_pinChangeView.setBlockedCallback(gpg_blocked_pw3);
        s_pinChangeView.init(cdc::core::PinManager::PW3_MIN, cdc::core::PinManager::PIN_MAX);
    }
    ui::ViewStack::instance().push(&s_pinChangeView);
}

/**
 * \brief Shows GPG settings menu.
 */
static void showSettings() {
    s_settingsItems[0].label = mstr(STR_USER_PIN);
    s_settingsItems[1].label = mstr(STR_ADMIN_PIN);
    s_settingsView.init(mstr(STR_SETTINGS), s_settingsItems, 2);
    s_settingsView.setOnSelect(onSettingsSelect);
    ui::ViewStack::instance().push(&s_settingsView);
}

/**
 * \brief Displays current GPG key status and metadata.
 */
static void showStatus() {
    gpg_status_t status = {};
    if (!gpg_get_status(&status)) {
        s_infoView.init(mstr(STR_STATUS), mstr(STR_NO_KEY));
        ui::ViewStack::instance().push(&s_infoView);
        return;
    }

    char fp_hex[GPG_FINGERPRINT_LEN * 2 + 1] = {};
    for (size_t i = 0; i < GPG_FINGERPRINT_LEN; i++) {
        snprintf(fp_hex + i * 2, 3, "%02X", status.fingerprint[i]);
    }
    const char* curveName = status.curve == CDC_CURVE_ED25519 ? mstr(STR_CURVE_ED25519)
                                                             : mstr(STR_CURVE_P256);
    static char detail[512];
    snprintf(detail, sizeof(detail),
             "User-ID: %s\nCurve: %s\nFP: %s\nCreated: %lu\nSign Count: %lu",
             status.user_id, curveName, fp_hex,
             static_cast<unsigned long>(status.created_at),
             static_cast<unsigned long>(status.sign_count));
    s_infoView.init(mstr(STR_STATUS), detail);
    ui::ViewStack::instance().push(&s_infoView);
}

static void onWizardName(const char* text);
static void onWizardEmail(const char* text);
static void onWizardCurve(uint16_t index, void*);

/**
 * \brief Starts key-generation wizard flow.
 */
static void wizardStart() {
    memset(&s_wizard, 0, sizeof(s_wizard));
    s_t9Input.init(mstr(STR_NAME), nullptr, 63);
    s_t9Input.setOnSave(onWizardName);
    ui::ViewStack::instance().push(&s_t9Input);
}

/**
 * \brief Saves wizard name and opens email step.
 * \param text Entered name.
 */
static void onWizardName(const char* text) {
    strncpy(s_wizard.name, text ? text : "", sizeof(s_wizard.name) - 1);
    s_t9Input.init(mstr(STR_EMAIL), nullptr, 63);
    s_t9Input.setOnSave(onWizardEmail);
    ui::ViewStack::instance().push(&s_t9Input);
}

/**
 * \brief Saves wizard email and opens curve selection.
 * \param text Entered email.
 */
static void onWizardEmail(const char* text) {
    strncpy(s_wizard.email, text ? text : "", sizeof(s_wizard.email) - 1);
    static ui::ListItem curveItems[] = {
        { nullptr, 0, false, nullptr },
        { nullptr, 0, false, nullptr },
    };
    curveItems[0].label = mstr(STR_CURVE_ED25519);
    curveItems[1].label = mstr(STR_CURVE_P256);
    s_curveView.init(mstr(STR_CURVE), curveItems, 2);
    s_curveView.setOnSelect(onWizardCurve);
    ui::ViewStack::instance().push(&s_curveView);
}

/**
 * \brief Finalizes wizard curve selection and triggers key generation.
 * \param index Selected curve index.
 * \param userData Optional callback context (unused).
 */
static void onWizardCurve(uint16_t index, void*) {
    s_wizard.curve = (index == 0) ? CDC_CURVE_ED25519 : CDC_CURVE_P256;
    char user_id[GPG_USER_ID_MAX] = {};
    size_t name_len = strnlen(s_wizard.name, sizeof(s_wizard.name) - 1);
    size_t email_len = strnlen(s_wizard.email, sizeof(s_wizard.email) - 1);
    if (email_len == 0) {
        snprintf(user_id, sizeof(user_id), "%.*s", static_cast<int>(sizeof(user_id) - 1), s_wizard.name);
    } else {
        size_t max_len = sizeof(user_id) - 1;
        size_t name_fit = name_len > max_len ? max_len : name_len;
        size_t email_fit = 0;
        if (name_fit < max_len) {
            size_t remaining = max_len - name_fit;
            if (remaining > 3) {
                email_fit = remaining - 3;
            }
        }
        if (email_fit == 0) {
            memcpy(user_id, s_wizard.name, name_fit);
            user_id[name_fit] = '\0';
        } else {
            size_t pos = 0;
            memcpy(user_id + pos, s_wizard.name, name_fit);
            pos += name_fit;
            user_id[pos++] = ' ';
            user_id[pos++] = '<';
            memcpy(user_id + pos, s_wizard.email, email_fit);
            pos += email_fit;
            user_id[pos++] = '>';
            user_id[pos] = '\0';
        }
    }
    gpg_set_pending_user_id(user_id);
    if (gpg_generate_key(s_wizard.curve)) {
        ui::showToastSuccess(ui::tr(ui::StringId::OK));
    } else {
        ui::showToastError(ui::tr(ui::StringId::FAILED));
    }
    while (ui::ViewStack::instance().depth() > 1) {
        ui::ViewStack::instance().pop();
    }
}

/**
 * \brief Exports public key to serial output and QR view.
 */
static void showExport() {
    EXT_RAM_BSS_ATTR static char pem_buf[2048];
    static char detail_buf[96];
    size_t out_len = 0;
    if (!gpg_export_pubkey_pem(pem_buf, sizeof(pem_buf), &out_len)) {
        ui::showToastError(ui::tr(ui::StringId::FAILED));
        return;
    }
    cdc::serial::Console::printf("%s\r\n", pem_buf);

    gpg_status_t status = {};
    detail_buf[0] = '\0';
    if (gpg_get_status(&status) && status.initialized) {
        const char* uid = status.user_id[0] ? status.user_id : "(card)";
        const char* curveName = (status.curve == CDC_CURVE_ED25519) ? "Ed25519" : "P-256";
        char fp_tail[12] = {0};
        for (int i = 0; i < 4; i++) {
            snprintf(fp_tail + i * 2, sizeof(fp_tail) - i * 2,
                     "%02X", status.fingerprint[GPG_FINGERPRINT_LEN - 4 + i]);
        }
        snprintf(detail_buf, sizeof(detail_buf),
                 "%s\n%s\nFP ..%s",
                 uid, curveName, fp_tail);
    }

    s_qrView.init(pem_buf, mstr(STR_EXPORT_TITLE),
                  detail_buf[0] ? detail_buf : nullptr);
    ui::ViewStack::instance().push(&s_qrView);
}

/**
 * \brief Confirm callback resetting all GPG key material.
 * \param userData Optional callback context (unused).
 */
static void onResetConfirm(void*) {
    if (gpg_reset()) {
        ui::showToastSuccess(ui::tr(ui::StringId::OK));
    } else {
        ui::showToastError(ui::tr(ui::StringId::FAILED));
    }
}

/**
 * \brief Opens reset confirmation dialog.
 */
static void confirmReset() {
    ui::showConfirm(mstr(STR_CONFIRM_RESET), onResetConfirm, nullptr,
                    ui::ConfirmView::Icon::WARNING, nullptr);
}

/**
 * \brief Returns singleton GPG module instance.
 * \return Module singleton reference.
 */
GpgModule& GpgModule::instance() {
    static GpgModule inst;
    return inst;
}

/**
 * \brief Initializes GPG module resources and slot assignments.
 * \return `true` if initialization succeeded.
 */
bool GpgModule::init() {
    LOG_I(TAG, "Initializing GPG module");
    registerStrings();
    registerCommands();

    core::ModuleRegistry::instance().registerModule(this);
    if (!slotRange_.hasEcc) {
        core::ModuleRegistry::instance().reportModuleError(getName(), "GPG slot range missing");
        state_ = core::ServiceState::ERROR;
        return false;
    }
    gpg_storage_set_slot_range(slotRange_.eccStart, slotRange_.eccEnd);
    if (slotRange_.hasRmem) {
        gpg_storage_set_rmem_range(slotRange_.rmemStart, slotRange_.rmemEnd);
    }
    if (!gpg_storage_ready()) {
        core::ModuleRegistry::instance().reportModuleError(getName(), "GPG slot range invalid");
        state_ = core::ServiceState::ERROR;
        return false;
    }
    core::ModuleRegistry::instance().clearModuleErrorByName(getName());
    state_ = core::ServiceState::INITIALIZED;
    return true;
}

/**
 * \brief Starts GPG module and registers USB CCID interface.
 * \return `true` if start transition succeeded.
 */
bool GpgModule::start() {
    if (state_ != core::ServiceState::INITIALIZED &&
        state_ != core::ServiceState::STOPPED) {
        return false;
    }

    core::UsbInterfaceSpec spec = {};
    spec.cls = core::UsbInterfaceClass::Ccid;
    spec.name = "OpenPGP SmartCard";
    spec.epInSize = 64;
    spec.epOutSize = 64;
    // Call ccid_init() rather than openpgp_init() directly: it brings up
    // OpenPGP and is the only external reference into ccid.cpp / ccid_driver.cpp.
    // Without it the linker drops the entire CCID translation unit (including
    // our strong usbd_app_driver_get_cb override), leaving tinyusb's weak
    // default in place and the smart-card interface unenumerated.
    if (!ccid_init()) {
        core::ModuleRegistry::instance().reportModuleError(getName(), "CCID init failed");
    }
    if (!core::UsbManager::instance().registerInterface(core::UsbHidInterface::Ccid, getName(), spec)) {
        LOG_W(TAG, "Failed to register CCID interface");
    }

    state_ = core::ServiceState::STARTED;
    return true;
}

/**
 * \brief Stops GPG module and unregisters CCID interface.
 */
void GpgModule::stop() {
    core::UsbManager::instance().unregisterInterface(core::UsbHidInterface::Ccid, getName());
    state_ = core::ServiceState::STOPPED;
}

/**
 * \brief Stores slot range assigned by module registry.
 * \param range Slot assignment.
 */
void GpgModule::setSlotRange(const core::IModule::SlotRange& range) {
    slotRange_ = range;
}

/**
 * \brief Declares slot requirements for GPG module.
 * \return Slot request descriptor.
 */
core::IModule::SlotRequest GpgModule::getSlotRequest() const {
    core::IModule::SlotRequest req = {};
    req.mapName = getName();
    req.minEccSlots = 3;
    req.minRmemSlots = 1;
    return req;
}

/**
 * \brief Provides main-menu entry for GPG module.
 * \param items Output menu item array.
 * \param maxItems Maximum writable entries.
 * \return Number of populated menu items.
 */
uint8_t GpgModule::getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;
    items[0] = {mstr(STR_GPG), 60, []() -> ui::IView* {
        if (!s_viewsInitialized) {
            s_menuView.setOnSelect(onMenuSelect);
            s_viewsInitialized = true;
        }
        if (!GpgModule::instance().slotRange_.hasEcc || !GpgModule::instance().slotRange_.hasRmem) {
            ui::showToastError(mstr(STR_SLOT_ERROR));
            return nullptr;
        }
        rebuildMenu();
        return &s_menuView;
    }, nullptr, getName(), core::MenuLocation::MAIN_MENU, nullptr};
    return 1;
}

} // namespace cdc::mod_gpg

/**
 * \brief Registers GPG module initializer in global registry.
 */
extern "C" void mod_gpg_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& module = cdc::mod_gpg::GpgModule::instance();
        if (module.init()) {
            module.start();
        }
    });
}
