/**
 * I18n Implementation
 *
 * Modular internationalization with:
 * - Core system strings
 * - Module registration for dynamic strings
 * - English fallback
 * - NVS persistence for language selection
 */

#include "cdc_ui/I18n.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cdc_log.h"
#include <cstring>

static const char* TAG = "I18n";

namespace cdc::ui {

// NVS namespace and key
static constexpr const char* NVS_NAMESPACE = "i18n";
static constexpr const char* NVS_KEY_LANG = "lang";

I18n& I18n::instance() {
    static I18n instance;
    return instance;
}

bool I18n::init() {
    // Initialize core strings
    initCoreStrings();

    // Load language from NVS
    loadFromNvs();

    LOG_I(TAG, "I18n initialized, lang=%s, strings=%d",
             getLanguageName(currentLang_), nextModuleId_);
    return true;
}

void I18n::setLanguage(Language lang) {
    if (lang >= Language::COUNT) {
        lang = Language::EN;
    }
    if (currentLang_ != lang) {
        currentLang_ = lang;
        saveToNvs();
        LOG_I(TAG, "Language changed to %s", getLanguageName(lang));
    }
}

const char* I18n::getLanguageName(Language lang) const {
    switch (lang) {
        case Language::EN: return "English";
        case Language::DE: return "Deutsch";
        default: return "?";
    }
}

const char* I18n::str(StringId id) const {
    return str(static_cast<uint16_t>(id));
}

const char* I18n::str(uint16_t id) const {
    if (id >= MAX_STRINGS) {
        return "?";
    }

    // Try current language first
    const char* text = strings_[static_cast<uint8_t>(currentLang_)][id];
    if (text) {
        return text;
    }

    // Fallback to English
    text = strings_[static_cast<uint8_t>(Language::EN)][id];
    if (text) {
        return text;
    }

    // No translation found
    return "?";
}

uint16_t I18n::registerModule(const char* moduleName, uint16_t count) {
    if (nextModuleId_ + count > MAX_STRINGS) {
        LOG_E(TAG, "Cannot register module '%s': out of string slots", moduleName);
        return 0;
    }

    uint16_t baseId = nextModuleId_;
    nextModuleId_ += count;

    LOG_I(TAG, "Module '%s' registered IDs %d-%d", moduleName, baseId, baseId + count - 1);
    return baseId;
}

bool I18n::registerTranslation(uint16_t stringId, Language lang, const char* text) {
    if (stringId >= MAX_STRINGS || lang >= Language::COUNT || !text) {
        return false;
    }

    strings_[static_cast<uint8_t>(lang)][stringId] = text;
    return true;
}

void I18n::registerTranslations(const Translation* translations) {
    if (!translations) return;

    while (translations->stringId != 0xFFFF) {
        registerTranslation(translations->stringId, translations->lang, translations->text);
        translations++;
    }
}

void I18n::loadFromNvs() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        uint8_t lang = 0;
        if (nvs_get_u8(handle, NVS_KEY_LANG, &lang) == ESP_OK) {
            if (lang < static_cast<uint8_t>(Language::COUNT)) {
                currentLang_ = static_cast<Language>(lang);
            }
        }
        nvs_close(handle);
    }
}

void I18n::saveToNvs() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_u8(handle, NVS_KEY_LANG, static_cast<uint8_t>(currentLang_));
        nvs_commit(handle);
        nvs_close(handle);
    }
}

