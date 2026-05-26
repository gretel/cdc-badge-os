#pragma once

#include <cstdint>
#include <cstddef>

namespace cdc::ui {

/**
 * Language enumeration
 */
enum class Language : uint8_t {
    EN = 0,     // English (default/fallback)
    DE = 1,     // German
    COUNT
};

/**
 * Core string IDs (system-wide)
 * Modules register additional strings dynamically
 */
enum class StringId : uint16_t {
    // === System ===
    MAIN_MENU = 0,
    SETTINGS,
    HARDWARE,
    TOOLS,
    HARDWARE_INFO,
    NAME,
    INFO,
    INFO2,
    DEFAULT_NAME,
    DEFAULT_INFO,
    BACK,
    OK,
    CANCEL,
    SAVE,
    DELETE,
    EDIT,
    VIEW,
    SELECT,
    YES,
    NO,
    ON,
    OFF,
    SAVED,
    DELETED,
    FAILED,
    TIMEOUT,
    EMPTY,

    // === Lock Screen ===
    LOCK,
    UNLOCK,
    ENTER_PIN,
    PRESS_ANY_KEY,
    DEEP_SLEEP,
    WRONG_PIN,
    LOCKED_OUT,
    TOO_MANY_ATTEMPTS,

    // === PIN ===
    CHANGE_PIN,
    CURRENT_PIN,
    NEW_PIN,
    CONFIRM_PIN,
    PIN_CHANGED,
    PINS_DONT_MATCH,
    PIN_TOO_SHORT,
    PIN_MISMATCH,
    RETRIES,
    ERROR_GENERIC,

    // === Settings ===
    BRIGHTNESS,
    LANGUAGE,
    TIMEZONE,
    SUMMER_TIME,
    BADGE_TEXT,
    AUTO_SLEEP,
    SET_DATE,
    SET_TIME,
    DATE,
    TIME,
    DATE_SAVED,
    TIME_SAVED,
    MODULES,
    NEVER,
    MINUTES,

    // === Hardware ===
    WIFI_MENU,
    WIFI_SETUP,
    WIFI_CONNECT,
    WIFI_DETAILS,
    WIFI_DISCONNECT,
    WIFI_SCANNING,
    WIFI_NO_NETWORKS,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_DISCONNECTED,
    WIFI_FAILED,
    WIFI_NO_CONFIG,
    WIFI_PASSWORD,
    WIFI_ADD_MANUAL,
    WIFI_SSID,
    WIFI_ENCRYPTION,
    WIFI_IP_MODE,
    WIFI_DHCP,
    WIFI_STATIC,
    WIFI_GATEWAY,
    WIFI_NETMASK,
    WIFI_DNS,
    WIFI_SAVED_CONFIG,
    WIFI_SIGNAL,
    NTP_SYNC,
    NTP_SYNCING,
    NTP_SUCCESS,
    NTP_FAILED,
    NTP_TIMEOUT,
    BLUETOOTH,
    BLUETOOTH_ON,
    BLUETOOTH_OFF,
    BLE_STATUS,
    BLE_SCAN,
    BLE_SCANNING,
    BLE_NO_DEVICES,
    BLE_CONNECTED_TO,
    BLE_NOT_CONNECTED,
    BLE_MAC_ADDRESS,
    BLE_SIGNAL,
    BLE_PAIRED_DEVICES,
    SYSTEM_TEST,
    TR01_CACHE_REBUILD,
    TR01_CACHE_CLEANUP,
    EXPERT,
    EXPERT_WARNING,
    TASK_WORKING,
    SLEEP,
    USB_REPLUG_REQUIRED,
    MODULE_ERROR_GENERIC,
    MODULE_RETRY_PROMPT,

    // === Hardware Info ===
    HW_SECTION_MEMORY,
    HW_SECTION_RUNTIME,
    HW_I2C_BUS,
    HW_BQ25895,
    HW_TCA9535,
    HW_DISPLAY,
    HW_TROPIC01,
    HW_TR01_SESSION,
    HW_TR01_RISCV_FW,
    HW_TR01_SPECT_FW,
    HW_TR01_RMEM_SLOT,
    HW_WIFI,
    HW_BLE,
    HW_HEAP,
    HW_PSRAM,
    HW_NVS,
    HW_ENTRIES,
    HW_BATTERY,
    HW_TEMP,
    HW_UPTIME,
    HW_CHARGING_SUFFIX,
    HW_NOT_AVAILABLE,

