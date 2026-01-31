/**
 * Serial Command Processor Implementation
 * Handles input buffering, line editing, and command dispatch
 */

#include "serial_cmd/SerialCmd.h"
#include "serial_cmd/Console.h"
#include "serial_cmd/ICommandRegistry.h"
#include "cdc_core/feature_flags.h"
#include "cdc_core/PinManager.h"
#include "cdc_core/TropicStorage.h"
#include "cdc_hal/ISecureElement.h"
#include "cdc_log.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <sys/time.h>
#include <time.h>

static const char* TAG = "SERIAL";

namespace cdc::serial {

// ============================================================================
// Constants
// ============================================================================

static constexpr size_t HISTORY_MAX = 10;
static constexpr size_t BLOB_PREVIEW_BYTES = 64;
static constexpr size_t HEX_DUMP_WIDTH = 16;
static constexpr size_t NVS_KEY_MAX_LEN = 15;
static constexpr size_t NVS_NAMESPACE_MAX_LEN = 15;
static constexpr int YEAR_MIN = 2020;
static constexpr int YEAR_MAX = 2100;
static constexpr uint32_t WIPE_PROGRESS_INTERVAL = 64;

// ============================================================================
// Static State
// ============================================================================

static char s_cmdBuffer[SerialCmd::CMD_BUFFER_SIZE] = {};
static size_t s_cmdBufferPos = 0;
static bool s_initialized = false;

// Command history (PSRAM)
EXT_RAM_BSS_ATTR static char s_historyBuffer[HISTORY_MAX][SerialCmd::CMD_BUFFER_SIZE];
static size_t s_historyCount = 0;
static size_t s_historyHead = 0;
static size_t s_historyPos = 0;

// Escape sequence state
enum class EscState : uint8_t { NONE, ESC, BRACKET };
static EscState s_escState = EscState::NONE;

// Callbacks
static TextChangeCallback s_textCallback = nullptr;
static TimeChangeCallback s_timeCallback = nullptr;

// Authentication state
static bool s_authenticated = false;
static uint64_t s_authTimestamp = 0;

// ============================================================================
// Helper Functions - General Purpose
// ============================================================================

#if FEATURE_SECURE_SERIAL
/**
 * Reset authentication timer on command execution
 */
static void resetAuthTimer() {
    if (s_authenticated) {
        s_authTimestamp = esp_timer_get_time();
    }
}
#endif

/**
 * Add command to history buffer (ring buffer)
 */
static void historyAdd(const char* cmd) {
    if (!cmd || !*cmd) return;

    strncpy(s_historyBuffer[s_historyHead], cmd, SerialCmd::CMD_BUFFER_SIZE - 1);
    s_historyBuffer[s_historyHead][SerialCmd::CMD_BUFFER_SIZE - 1] = '\0';

    s_historyHead = (s_historyHead + 1) % HISTORY_MAX;
    if (s_historyCount < HISTORY_MAX) {
        s_historyCount++;
    }
}

/**
 * Get history entry by index (0 = newest)
 */
static const char* historyGet(size_t idx) {
    if (idx >= s_historyCount) return nullptr;
    size_t pos = (s_historyHead + HISTORY_MAX - 1 - idx) % HISTORY_MAX;
    return s_historyBuffer[pos];
}

/**
 * Clear current line and redraw with new content
 */
static void redrawLine(const char* newContent, size_t& bufferPos) {
    while (bufferPos > 0) {
        Console::print("\b \b");
        bufferPos--;
    }

    if (newContent) {
        size_t len = strlen(newContent);
        if (len >= SerialCmd::CMD_BUFFER_SIZE) len = SerialCmd::CMD_BUFFER_SIZE - 1;
        strncpy(s_cmdBuffer, newContent, len);
        s_cmdBuffer[len] = '\0';
        bufferPos = len;
        Console::print(s_cmdBuffer);
    }
}

// ============================================================================
// Helper Functions - Slot Parsing
// ============================================================================

/**
 * Result of slot parsing operation
 */
struct SlotParseResult {
    bool valid;
    long value;
};

/**
 * Parse a slot number from string argument
 * @param args Input string containing the slot number
 * @param maxSlot Maximum valid slot value (exclusive)
 * @param slotTypeName Name for error messages (e.g., "ECC slot", "R-Memory slot")
 * @return Parse result with validity flag and parsed value
 */
static SlotParseResult parseSlotArg(const char* args, uint16_t maxSlot, const char* slotTypeName) {
    SlotParseResult result = {false, 0};

    if (!args || !*args) {
        Console::printf("Usage: Provide a %s number\r\n", slotTypeName);
        return result;
    }

    char* endptr = nullptr;
    long slotVal = strtol(args, &endptr, 10);

    if (endptr == args || *endptr != '\0' || slotVal < 0) {
        Console::printf("ERROR: Invalid %s number\r\n", slotTypeName);
        return result;
    }

    if (slotVal >= maxSlot) {
        Console::printf("ERROR: Invalid %s (0-%d)\r\n", slotTypeName, maxSlot - 1);
        return result;
    }

    result.valid = true;
    result.value = slotVal;
    return result;
}

// ============================================================================
// Helper Functions - Secure Element
// ============================================================================

/**
 * Get secure element instance with validation
 * @return Pointer to SE instance, or nullptr with error message printed
 */
static hal::ISecureElement* getSecureElementWithCheck() {
    auto* se = hal::getSecureElementInstance();
    if (!se) {
        Console::printf("ERROR: Secure Element not available\r\n");
    }
    return se;
}

// ============================================================================
// Helper Functions - NVS
// ============================================================================

/**
 * Print hex dump of binary data
 */
static void printHexDump(const uint8_t* data, size_t len, size_t maxBytes) {
    for (size_t i = 0; i < len && i < maxBytes; i += HEX_DUMP_WIDTH) {
        Console::printf("  %04X: ", (unsigned)i);
        for (size_t j = 0; j < HEX_DUMP_WIDTH && (i + j) < len; j++) {
            Console::printf("%02X ", data[i + j]);
        }
        Console::printf("\r\n");
    }
    if (len > maxBytes) {
        Console::printf("  ... (%d more bytes)\r\n", (int)(len - maxBytes));
    }
}

/**
 * Get human-readable name for NVS type
 */
static const char* getNvsTypeName(nvs_type_t type) {
    switch (type) {
        case NVS_TYPE_U8:   return "u8";
        case NVS_TYPE_I8:   return "i8";
        case NVS_TYPE_U16:  return "u16";
        case NVS_TYPE_I16:  return "i16";
        case NVS_TYPE_U32:  return "u32";
        case NVS_TYPE_I32:  return "i32";
        case NVS_TYPE_U64:  return "u64";
        case NVS_TYPE_I64:  return "i64";
        case NVS_TYPE_STR:  return "str";
        case NVS_TYPE_BLOB: return "blob";
        default:            return "?";
    }
}

/**
 * Find NVS key type by iterating through namespace
 */
static nvs_type_t findNvsKeyType(const char* ns, const char* key) {
    nvs_iterator_t it = nullptr;
    esp_err_t err = nvs_entry_find("nvs", ns, NVS_TYPE_ANY, &it);
    nvs_type_t keyType = NVS_TYPE_ANY;

    while (it != nullptr) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        if (strcmp(info.key, key) == 0) {
            keyType = info.type;
            break;
        }
        err = nvs_entry_next(&it);
        if (err != ESP_OK) break;
    }
    nvs_release_iterator(it);
    return keyType;
}