void I18n::initCoreStrings() {
    // Macro for cleaner registration
    #define REG(id, en, de) \
        strings_[0][static_cast<uint16_t>(StringId::id)] = en; \
        strings_[1][static_cast<uint16_t>(StringId::id)] = de

    // === System ===
    REG(MAIN_MENU,      "Main Menu",        "Hauptmenu");
    REG(SETTINGS,       "Settings",         "Einstellungen");
    REG(HARDWARE,       "Hardware",         "Hardware");
    REG(TOOLS,          "Tools",            "Werkzeuge");
    REG(HARDWARE_INFO,  "Hardware Info",    "Hardware-Info");
    REG(NAME,           "Name",             "Name");
    REG(INFO,           "Info",             "Info");
    REG(INFO2,          "Info 2",            "Info 2");
    REG(DEFAULT_NAME,   "CDC Badge",        "CDC Badge");
    REG(DEFAULT_INFO,   "v0.5 Modular",     "v0.5 Modular");
    REG(BACK,           "Back",             "Zuruck");
    REG(OK,             "OK",               "OK");
    REG(CANCEL,         "Cancel",           "Abbrechen");
    REG(SAVE,           "Save",             "Speichern");
    REG(DELETE,         "Delete",           "Loschen");
    REG(EDIT,           "Edit",             "Bearbeiten");
    REG(VIEW,           "View",             "Anzeigen");
    REG(SELECT,         "Select",           "Auswahlen");
    REG(YES,            "Yes",              "Ja");
    REG(NO,             "No",               "Nein");
    REG(ON,             "On",               "Ein");
    REG(OFF,            "Off",              "Aus");
    REG(SAVED,          "Saved",            "Gespeichert");
    REG(DELETED,        "Deleted",          "Geloscht");
    REG(FAILED,         "Failed",           "Fehlgeschlagen");
    REG(TIMEOUT,        "Timeout",          "Zeituberschreitung");
    REG(EMPTY,          "empty",            "leer");

    // === Lock Screen ===
    REG(LOCK,               "Lock",                 "Sperren");
    REG(UNLOCK,             "Unlock",               "Entsperren");
    REG(ENTER_PIN,          "Enter PIN",            "PIN eingeben");
    REG(PRESS_ANY_KEY,      "Press any key",        "Beliebige Taste drucken");
    REG(WRONG_PIN,          "Wrong PIN",            "Falsche PIN");
    REG(LOCKED_OUT,         "Locked out",           "Gesperrt");
    REG(TOO_MANY_ATTEMPTS,  "Too many attempts",    "Zu viele Versuche");

    // === PIN ===
    REG(CHANGE_PIN,         "Change PIN",           "PIN andern");
    REG(CURRENT_PIN,        "Current PIN",          "Aktuelle PIN");
    REG(NEW_PIN,            "New PIN",              "Neue PIN");
    REG(CONFIRM_PIN,        "Confirm PIN",          "PIN bestatigen");
    REG(PIN_CHANGED,        "PIN changed",          "PIN geandert");
    REG(PINS_DONT_MATCH,    "PINs don't match",     "PINs stimmen nicht");
    REG(PIN_TOO_SHORT,      "PIN too short",        "PIN zu kurz");
    REG(PIN_MISMATCH,       "PINs don't match",     "PINs stimmen nicht");
    REG(RETRIES,            "Retries",              "Versuche");
    REG(ERROR_GENERIC,      "Error",                "Fehler");

    // === Settings ===
    REG(BRIGHTNESS,     "Brightness",       "Helligkeit");
    REG(LANGUAGE,       "Language",         "Sprache");
    REG(TIMEZONE,       "Timezone",         "Zeitzone");
    REG(SUMMER_TIME,    "Daylight Saving",  "Sommerzeit");
    REG(BADGE_TEXT,     "Badge Text",       "Badge-Text");
    REG(AUTO_SLEEP,     "Sleep Interval",   "Schlafintervall");
    REG(SET_DATE,       "Set Date",         "Datum einstellen");
    REG(SET_TIME,       "Set Time",         "Uhrzeit einstellen");
    REG(DATE,           "Date",             "Datum");
    REG(TIME,           "Time",             "Uhrzeit");
    REG(DATE_SAVED,     "Date saved",       "Datum gespeichert");
    REG(TIME_SAVED,     "Time saved",       "Uhrzeit gespeichert");
    REG(MODULES,        "Modules",          "Module");
    REG(NEVER,          "Never",            "Nie");
    REG(MINUTES,        "min",              "min");

    // === Hardware ===
    REG(WIFI_MENU,          "WiFi",                 "WLAN");
    REG(WIFI_SETUP,         "WiFi Setup",           "WLAN Setup");
    REG(WIFI_CONNECT,       "Connect",              "Verbinden");
    REG(WIFI_DETAILS,       "Details",              "Details");
    REG(WIFI_DISCONNECT,    "Disconnect",           "Trennen");
    REG(WIFI_SCANNING,      "Scanning...",          "Suche...");
    REG(WIFI_NO_NETWORKS,   "No networks",          "Keine Netzwerke");
    REG(WIFI_CONNECTING,    "Connecting...",        "Verbinde...");
    REG(WIFI_CONNECTED,     "Connected!",           "Verbunden!");
    REG(WIFI_DISCONNECTED,  "Disconnected",         "Getrennt");
    REG(WIFI_FAILED,        "Connection failed",    "Verbindung fehlgeschlagen");
    REG(WIFI_NO_CONFIG,     "No WiFi configured",   "Kein WLAN konfiguriert");
    REG(WIFI_PASSWORD,      "Password",             "Passwort");
    REG(WIFI_ADD_MANUAL,    "Add Manual",           "Manuell hinzufuegen");
    REG(WIFI_SSID,          "SSID",                 "SSID");
    REG(WIFI_ENCRYPTION,    "Encryption",           "Verschluesselung");
    REG(WIFI_IP_MODE,       "IP Mode",              "IP Modus");
    REG(WIFI_DHCP,          "DHCP (Auto)",          "DHCP (Auto)");
    REG(WIFI_STATIC,        "Static IP",            "Statische IP");
    REG(WIFI_GATEWAY,       "Gateway",              "Gateway");
    REG(WIFI_NETMASK,       "Netmask",              "Netzmaske");
    REG(WIFI_DNS,           "DNS",                  "DNS");
    REG(WIFI_SAVED_CONFIG,  "Saved Config",         "Gespeicherte Config");
    REG(WIFI_SIGNAL,        "Signal",               "Signal");
    REG(NTP_SYNC,           "Sync Time",            "Zeit synchronisieren");
    REG(NTP_SYNCING,        "Syncing time...",      "Synchronisiere Zeit...");
    REG(NTP_SUCCESS,        "Time synced!",         "Zeit synchronisiert!");
    REG(NTP_FAILED,         "Sync failed",          "Sync fehlgeschlagen");
    REG(NTP_TIMEOUT,        "Sync timeout",         "Sync Timeout");
    REG(BLUETOOTH,          "Bluetooth",            "Bluetooth");
    REG(BLUETOOTH_ON,       "Bluetooth ON",         "Bluetooth EIN");
    REG(BLUETOOTH_OFF,      "Bluetooth OFF",        "Bluetooth AUS");
    REG(BLE_STATUS,         "BLE Status",           "BLE Status");
    REG(BLE_SCAN,           "Scan Devices",         "Geraete suchen");
    REG(BLE_SCANNING,       "Scanning...",          "Suche...");
    REG(BLE_NO_DEVICES,     "No devices found",     "Keine Geraete gefunden");
    REG(BLE_CONNECTED_TO,   "Connected to",         "Verbunden mit");
    REG(BLE_NOT_CONNECTED,  "Not connected",        "Nicht verbunden");
    REG(BLE_MAC_ADDRESS,    "MAC",                  "MAC");
    REG(BLE_SIGNAL,         "Signal",               "Signal");
    REG(BLE_PAIRED_DEVICES, "Paired devices",       "Gekoppelte Geraete");
    REG(SYSTEM_TEST,        "System Test",          "Systemtest");
    REG(TR01_CACHE_REBUILD, "TR01 Cache Rebuild",   "TR01 Cache neu aufbauen");
    REG(TR01_CACHE_CLEANUP, "TR01 Cache Cleanup",   "TR01 Cache aufraeumen");
    REG(EXPERT,             "Expert",               "Experte");
    REG(EXPERT_WARNING,     "Possible breaking",    "Moeglich riskant");
    REG(TASK_WORKING,       "Please wait",          "Bitte warten");
    REG(USB_REPLUG_REQUIRED,"USB replug may be needed", "USB replug ggf. noetig");
    REG(SLEEP,              "Sleep",                "Schlafmodus");

    // === Hardware Info ===
    REG(HW_SECTION_MEMORY,  "Memory",               "Speicher");
    REG(HW_SECTION_RUNTIME, "Runtime",              "Laufzeit");
    REG(HW_I2C_BUS,         "I2C Bus",              "I2C Bus");
    REG(HW_BQ25895,         "BQ25895",              "BQ25895");
    REG(HW_TCA9535,         "TCA9535",              "TCA9535");
    REG(HW_DISPLAY,         "Display",              "Display");
    REG(HW_TROPIC01,        "TROPIC01",             "TROPIC01");
    REG(HW_TR01_SESSION,    "TR01 Session",         "TR01 Sitzung");
    REG(HW_WIFI,            "WiFi",                 "WiFi");
    REG(HW_BLE,             "BLE",                  "BLE");
    REG(HW_HEAP,            "Heap",                 "Heap");
    REG(HW_PSRAM,           "PSRAM",                "PSRAM");
    REG(HW_NVS,             "NVS",                  "NVS");
    REG(HW_ENTRIES,         "entries",              "Eintraege");
    REG(HW_BATTERY,         "Battery",              "Batterie");
    REG(HW_TEMP,            "Temp",                 "Temperatur");
    REG(HW_UPTIME,          "Uptime",               "Uptime");
    REG(HW_CHARGING_SUFFIX, " (chg)",               " (laden)");
    REG(HW_NOT_AVAILABLE,   "n/a",                  "n/v");

    // === Actions ===
    REG(ACTIONS,            "Actions",              "Aktionen");
    REG(LIGHT,              "Light",                "Licht");

    // === Footer Hints ===
    REG(HINT_BACK,          "[N] Back",             "[N] Zuruck");
    REG(HINT_SELECT,        "[Y] Select",           "[Y] Auswahlen");
    REG(HINT_OK_BACK,       "[Y] OK [N] Back",      "[Y] OK [N] Zuruck");
    REG(HINT_APPROVE_DENY,  "[Y] Approve  [N] Deny","[Y] OK  [N] Abbruch");
    REG(HINT_BRIGHTNESS,    "<4 6> Adjust [Y] Save", "<4 6> Anpassen [Y] Speichern");
    REG(HINT_PIN_INPUT,     "[0-9] Input [Y] OK",   "[0-9] Eingabe [Y] OK");
    REG(HINT_T9_INPUT,      "[0-9] T9 [Y] OK",      "[0-9] T9 [Y] OK");
    REG(HINT_LIST_MENU,     "[3] Menu",             "[3] Menu");
    REG(HINT_SCROLL_BACK,   "[2/8] Scroll [N] Back","[2/8] Scrollen [N] Zuruck");
    REG(HINT_FIELD_NAV,     "[4] <  [6] >",         "[4] <  [6] >");
    REG(HINT_DATE_INPUT,    "[0-9] [Y] OK [N] Clear",   "[0-9] [Y] OK [N] Loeschen");
    REG(HINT_TIME_INPUT,    "[0-9] [Y] OK [N] Clear",   "[0-9] [Y] OK [N] Loeschen");

    // === QR ===
    REG(QR_ERROR,           "QR Error",            "QR Fehler");
    REG(NO_DATA,            "No data",             "Keine Daten");

    #undef REG
}

} // namespace cdc::ui