    // === Actions ===
    ACTIONS,
    LIGHT,

    // === Footer Hints ===
    HINT_BACK,
    HINT_SELECT,
    HINT_OK_BACK,
    HINT_APPROVE_DENY,
    HINT_BRIGHTNESS,
    HINT_PIN_INPUT,
    HINT_T9_INPUT,
    T9_FULL,
    HINT_LIST_MENU,
    HINT_SCROLL_BACK,
    HINT_FIELD_NAV,
    HINT_DATE_INPUT,
    HINT_TIME_INPUT,
    HINT_PASSWORD_HIDDEN,
    HINT_PASSWORD_REVEALED,

    // === QR ===
    QR_ERROR,
    NO_DATA,

    // === Bootloader ===
    BOOTLOADER,

    // Core string count - modules start from here
    CORE_COUNT
};

/**
 * Translation entry for module registration
 */
struct Translation {
    uint16_t stringId;      // String ID (core or module-specific)
    Language lang;          // Language
    const char* text;       // Translation text
};

/**
 * I18n Controller Interface
 *
 * Provides internationalization with:
 * - Core system strings (StringId enum)
 * - Module-registered strings (dynamic IDs)
 * - English fallback when translation missing
 *
 * Usage:
 *   auto& i18n = I18n::instance();
 *   const char* text = i18n.str(StringId::SETTINGS);
 *
 * Module registration:
 *   uint16_t baseId = i18n.registerModule("totp", 50);  // Reserve 50 IDs
 *   i18n.registerTranslation(baseId + 0, Language::EN, "TOTP Codes");
 *   i18n.registerTranslation(baseId + 0, Language::DE, "TOTP Codes");
 */
class I18n {
public:
    /**
     * Get singleton instance
     */
    static I18n& instance();

    /**
     * Initialize i18n system (loads language from NVS)
     */
    bool init();

    /**
     * Get/set current language
     */
    Language getLanguage() const { return currentLang_; }
    void setLanguage(Language lang);

    /**
     * Get language display name
     */
    const char* getLanguageName(Language lang) const;

    /**
     * Get translated string for core string ID
     * Falls back to English if translation missing
     */
    const char* str(StringId id) const;

    /**
     * Get translated string by numeric ID (for module strings)
     * Falls back to English if translation missing
     */
    const char* str(uint16_t id) const;

    /**
     * Register a module and reserve string IDs
     * @param moduleName Module name for debugging
     * @param count Number of string IDs to reserve
     * @return Base string ID for this module, or 0 on failure
     */
    uint16_t registerModule(const char* moduleName, uint16_t count);

    /**
     * Register a translation
     * @param stringId String ID (core or from registerModule)
     * @param lang Language
     * @param text Translation text
     * @return true on success
     */
    bool registerTranslation(uint16_t stringId, Language lang, const char* text);

    /**
     * Batch register translations
     * @param translations Array of translations (terminated by stringId=0xFFFF)
     */
    void registerTranslations(const Translation* translations);

    /**
     * Get total registered string count
     */
    uint16_t getStringCount() const { return nextModuleId_; }

private:
    I18n() = default;

    // Configuration
    static constexpr uint16_t MAX_STRINGS = 512;
    static constexpr uint8_t LANG_COUNT = static_cast<uint8_t>(Language::COUNT);

    // String storage (pointer arrays)
    const char* strings_[LANG_COUNT][MAX_STRINGS] = {};

    // Current language
    Language currentLang_ = Language::EN;

    // Next available module ID
    uint16_t nextModuleId_ = static_cast<uint16_t>(StringId::CORE_COUNT);

    // Initialize core strings
    void initCoreStrings();

    // Save/load from NVS
    void loadFromNvs();
    void saveToNvs();
};

// Convenience function
inline const char* tr(StringId id) {
    return I18n::instance().str(id);
}

inline const char* tr(uint16_t id) {
    return I18n::instance().str(id);
}

} // namespace cdc::ui