/**
 * Print NVS value based on its type
 */
static void printNvsValue(nvs_handle_t nvs, const char* key, nvs_type_t type) {
    switch (type) {
        case NVS_TYPE_U8: {
            uint8_t val;
            if (nvs_get_u8(nvs, key, &val) == ESP_OK) {
                Console::printf("%u (0x%02X)\r\n", val, val);
            }
            break;
        }
        case NVS_TYPE_I8: {
            int8_t val;
            if (nvs_get_i8(nvs, key, &val) == ESP_OK) {
                Console::printf("%d\r\n", val);
            }
            break;
        }
        case NVS_TYPE_U16: {
            uint16_t val;
            if (nvs_get_u16(nvs, key, &val) == ESP_OK) {
                Console::printf("%u (0x%04X)\r\n", val, val);
            }
            break;
        }
        case NVS_TYPE_I16: {
            int16_t val;
            if (nvs_get_i16(nvs, key, &val) == ESP_OK) {
                Console::printf("%d\r\n", val);
            }
            break;
        }
        case NVS_TYPE_U32: {
            uint32_t val;
            if (nvs_get_u32(nvs, key, &val) == ESP_OK) {
                Console::printf("%lu (0x%08lX)\r\n", (unsigned long)val, (unsigned long)val);
            }
            break;
        }
        case NVS_TYPE_I32: {
            int32_t val;
            if (nvs_get_i32(nvs, key, &val) == ESP_OK) {
                Console::printf("%ld\r\n", (long)val);
            }
            break;
        }
        case NVS_TYPE_U64: {
            uint64_t val;
            if (nvs_get_u64(nvs, key, &val) == ESP_OK) {
                Console::printf("%llu\r\n", (unsigned long long)val);
            }
            break;
        }
        case NVS_TYPE_I64: {
            int64_t val;
            if (nvs_get_i64(nvs, key, &val) == ESP_OK) {
                Console::printf("%lld\r\n", (long long)val);
            }
            break;
        }
        case NVS_TYPE_STR: {
            size_t len = 0;
            if (nvs_get_str(nvs, key, nullptr, &len) == ESP_OK && len > 0) {
                char* buf = static_cast<char*>(malloc(len));
                if (!buf) {
                    LOG_E(TAG, "Failed to allocate %d bytes for NVS string", (int)len);
                    Console::printf("(allocation failed)\r\n");
                    break;
                }
                if (nvs_get_str(nvs, key, buf, &len) == ESP_OK) {
                    Console::printf("\"%s\"\r\n", buf);
                }
                free(buf);
            }
            break;
        }
        case NVS_TYPE_BLOB: {
            size_t len = 0;
            if (nvs_get_blob(nvs, key, nullptr, &len) == ESP_OK && len > 0) {
                Console::printf("(blob, %d bytes)\r\n", (int)len);
                uint8_t* buf = static_cast<uint8_t*>(malloc(len));
                if (!buf) {
                    LOG_E(TAG, "Failed to allocate %d bytes for NVS blob", (int)len);
                    Console::printf("  (allocation failed)\r\n");
                    break;
                }
                if (nvs_get_blob(nvs, key, buf, &len) == ESP_OK) {
                    printHexDump(buf, len, BLOB_PREVIEW_BYTES);
                }
                free(buf);
            }
            break;
        }
        default:
            Console::printf("(unknown type)\r\n");
            break;
    }
}

