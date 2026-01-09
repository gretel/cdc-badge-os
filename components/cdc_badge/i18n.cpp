// Internationalization (i18n) Module for CDC Badge
// Supports multiple languages with NVS persistence

#include "i18n.h"
#include "cdc_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#define NVS_NAMESPACE "badge"
#define NVS_KEY_LANG "lang"

// Current language (default: English)
static language_t g_language = LANG_EN;
static bool g_initialized = false;

// Language names (in their own language)
static const char* language_names[] = {
    "English",   // LANG_EN
    "Deutsch",   // LANG_DE
};

// Translation strings - English
static const char* strings_en[] = {
    // Menu titles
    "Main Menu",        // STR_MAIN_MENU
    "Settings",         // STR_SETTINGS
    "Badge Texts",      // STR_BADGE_TEXTS

    // Main menu items
    "Authenticator",    // STR_TOTP_CODES
    "WebAuthn",       // STR_FIDO2_KEYS
    "System Test",      // STR_SYSTEM_TEST
    "Deep Sleep",       // STR_SLEEP

    // Settings menu items
    "Change PIN",       // STR_CHANGE_PIN
    "Brightness",       // STR_BRIGHTNESS
    "Set Date",         // STR_SET_DATE
    "Set Time",         // STR_SET_TIME
    "Set Date/Time",    // STR_SET_DATETIME
    "Language",         // STR_LANGUAGE
    "Back",             // STR_BACK

    // Badge texts menu
    "Name",             // STR_NAME
    "Info",             // STR_INFO
    "Info2",            // STR_INFO2

    // PIN screens
    "Enter PIN",        // STR_ENTER_PIN
    "Current PIN",      // STR_CURRENT_PIN
    "New PIN",          // STR_NEW_PIN
    "Confirm PIN",      // STR_CONFIRM_PIN

    // Lock screen
    "[Y] Unlock  [3] Menu",   // STR_UNLOCK
    "Press any key",    // STR_PRESS_ANY_KEY

    // Date/Time
    "Set Date",         // STR_DATE
    "Set Time",         // STR_TIME

    // Messages
    "Saved!",           // STR_SAVED
    "Unlocked!",        // STR_UNLOCKED
    "Wrong PIN",        // STR_WRONG_PIN
    "Locked out!",      // STR_LOCKED_OUT
    "PINs don't match", // STR_PINS_DONT_MATCH
    "PIN Changed!",     // STR_PIN_CHANGED
    "Deleted!",         // STR_DELETED
    "Delete failed",    // STR_DELETE_FAILED
    "USB not ready",    // STR_USB_NOT_READY
    "Code typed!",      // STR_CODE_TYPED
    "No accounts",      // STR_NO_ACCOUNTS
    "No keys",          // STR_NO_KEYS
    "Unplug USB first", // STR_UNPLUG_USB
    "Time not synced!", // STR_TIME_NOT_SYNCED

    // Date/Time saved messages
    "Date saved!",      // STR_DATE_SAVED
    "Time saved!",      // STR_TIME_SAVED
    "Too many attempts",// STR_TOO_MANY_ATTEMPTS
    "Save failed",      // STR_SAVE_FAILED

    // Actions
    "Select",           // STR_SELECT
    "Edit",             // STR_EDIT
    "Delete",           // STR_DELETE
    "Cancel",           // STR_CANCEL
    "Approve",          // STR_APPROVE
    "Deny",             // STR_DENY
    "OK",               // STR_OK

    // FIDO2/TOTP specific
    "TOTP Code",        // STR_TOTP_CODE
    "WebAuthn",    // STR_FIDO2_REQUEST
    "Sign in to",       // STR_FIDO2_DETAIL
    "Register Key",     // STR_REGISTER_KEY
    "Sign In",          // STR_SIGN_IN
    "Use this device?", // STR_USE_DEVICE
    "Show Code",        // STR_SHOW_CODE
    "Type Code",        // STR_TYPE_CODE

    // Badge input prompts
    "Enter Name",       // STR_BADGE_NAME
    "Enter Info",       // STR_BADGE_INFO
    "Enter Info2",      // STR_BADGE_INFO2

    // UI Hints
    "[Y] Type  [N] Back",           // STR_HINT_TYPE_BACK
    "[3] Menu  [Y] Type  [N] Back", // STR_HINT_TOTP_CODE
    "[3] Delete  [N] Back",         // STR_HINT_DELETE_BACK
    "[Y] Approve  [N] Deny",        // STR_HINT_APPROVE_DENY
    "[4] -  [6] +  [Y] Save",       // STR_HINT_BRIGHTNESS
    "[N] Del  [2s N] Abort  [Y] OK",// STR_HINT_T9_INPUT
    "[0-9] Digit  [N] Del  [Y] OK", // STR_HINT_PIN_INPUT
    "[0-9] [4/6] Field [Y] OK",     // STR_HINT_DATE_TIME
    "[N] Back",                     // STR_HINT_BACK
    "[Y] Select",                   // STR_HINT_SELECT
    "[3] Menu",                     // STR_HINT_LIST_MENU

    // Tools Menu
    "Tools",                        // STR_TOOLS
    "NTP Sync",                     // STR_NTP_SYNC
    "Syncing time...",              // STR_NTP_SYNCING
    "Time synced!",                 // STR_NTP_SUCCESS
    "NTP sync failed",              // STR_NTP_FAILED
    "Timeout",                      // STR_TIMEOUT

    // WiFi
    "WiFi",                         // STR_WIFI_MENU
    "WiFi Setup",                   // STR_WIFI_SETUP
    "Connect",                      // STR_WIFI_CONNECT
    "Details",                      // STR_WIFI_DETAILS
    "Disconnect",                   // STR_WIFI_DISCONNECT
    "Disconnected",                 // STR_WIFI_DISCONNECTED
    "No WiFi configured",           // STR_WIFI_NO_CONFIG
    "Scanning...",                  // STR_WIFI_SCANNING
    "No networks",                  // STR_WIFI_NO_NETWORKS
    "Password",                     // STR_WIFI_PASSWORD
    "Encryption",                   // STR_WIFI_ENCRYPTION
    "IP Mode",                      // STR_WIFI_IP_MODE
    "DHCP (Auto)",                  // STR_WIFI_DHCP
    "Static IP",                    // STR_WIFI_STATIC
    "Connecting...",                // STR_WIFI_CONNECTING
    "Connected!",                   // STR_WIFI_CONNECTED
    "Connection failed",            // STR_WIFI_FAILED
    "Last IP",                      // STR_WIFI_SAVED_IP
    "Add Manual",                   // STR_WIFI_ADD_MANUAL
    "SSID",                         // STR_WIFI_SSID

    // Settings
    "Timezone",                     // STR_TIMEZONE

    // Bluetooth
    "Bluetooth ON",                 // STR_BLUETOOTH_ON
    "Bluetooth OFF",                // STR_BLUETOOTH_OFF
    "BLE Auto-Off",                 // STR_BLUETOOTH_AUTO_OFF
    "WiFi off first",               // STR_BLUETOOTH_DISABLE_WIFI
    "BT off first",                 // STR_WIFI_DISABLE_BLUETOOTH

    // Certificate Authority (CA)
    "CA Management",                // STR_CA_MENU
    "CA Status",                    // STR_CA_STATUS
    "Generate Root CA",             // STR_CA_GENERATE
    "Import Root CA",               // STR_CA_IMPORT
    "Show Cert",                    // STR_CA_EXPORT
    "Sign CSR",                     // STR_CA_SIGN_CSR
    "CA not initialized",           // STR_CA_NOT_INIT
    "CA initialized",               // STR_CA_INITIALIZED
    "Generating...",                // STR_CA_GENERATING
    "CA generated!",                // STR_CA_GENERATED
    "Generation failed",            // STR_CA_GENERATE_FAILED
    "Common Name",                  // STR_CA_COMMON_NAME
    "Created",                      // STR_CA_CREATED
    "Valid until",                  // STR_CA_VALID_UNTIL
    "Issued certs",                 // STR_CA_ISSUED_CERTS
    "Next serial",                  // STR_CA_SERIAL
    "Show QR Code",                 // STR_CA_SHOW_QR
    "CA Public Key",                // STR_CA_QR_PUBKEY
    "Reset Root CA",                // STR_CA_RESET
    "Delete CA? Y=Yes N=No",        // STR_CA_RESET_CONFIRM
    "CA deleted",                   // STR_CA_RESET_SUCCESS
    // CA Wizard
    "Common Name (CN)",             // STR_CA_ENTER_CN
    "Organization (O)",             // STR_CA_ENTER_ORG
    "Org. Unit (OU)",               // STR_CA_ENTER_OU
    "Country (C)",                  // STR_CA_ENTER_COUNTRY
    "City/Locality (L)",            // STR_CA_ENTER_LOCALITY
    "State/Province (ST)",          // STR_CA_ENTER_STATE
    "Validity (Years)",             // STR_CA_VALIDITY_YEARS
};

// Translation strings - German
static const char* strings_de[] = {
    // Menu titles
    "Hauptmenu",        // STR_MAIN_MENU
    "Einstellungen",    // STR_SETTINGS
    "Badge-Texte",      // STR_BADGE_TEXTS

    // Main menu items
    "Authentikator",    // STR_TOTP_CODES
    "WebAuthn",       // STR_FIDO2_KEYS
    "Systemtest",       // STR_SYSTEM_TEST
    "Standby",          // STR_SLEEP

    // Settings menu items
    "PIN aendern",      // STR_CHANGE_PIN
    "Helligkeit",       // STR_BRIGHTNESS
    "Datum setzen",     // STR_SET_DATE
    "Uhrzeit setzen",   // STR_SET_TIME
    "Datum/Zeit",       // STR_SET_DATETIME
    "Sprache",          // STR_LANGUAGE
    "Zurueck",          // STR_BACK

    // Badge texts menu
    "Name",             // STR_NAME
    "Info",             // STR_INFO
    "Info2",            // STR_INFO2

    // PIN screens
    "PIN eingeben",     // STR_ENTER_PIN
    "Aktuelle PIN",     // STR_CURRENT_PIN
    "Neue PIN",         // STR_NEW_PIN
    "PIN bestaetigen",  // STR_CONFIRM_PIN

    // Lock screen
    "[Y] Entsperren  [3] Menu",   // STR_UNLOCK
    "Taste druecken",   // STR_PRESS_ANY_KEY

    // Date/Time
    "Datum setzen",     // STR_DATE
    "Uhrzeit setzen",   // STR_TIME

    // Messages
    "Gespeichert!",     // STR_SAVED
    "Entsperrt!",       // STR_UNLOCKED
    "Falsche PIN",      // STR_WRONG_PIN
    "Gesperrt!",        // STR_LOCKED_OUT
    "PINs ungleich",    // STR_PINS_DONT_MATCH
    "PIN geaendert!",   // STR_PIN_CHANGED
    "Geloescht!",       // STR_DELETED
    "Loeschen fehlg.",  // STR_DELETE_FAILED
    "USB nicht bereit", // STR_USB_NOT_READY
    "Code getippt!",    // STR_CODE_TYPED
    "Keine Konten",     // STR_NO_ACCOUNTS
    "Keine Keys",       // STR_NO_KEYS
    "USB trennen",      // STR_UNPLUG_USB
    "Zeit nicht sync!", // STR_TIME_NOT_SYNCED

    // Date/Time saved messages
    "Datum gesp.!",        // STR_DATE_SAVED
    "Zeit gesp.!",         // STR_TIME_SAVED
    "Zu viele Versuche",   // STR_TOO_MANY_ATTEMPTS
    "Speichern fehlg.",    // STR_SAVE_FAILED

    // Actions
    "Waehlen",          // STR_SELECT
    "Bearbeiten",       // STR_EDIT
    "Loeschen",         // STR_DELETE
    "Abbrechen",        // STR_CANCEL
    "Bestaetigen",      // STR_APPROVE
    "Ablehnen",         // STR_DENY
    "OK",               // STR_OK

    // FIDO2/TOTP specific
    "TOTP Code",        // STR_TOTP_CODE
    "WebAuthn",    // STR_FIDO2_REQUEST
    "Anmelden bei",     // STR_FIDO2_DETAIL
    "Key registrieren", // STR_REGISTER_KEY
    "Anmelden",         // STR_SIGN_IN
    "Dieses Geraet?",   // STR_USE_DEVICE
    "Code anzeigen",    // STR_SHOW_CODE
    "Code tippen",      // STR_TYPE_CODE

    // Badge input prompts
    "Name eingeben",    // STR_BADGE_NAME
    "Info eingeben",    // STR_BADGE_INFO
    "Info2 eingeben",   // STR_BADGE_INFO2

    // UI Hints
    "[Y] Tippen  [N] Zurueck",      // STR_HINT_TYPE_BACK
    "[3] Menu [Y] Tippen [N] Zur.", // STR_HINT_TOTP_CODE
    "[3] Loeschen  [N] Zurueck",    // STR_HINT_DELETE_BACK
    "[Y] OK  [N] Abbruch",          // STR_HINT_APPROVE_DENY
    "[4] -  [6] +  [Y] Speichern",  // STR_HINT_BRIGHTNESS
    "[N] Del [2s N] Abbr. [Y] OK",  // STR_HINT_T9_INPUT
    "[0-9] [N] Del  [Y] OK",        // STR_HINT_PIN_INPUT
    "[0-9] [4/6] Feld [Y] OK",      // STR_HINT_DATE_TIME
    "[N] Zurueck",                  // STR_HINT_BACK
    "[Y] Waehlen",                  // STR_HINT_SELECT
    "[3] Menue",                    // STR_HINT_LIST_MENU

    // Tools Menu
    "Tools",                        // STR_TOOLS (bleibt englisch)
    "NTP Sync",                     // STR_NTP_SYNC
    "Zeit Sync...",      // STR_NTP_SYNCING
    "Zeit Sync ok!",         // STR_NTP_SUCCESS
    "NTP Sync Fehler",      // STR_NTP_FAILED
    "Zeitüberschreitung",           // STR_TIMEOUT

    // WiFi
    "WiFi",                         // STR_WIFI_MENU
    "WLAN Setup",             // STR_WIFI_SETUP
    "Verbinden",                    // STR_WIFI_CONNECT
    "Details",                      // STR_WIFI_DETAILS
    "Trennen",                      // STR_WIFI_DISCONNECT
    "Getrennt",                     // STR_WIFI_DISCONNECTED
    "Kein WLAN konfiguriert",       // STR_WIFI_NO_CONFIG
    "Suche...",                     // STR_WIFI_SCANNING
    "Keine Netzwerke",              // STR_WIFI_NO_NETWORKS
    "Passwort",                     // STR_WIFI_PASSWORD
    "Verschluesselung",             // STR_WIFI_ENCRYPTION
    "IP Modus",                     // STR_WIFI_IP_MODE
    "DHCP (Auto)",                  // STR_WIFI_DHCP
    "Statische IP",                 // STR_WIFI_STATIC
    "Verbinde...",                  // STR_WIFI_CONNECTING
    "Verbunden!",                   // STR_WIFI_CONNECTED
    "Verbindungsfehler",    // STR_WIFI_FAILED
    "Letzte IP",                    // STR_WIFI_SAVED_IP
    "Manuell",          // STR_WIFI_ADD_MANUAL
    "SSID",                         // STR_WIFI_SSID

    // Settings
    "Zeitzone",                     // STR_TIMEZONE

    // Bluetooth
    "Bluetooth AN",                 // STR_BLUETOOTH_ON
    "Bluetooth AUS",                // STR_BLUETOOTH_OFF
    "BLE Auto-Aus",                 // STR_BLUETOOTH_AUTO_OFF
    "Erst WiFi aus",                // STR_BLUETOOTH_DISABLE_WIFI
    "Erst BT aus",                  // STR_WIFI_DISABLE_BLUETOOTH

    // Certificate Authority (CA)
    "CA-Verwaltung",                // STR_CA_MENU
    "CA-Status",                    // STR_CA_STATUS
    "Root-CA generieren",           // STR_CA_GENERATE
    "Root-CA importieren",          // STR_CA_IMPORT
    "Zertifikat",                   // STR_CA_EXPORT (public cert, not private key!)
    "CSR signieren",                // STR_CA_SIGN_CSR
    "CA nicht initialisiert",       // STR_CA_NOT_INIT
    "CA initialisiert",             // STR_CA_INITIALIZED
    "Generiere...",                 // STR_CA_GENERATING
    "CA generiert!",                // STR_CA_GENERATED
    "Generierung fehlg.",           // STR_CA_GENERATE_FAILED
    "Common Name",                  // STR_CA_COMMON_NAME
    "Erstellt",                     // STR_CA_CREATED
    "Gueltig bis",                  // STR_CA_VALID_UNTIL
    "Ausgest. Zertif.",             // STR_CA_ISSUED_CERTS
    "Naechste Seriennr.",           // STR_CA_SERIAL
    "QR Code anzeigen",             // STR_CA_SHOW_QR
    "CA Public Key",                // STR_CA_QR_PUBKEY
    "Root-CA loeschen",             // STR_CA_RESET
    "CA loeschen? Y=Ja N=Nein",     // STR_CA_RESET_CONFIRM
    "CA geloescht",                 // STR_CA_RESET_SUCCESS
    // CA Wizard
    "Common Name (CN)",             // STR_CA_ENTER_CN
    "Organisation (O)",             // STR_CA_ENTER_ORG
    "Abteilung (OU)",               // STR_CA_ENTER_OU
    "Land (C)",                     // STR_CA_ENTER_COUNTRY
    "Stadt (L)",                    // STR_CA_ENTER_LOCALITY
    "Bundesland (ST)",              // STR_CA_ENTER_STATE
    "Gueltigkeit (Jahre)",          // STR_CA_VALIDITY_YEARS
};

// All language string tables
static const char** all_strings[] = {
    strings_en,
    strings_de,
};

void i18n_init(void) {
    if (g_initialized) return;

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        uint8_t lang = 0;
        if (nvs_get_u8(nvs, NVS_KEY_LANG, &lang) == ESP_OK) {
            if (lang < LANG_COUNT) {
                g_language = (language_t)lang;
                LOG_I("I18N", "Loaded language: %s", language_names[g_language]);
            }
        }
        nvs_close(nvs);
    }

    g_initialized = true;
}

language_t i18n_get_language(void) {
    if (!g_initialized) i18n_init();
    return g_language;
}

void i18n_set_language(language_t lang) {
    if (lang >= LANG_COUNT) return;

    g_language = lang;

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u8(nvs, NVS_KEY_LANG, (uint8_t)lang);
        nvs_commit(nvs);
        nvs_close(nvs);
        LOG_I("I18N", "Saved language: %s", language_names[lang]);
    }
}

const char* i18n_str(string_id_t id) {
    if (!g_initialized) i18n_init();
    if (id >= STR_COUNT) return "???";
    return all_strings[g_language][id];
}

const char* i18n_get_language_name(language_t lang) {
    if (lang >= LANG_COUNT) return "???";
    return language_names[lang];
}