// ============================================================================
// Helper Functions - Time
// ============================================================================

/**
 * Get current time as struct tm
 * @param tv Output timeval
 * @param tm Output tm struct pointer
 * @return true if successful
 */
static bool getCurrentTime(struct timeval& tv, struct tm*& tm) {
    gettimeofday(&tv, nullptr);
    tm = localtime(&tv.tv_sec);
    return tm != nullptr;
}

/**
 * Set system time from tm struct
 */
static bool setSystemTime(struct tm* tm) {
    struct timeval tv;
    tv.tv_sec = mktime(tm);
    tv.tv_usec = 0;
    return settimeofday(&tv, nullptr) == 0;
}

// ============================================================================
// Command Handlers - System
// ============================================================================

static void cmdHelp(const char* args) {
    (void)args;
    getCommandRegistry().showHelp();
}

static void cmdPing(const char* args) {
    (void)args;
    Console::printf("PONG\r\n");
}

static void cmdStatus(const char* args) {
    (void)args;
    Console::printf("=== System Status ===\r\n");
    Console::printf("Free heap: %lu bytes\r\n", (unsigned long)esp_get_free_heap_size());
    Console::printf("Min free heap: %lu bytes\r\n", (unsigned long)esp_get_minimum_free_heap_size());
    Console::printf("Uptime: %llu ms\r\n", esp_timer_get_time() / 1000ULL);
    Console::flush();
}

static void cmdMem(const char* args) {
    (void)args;
    Console::printf("=== Memory Usage ===\r\n");
    Console::printf("Heap: %lu / %lu bytes free\r\n",
                   (unsigned long)esp_get_free_heap_size(),
                   (unsigned long)heap_caps_get_total_size(MALLOC_CAP_DEFAULT));

    size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psramTotal = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (psramTotal > 0) {
        Console::printf("PSRAM: %lu / %lu bytes free\r\n",
                       (unsigned long)psramFree,
                       (unsigned long)psramTotal);
    }
    Console::flush();
}

static void cmdReboot(const char* args) {
    (void)args;
    Console::printf("Rebooting...\r\n");
    Console::flush();
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
}

static void cmdErrorLog(const char* args) {
    if (args && strcmp(args, "CLEAR") == 0) {
        error_log_clear();
        Console::printf("Error log cleared.\r\n");
    } else {
        error_log_dump();
    }
}

// ============================================================================
// Command Handlers - NVS
// ============================================================================

static void cmdNvsClear(const char* args) {
    if (!args || strcmp(args, "YES") != 0) {
        Console::printf("WARNING: This will ERASE ALL NVS data!\r\n");
        Console::printf("  - All module settings\r\n");
        Console::printf("  - All stored preferences\r\n");
        Console::printf("  - WiFi credentials\r\n");
        Console::printf("  - Timezone settings\r\n");
        Console::printf("\r\nTo proceed, type: NVS_CLEAR YES\r\n");
        return;
    }

    Console::printf("Clearing NVS...\r\n");
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK) {
        Console::printf("ERROR: NVS erase failed (%s)\r\n", esp_err_to_name(err));
        return;
    }
    err = nvs_flash_init();
    if (err != ESP_OK) {
        Console::printf("ERROR: NVS init failed (%s)\r\n", esp_err_to_name(err));
        return;
    }
    Console::printf("OK: NVS cleared. Reboot recommended.\r\n");
}

static void cmdNvsList(const char* args) {
    const char* nsFilter = (args && *args) ? args : nullptr;

    nvs_iterator_t it = nullptr;
    esp_err_t err = nvs_entry_find("nvs", nsFilter, NVS_TYPE_ANY, &it);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        if (nsFilter) {
            Console::printf("Namespace '%s' not found or empty\r\n", nsFilter);
        } else {
            Console::printf("NVS is empty\r\n");
        }
        return;
    }

    if (err != ESP_OK) {
        Console::printf("ERROR: nvs_entry_find failed (%s)\r\n", esp_err_to_name(err));
        return;
    }

    Console::printf("=== NVS Contents ===\r\n");
    if (nsFilter) {
        Console::printf("Namespace: %s\r\n", nsFilter);
    }

    char lastNs[NVS_NAMESPACE_MAX_LEN + 1] = {0};
    int count = 0;

    while (it != nullptr) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);

        if (!nsFilter && strcmp(lastNs, info.namespace_name) != 0) {
            strncpy(lastNs, info.namespace_name, sizeof(lastNs) - 1);
            Console::printf("\r\n[%s]\r\n", info.namespace_name);
        }

        Console::printf("  %s (%s)\r\n", info.key, getNvsTypeName(info.type));
        count++;

        err = nvs_entry_next(&it);
        if (err != ESP_OK) break;
    }

    nvs_release_iterator(it);
    Console::printf("\r\nTotal: %d entries\r\n", count);
}

static void cmdNvsRead(const char* args) {
    if (!args || !*args) {
        Console::printf("Usage: NVS_READ <namespace> <key>\r\n");
        return;
    }

    char ns[NVS_NAMESPACE_MAX_LEN + 1] = {0};
    char key[NVS_KEY_MAX_LEN + 1] = {0};
    if (sscanf(args, "%15s %15s", ns, key) != 2) {
        Console::printf("Usage: NVS_READ <namespace> <key>\r\n");
        return;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        Console::printf("ERROR: Cannot open namespace '%s' (%s)\r\n", ns, esp_err_to_name(err));
        return;
    }

    nvs_type_t keyType = findNvsKeyType(ns, key);
    if (keyType == NVS_TYPE_ANY) {
        Console::printf("ERROR: Key '%s' not found in namespace '%s'\r\n", key, ns);
        nvs_close(nvs);
        return;
    }

    Console::printf("%s.%s = ", ns, key);
    printNvsValue(nvs, key, keyType);
    nvs_close(nvs);
}

static void cmdNvsDel(const char* args) {
    if (!args || !*args) {
        Console::printf("Usage: NVS_DEL <namespace> [key]\r\n");
        Console::printf("  Without key: erases entire namespace\r\n");
        return;
    }

    char ns[NVS_NAMESPACE_MAX_LEN + 1] = {0};
    char key[NVS_KEY_MAX_LEN + 1] = {0};
    int parsed = sscanf(args, "%15s %15s", ns, key);

    if (parsed < 1) {
        Console::printf("Usage: NVS_DEL <namespace> [key]\r\n");
        return;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        Console::printf("ERROR: Cannot open namespace '%s' (%s)\r\n", ns, esp_err_to_name(err));
        return;
    }

    if (parsed == 1 || key[0] == '\0') {
        err = nvs_erase_all(nvs);
        if (err == ESP_OK) {
            nvs_commit(nvs);
            Console::printf("OK: Namespace '%s' erased\r\n", ns);
        } else {
            Console::printf("ERROR: Erase failed (%s)\r\n", esp_err_to_name(err));
        }
    } else {
        err = nvs_erase_key(nvs, key);
        if (err == ESP_OK) {
            nvs_commit(nvs);
            Console::printf("OK: Key '%s.%s' deleted\r\n", ns, key);
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            Console::printf("ERROR: Key '%s' not found\r\n", key);
        } else {
            Console::printf("ERROR: Delete failed (%s)\r\n", esp_err_to_name(err));
        }
    }

    nvs_close(nvs);
}

// ============================================================================
// Command Handlers - Time
// ============================================================================

static void cmdGetTime(const char* args) {
    (void)args;
    struct timeval tv;
    struct tm* tm;
    if (getCurrentTime(tv, tm)) {
        Console::printf("%02d:%02d:%02d\r\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
    } else {
        Console::printf("--:--:--\r\n");
    }
}

static void cmdGetDate(const char* args) {
    (void)args;
    struct timeval tv;
    struct tm* tm;
    if (getCurrentTime(tv, tm)) {
        Console::printf("%02d.%02d.%04d\r\n", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
    } else {
        Console::printf("--.---.----\r\n");
    }
}

static void cmdSetTime(const char* args) {
    if (!args || !*args) {
        Console::printf("Usage: SET_TIME HH:MM:SS\r\n");
        return;
    }
    int h, m, s;
    if (sscanf(args, "%d:%d:%d", &h, &m, &s) != 3) {
        Console::printf("ERROR: Invalid format. Use HH:MM:SS\r\n");
        return;
    }
    if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59) {
        Console::printf("ERROR: Invalid time values\r\n");
        return;
    }

    struct timeval tv;
    struct tm* tm;
    if (getCurrentTime(tv, tm)) {
        tm->tm_hour = h;
        tm->tm_min = m;
        tm->tm_sec = s;
        if (setSystemTime(tm)) {
            Console::printf("OK: Time set to %02d:%02d:%02d\r\n", h, m, s);
            if (s_timeCallback) {
                s_timeCallback();
            }
        } else {
            Console::printf("ERROR: Failed to set time\r\n");
        }
    } else {
        Console::printf("ERROR: Failed to set time\r\n");
    }
}

static void cmdSetDate(const char* args) {
    if (!args || !*args) {
        Console::printf("Usage: SET_DATE DD.MM.YYYY\r\n");
        return;
    }
    int d, m, y;
    if (sscanf(args, "%d.%d.%d", &d, &m, &y) != 3) {
        Console::printf("ERROR: Invalid format. Use DD.MM.YYYY\r\n");
        return;
    }
    if (d < 1 || d > 31 || m < 1 || m > 12 || y < YEAR_MIN || y > YEAR_MAX) {
        Console::printf("ERROR: Invalid date values\r\n");
        return;
    }

    struct timeval tv;
    struct tm* tm;
    if (getCurrentTime(tv, tm)) {
        tm->tm_mday = d;
        tm->tm_mon = m - 1;
        tm->tm_year = y - 1900;
        if (setSystemTime(tm)) {
            Console::printf("OK: Date set to %02d.%02d.%04d\r\n", d, m, y);
            if (s_timeCallback) {
                s_timeCallback();
            }
        } else {
            Console::printf("ERROR: Failed to set date\r\n");
        }
    } else {
        Console::printf("ERROR: Failed to set date\r\n");
    }
}

// ============================================================================
// Command Handlers - Display
// ============================================================================

static void cmdSetName(const char* args) {
    if (!args) args = "";
    if (s_textCallback) {
        s_textCallback("name", args);
    }
    Console::printf("OK: Name set to \"%s\"\r\n", args);
}

static void cmdSetInfo(const char* args) {
    if (!args) args = "";
    if (s_textCallback) {
        s_textCallback("info", args);
    }
    Console::printf("OK: Info set to \"%s\"\r\n", args);
}

static void cmdSetInfo2(const char* args) {
    if (!args) args = "";
    if (s_textCallback) {
        s_textCallback("info2", args);
    }
    Console::printf("OK: Info2 set to \"%s\"\r\n", args);
}

// ============================================================================
// Command Handlers - Authentication
// ============================================================================

#if FEATURE_SECURE_SERIAL
static void cmdAuth(const char* args) {
    auto& pm = core::PinManager::instance();

    if (pm.isBadgeBlocked()) {
        if (pm.isLockoutActive()) {
            uint32_t remainingSec = pm.getLockoutRemainingMs() / 1000;
            Console::printf("ERROR: PIN locked. Wait %lu seconds.\r\n", (unsigned long)remainingSec);
        } else {
            Console::printf("ERROR: PIN permanently locked.\r\n");
        }
        return;
    }

    if (!args || !*args) {
        Console::printf("Usage: AUTH <pin>\r\n");
        Console::printf("Retries: %d\r\n", pm.getBadgeRetries());
        return;
    }

    if (SerialCmd::authenticate(args)) {
        Console::printf("OK: Authenticated\r\n");
    } else {
        uint8_t retries = pm.getBadgeRetries();
        if (retries == 0) {
            if (pm.isLockoutActive()) {
                uint32_t remainingSec = pm.getLockoutRemainingMs() / 1000;
                Console::printf("ERROR: Wrong PIN. Locked for %lu seconds.\r\n", (unsigned long)remainingSec);
            } else {
                Console::printf("ERROR: Wrong PIN. Permanently locked.\r\n");
            }
        } else {
            Console::printf("ERROR: Wrong PIN. %d retries remaining.\r\n", retries);
        }
    }
}

static void cmdLogout(const char* args) {
    (void)args;
    SerialCmd::logout();
    Console::printf("OK: Logged out\r\n");
}
#endif

// ============================================================================
// Command Handlers - PIN Debug
// ============================================================================

static void cmdPinReset(const char* args) {
    (void)args;
    core::PinManager::instance().resetBadgeRetries();
    Console::printf("OK: Badge PIN retries reset to %d\r\n",
                    core::PinManager::instance().getBadgeRetries());
}

static void cmdPinStatus(const char* args) {
    (void)args;
    auto& pm = core::PinManager::instance();
    Console::printf("Badge PIN: retries=%d blocked=%s set=%s\r\n",
                    pm.getBadgeRetries(),
                    pm.isBadgeBlocked() ? "yes" : "no",
                    pm.isPinSet() ? "yes" : "no");
}

// ============================================================================
// Command Handlers - TROPIC01 Secure Element
// ============================================================================

static void cmdTr01Status(const char* args) {
    (void)args;
    auto* se = getSecureElementWithCheck();
    if (!se) return;

    Console::printf("TR01 Status:\r\n");
    Console::printf("  Session: %s\r\n", se->isSessionActive() ? "active" : "inactive");
}

static void cmdTr01Info(const char* args) {
    (void)args;
    auto* se = getSecureElementWithCheck();
    if (!se) return;

    uint8_t chipId[8];
    uint8_t riscvVer = 0, spectVer = 0;

    Console::printf("TR01 Info:\r\n");

    if (se->getChipId(chipId, sizeof(chipId))) {
        Console::printf("  Chip ID: ");
        for (int i = 0; i < 8; i++) {
            Console::printf("%02X", chipId[i]);
        }
        Console::printf("\r\n");
    } else {
        Console::printf("  Chip ID: (read failed)\r\n");
    }

    if (se->getFwVersion(&riscvVer, &spectVer)) {
        Console::printf("  RISC-V FW: %d\r\n", riscvVer);
        Console::printf("  SPECT FW: %d\r\n", spectVer);
    } else {
        Console::printf("  FW Version: (read failed)\r\n");
    }
}

static void cmdTr01Session(const char* args) {
    (void)args;
    auto* se = getSecureElementWithCheck();
    if (!se) return;

    if (se->isSessionActive()) {
        Console::printf("Session already active, reconnecting...\r\n");
        se->sessionEnd();
    }

    if (se->sessionStart()) {
        Console::printf("OK: Session started\r\n");
    } else {
        Console::printf("ERROR: Session start failed\r\n");
    }
}

static void cmdTr01Slots(const char* args) {
    (void)args;
    auto* se = getSecureElementWithCheck();
    if (!se) return;

    Console::printf("ECC Key Slots (0-31):\r\n");
    int eccCount = 0;
    for (uint8_t i = 0; i < hal::ISecureElement::ECC_SLOT_COUNT; i++) {
        if (se->eccSlotUsed(i)) {
            Console::printf("  [%02d] Used\r\n", i);
            eccCount++;
        }
    }
    if (eccCount == 0) {
        Console::printf("  (none)\r\n");
    }

    Console::printf("\r\nR-Memory Slots summary:\r\n");
    Console::printf("  Slot 0:        System PIN/lockout\r\n");
    Console::printf("  Slots 1-31:    ECC paired (module-owned)\r\n");
    Console::printf("  Slots 32-131:  TOTP accounts\r\n");
    Console::printf("  Slots 132-511: Password vault\r\n");
}

static void cmdTr01RmemRead(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::RMEM_SLOT_COUNT, "R-Memory slot");
    if (!result.valid) {
        Console::printf("Usage: TR01_RMEM_READ <slot>\r\n");
        return;
    }

    auto* se = getSecureElementWithCheck();
    if (!se) return;

    uint16_t slot = static_cast<uint16_t>(result.value);
    uint8_t data[256];
    uint16_t actualLen = 0;

    hal::SeResult seResult = se->rmemRead(slot, data, sizeof(data), &actualLen);
    if (seResult != hal::SeResult::OK) {
        Console::printf("ERROR: Read failed (slot may be empty)\r\n");
        return;
    }

    Console::printf("R-Memory Slot %d (%d bytes):\r\n", slot, actualLen);
    printHexDump(data, actualLen, actualLen);
}

static void cmdTr01EccDel(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::ECC_SLOT_COUNT, "ECC slot");
    if (!result.valid) {
        Console::printf("Usage: TR01_ECC_DEL <slot>\r\n");
        return;
    }

    auto* se = getSecureElementWithCheck();
    if (!se) return;

    uint8_t slot = static_cast<uint8_t>(result.value);
    hal::SeResult seResult = se->eccDelete(slot);
    if (seResult == hal::SeResult::OK) {
        Console::printf("OK: ECC slot %d deleted\r\n", slot);
    } else {
        Console::printf("ERROR: Delete failed\r\n");
    }
}

static void cmdTr01RmemDel(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::RMEM_SLOT_COUNT, "R-Memory slot");
    if (!result.valid) {
        Console::printf("Usage: TR01_RMEM_DEL <slot>\r\n");
        return;
    }

    auto* se = getSecureElementWithCheck();
    if (!se) return;

    uint16_t slot = static_cast<uint16_t>(result.value);
    hal::SeResult seResult = se->rmemErase(slot);
    if (seResult == hal::SeResult::OK) {
        Console::printf("OK: R-Memory slot %d erased\r\n", slot);
    } else {
        Console::printf("ERROR: Erase failed\r\n");
    }
}

static void cmdTr01Resync(const char* args) {
    (void)args;
    auto* se = getSecureElementWithCheck();
    if (!se) return;

    Console::printf("Resyncing TR01 session...\r\n");

    if (se->isSessionActive()) {
        se->sessionEnd();
    }

    if (se->sessionStart()) {
        Console::printf("OK: Session restarted, cache invalidated\r\n");
    } else {
        Console::printf("ERROR: Session restart failed\r\n");
    }
}

static void cmdTr01CacheRebuild(const char* args) {
    (void)args;
    auto& storage = core::TropicStorage::instance();
    Console::printf("Rebuilding TR01 cache...\r\n");

    auto logFn = [](uint16_t slot, const char* message, void* ctx) {
        (void)ctx;
        if (!message) return;
        if (strcmp(message, "invalid header") == 0 ||
            strcmp(message, "mismatched module") == 0 ||
            strcmp(message, "nvs write failed") == 0 ||
            strcmp(message, "session start failed") == 0 ||
            strcmp(message, "read failed") == 0) {
            Console::printf("  slot %u: %s\r\n", slot, message);
        } else {
            Console::printf("  slot %u: found %s\r\n", slot, message);
        }
    };

    if (storage.rebuildVerbose(logFn, nullptr)) {
        Console::printf("OK: Cache rebuilt\r\n");
    } else {
        Console::printf("ERROR: Cache rebuild failed\r\n");
    }
}

static void cmdTr01Cleanup(const char* args) {
    (void)args;
    auto& storage = core::TropicStorage::instance();
    Console::printf("Cleaning TR01 cache + slots...\r\n");
    if (storage.cleanup()) {
        Console::printf("OK: Cleanup complete\r\n");
    } else {
        Console::printf("ERROR: Cleanup failed\r\n");
    }
}

static void cmdTr01Wipe(const char* args) {
    auto* se = getSecureElementWithCheck();
    if (!se) return;

    if (!args || strcmp(args, "CONFIRM") != 0) {
        Console::printf("WARNING: This will ERASE ALL data on TROPIC01!\r\n");
        Console::printf("  - All ECC keys (slots 0-31)\r\n");
        Console::printf("  - All R-Memory data (slots 0-511)\r\n");
        Console::printf("\r\nTo proceed, type: TR01_WIPE CONFIRM\r\n");
        return;
    }

    Console::printf("=== TROPIC01 Factory Reset ===\r\n");
    Console::flush();

    if (!se->isSessionActive()) {
        if (!se->sessionStart()) {
            Console::printf("ERROR: Cannot start session\r\n");
            return;
        }
    }

    uint16_t eccDeleted = 0;
    uint16_t rmemDeleted = 0;

    // Delete all ECC keys
    Console::printf("Erasing ECC keys...\r\n");
    Console::flush();
    for (uint8_t i = 0; i < hal::ISecureElement::ECC_SLOT_COUNT; i++) {
        if (se->eccSlotUsed(i)) {
            if (se->eccDelete(i) == hal::SeResult::OK) {
                eccDeleted++;
            }
        }
    }
    Console::printf("  Deleted %d ECC keys\r\n", eccDeleted);

    // Erase R-Memory slots
    Console::printf("Erasing R-Memory (this may take a while)...\r\n");
    Console::flush();
    for (uint16_t i = 0; i < hal::ISecureElement::RMEM_SLOT_COUNT; i++) {
        if ((i & (WIPE_PROGRESS_INTERVAL - 1)) == 0) {
            Console::printf("  Progress: %d/%d\r\n", i, hal::ISecureElement::RMEM_SLOT_COUNT);
            Console::flush();
        }
        if (se->rmemSlotUsed(i)) {
            if (se->rmemErase(i) == hal::SeResult::OK) {
                rmemDeleted++;
            }
        }
    }
    Console::printf("  Deleted %d R-Memory slots\r\n", rmemDeleted);

    Console::printf("\r\n=== Factory Reset Complete ===\r\n");
    Console::printf("Deleted: %d ECC keys, %d R-Memory slots\r\n", eccDeleted, rmemDeleted);
}

// ============================================================================
// SerialCmd Public Interface
// ============================================================================

void SerialCmd::init() {
    if (s_initialized) return;

    Console::init();

#if FEATURE_SECURE_SERIAL
    getCommandRegistry().setAuthProvider(isAuthenticated);
    getCommandRegistry().setOnCommandExecuted(resetAuthTimer);
#endif

    registerBuiltinCommands();

    s_initialized = true;
    LOG_I(TAG, "Serial command processor initialized");

    Console::printf("\r\n=== CDC Badge OS Serial Console ===\r\n");
#if FEATURE_SECURE_SERIAL
    Console::printf("Login with: AUTH <pin>\r\n");
#endif
    Console::printf("Type 'HELP' for available commands.\r\n");
    Console::showPrompt();
}

bool SerialCmd::process() {
    int c = Console::getchar();
    if (c < 0) return false;

    // Handle escape sequences (arrow keys)
    if (s_escState == EscState::ESC) {
        if (c == '[') {
            s_escState = EscState::BRACKET;
            return false;
        }
        s_escState = EscState::NONE;
    } else if (s_escState == EscState::BRACKET) {
        s_escState = EscState::NONE;
        switch (c) {
            case 'A':  // Arrow up - older command
                if (s_historyPos < s_historyCount) {
                    const char* hist = historyGet(s_historyPos);
                    if (hist) {
                        redrawLine(hist, s_cmdBufferPos);
                        s_historyPos++;
                    }
                }
                return false;

            case 'B':  // Arrow down - newer command
                if (s_historyPos > 0) {
                    s_historyPos--;
                    if (s_historyPos == 0) {
                        redrawLine("", s_cmdBufferPos);
                    } else {
                        const char* hist = historyGet(s_historyPos - 1);
                        if (hist) {
                            redrawLine(hist, s_cmdBufferPos);
                        }
                    }
                }
                return false;

            default:
                return false;
        }
    }

    // Handle special characters
    switch (c) {
        case 0x1B:  // ESC
            s_escState = EscState::ESC;
            return false;

        case '\r':
        case '\n':
            Console::print("\r\n");
            s_cmdBuffer[s_cmdBufferPos] = '\0';
            if (s_cmdBufferPos > 0) {
                historyAdd(s_cmdBuffer);
                executeCommand(s_cmdBuffer);
            }
            s_cmdBufferPos = 0;
            s_historyPos = 0;
            Console::showPrompt();
            return true;

        case 0x7F:  // Backspace (DEL)
        case 0x08:  // Backspace (BS)
            if (s_cmdBufferPos > 0) {
                s_cmdBufferPos--;
                Console::print("\b \b");
            }
            return false;

        case 0x03:  // Ctrl+C
            Console::print("^C\r\n");
            s_cmdBufferPos = 0;
            s_historyPos = 0;
            Console::showPrompt();
            return false;

        case 0x15:  // Ctrl+U
            while (s_cmdBufferPos > 0) {
                Console::print("\b \b");
                s_cmdBufferPos--;
            }
            return false;

        default:
            if (c >= 0x20 && c < 0x7F && s_cmdBufferPos < CMD_BUFFER_SIZE - 1) {
                s_cmdBuffer[s_cmdBufferPos++] = static_cast<char>(c);
                Console::putchar(static_cast<char>(c));
            }
            return false;
    }
}

ICommandRegistry& SerialCmd::getRegistry() {
    return getCommandRegistry();
}

void SerialCmd::setTextCallback(TextChangeCallback callback) {
    s_textCallback = callback;
}

void SerialCmd::setTimeCallback(TimeChangeCallback callback) {
    s_timeCallback = callback;
}

bool SerialCmd::isAuthenticated() {
#if FEATURE_SECURE_SERIAL
    if (!s_authenticated) return false;

    uint64_t now = esp_timer_get_time();
    if ((now - s_authTimestamp) > (AUTH_TIMEOUT_MS * 1000ULL)) {
        s_authenticated = false;
        LOG_I(TAG, "Session timed out");
        return false;
    }

    return true;
#else
    return true;
#endif
}

bool SerialCmd::authenticate(const char* pin) {
    auto& pm = core::PinManager::instance();

    if (pm.isBadgeBlocked()) {
        if (pm.isLockoutActive()) {
            uint32_t remainingSec = pm.getLockoutRemainingMs() / 1000;
            LOG_W(TAG, "PIN locked, %lu seconds remaining", (unsigned long)remainingSec);
        } else {
            LOG_W(TAG, "PIN permanently blocked (retries exhausted)");
        }
        return false;
    }

    if (!pin || !*pin) {
        LOG_W(TAG, "Empty PIN provided");
        return false;
    }

    if (!pm.verifyBadgePin(pin)) {
        LOG_W(TAG, "Authentication failed, %d retries remaining", pm.getBadgeRetries());
        return false;
    }

    s_authenticated = true;
    s_authTimestamp = esp_timer_get_time();
    LOG_I(TAG, "Authenticated via serial");
    return true;
}

void SerialCmd::logout() {
    s_authenticated = false;
    s_authTimestamp = 0;
    LOG_I(TAG, "Logged out");
}

void SerialCmd::executeCommand(char* cmd) {
    cmd = trim(cmd);
    if (!*cmd) return;

    LOG_D(TAG, "Executing: %s", cmd);
    getCommandRegistry().processCommand(cmd);
}

char* SerialCmd::trim(char* str) {
    if (!str) return str;

    while (*str && isspace(static_cast<unsigned char>(*str))) str++;
    if (*str == '\0') return str;

    char* end = str + strlen(str) - 1;
    while (end > str && isspace(static_cast<unsigned char>(*end))) end--;
    *(end + 1) = '\0';

    return str;
}

void SerialCmd::registerBuiltinCommands() {
    auto& reg = getCommandRegistry();

    // System commands
    reg.registerCommand({"HELP", "Show available commands", cmdHelp, "system", false});
    reg.registerCommand({"PING", "Check if device is responsive", cmdPing, "system", false});
    reg.registerCommand({"STATUS", "Show system status", cmdStatus, "system", false});
    reg.registerCommand({"MEM", "Show memory usage", cmdMem, "system", false});
    reg.registerCommand({"ERROR_LOG", "Show error log (CLEAR to reset)", cmdErrorLog, "system", false});
    reg.registerCommand({"REBOOT", "Restart the device", cmdReboot, "system", true});

    // NVS commands
    reg.registerCommand({"NVS_LIST", "List NVS entries [namespace]", cmdNvsList, "nvs", false});
    reg.registerCommand({"NVS_READ", "Read NVS key (ns key)", cmdNvsRead, "nvs", false});
    reg.registerCommand({"NVS_DEL", "Delete NVS key/namespace", cmdNvsDel, "nvs", true});
    reg.registerCommand({"NVS_CLEAR", "Erase entire NVS (NVS_CLEAR YES)", cmdNvsClear, "nvs", true});

    // Time commands
    reg.registerCommand({"GET_TIME", "Show current time", cmdGetTime, "time", false});
    reg.registerCommand({"GET_DATE", "Show current date", cmdGetDate, "time", false});
    reg.registerCommand({"SET_TIME", "Set time (HH:MM:SS)", cmdSetTime, "time", false});
    reg.registerCommand({"SET_DATE", "Set date (DD.MM.YYYY)", cmdSetDate, "time", false});

    // Display commands
    reg.registerCommand({"SET_NAME", "Set display name", cmdSetName, "display", false});
    reg.registerCommand({"SET_INFO", "Set info line 1", cmdSetInfo, "display", false});
    reg.registerCommand({"SET_INFO2", "Set info line 2", cmdSetInfo2, "display", false});

    // PIN debug commands
    reg.registerCommand({"PIN_STATUS", "Show PIN status", cmdPinStatus, "pin", false});
    reg.registerCommand({"PIN_RESET", "Reset PIN retries (debug)", cmdPinReset, "pin", false});

    // TROPIC01 Secure Element commands
    reg.registerCommand({"TR01_STATUS", "Show TR01 status", cmdTr01Status, "tr01", false});
    reg.registerCommand({"TR01_INFO", "Show TR01 chip info", cmdTr01Info, "tr01", false});
    reg.registerCommand({"TR01_SESSION", "Start/restart TR01 session", cmdTr01Session, "tr01", false});
    reg.registerCommand({"TR01_SLOTS", "Show TR01 slot usage", cmdTr01Slots, "tr01", false});
    reg.registerCommand({"TR01_RMEM_READ", "Read R-Memory slot", cmdTr01RmemRead, "tr01", false});
    reg.registerCommand({"TR01_ECC_DEL", "Delete ECC key slot", cmdTr01EccDel, "tr01", true});
    reg.registerCommand({"TR01_RMEM_DEL", "Delete R-Memory slot", cmdTr01RmemDel, "tr01", true});
    reg.registerCommand({"TR01_RESYNC", "Resync TR01 session and cache", cmdTr01Resync, "tr01", false});
    reg.registerCommand({"TR01_CACHE_REBUILD", "Rebuild TR01 cache", cmdTr01CacheRebuild, "tr01", false});
    reg.registerCommand({"TR01_CLEANUP", "Cleanup mismatched slots + rebuild cache", cmdTr01Cleanup, "tr01", true});
    reg.registerCommand({"TR01_WIPE", "Factory reset (TR01_WIPE CONFIRM)", cmdTr01Wipe, "tr01", true});

#if FEATURE_SECURE_SERIAL
    // Authentication commands
    reg.registerCommand({"AUTH", "Authenticate with PIN", cmdAuth, "auth", false});
    reg.registerCommand({"LOGOUT", "End authenticated session", cmdLogout, "auth", false});
#endif
}

} // namespace cdc::serial
