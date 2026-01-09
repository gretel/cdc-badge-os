// Serial Command Interface for CDC Badge
// Extended command set:
//   System: HELP, PING, STATUS
//   Time: SET_TIME, SET_DATE, GET_TIME, GET_DATE
//   Display: SET_NAME, SET_INFO, SET_INFO2
//   TOTP: TOTP_LIST, TOTP_ADD, TOTP_DEL, TOTP_GET, TOTP_TYPE
//   FIDO2: FIDO_STATUS, FIDO_LIST, FIDO_DEL, FIDO_RESET
//   TROPIC01: TR01_STATUS, TR01_SLOTS
//
// All I/O goes through cdc_log console functions (TinyUSB CDC)

#include "serial_cmd.h"
#include "cdc_rtc.h"
#include "cdc_log.h"
#include "feature_flags.h"
#include "tropic01.h"
#include "tropic01_cache.h"
#include "pin_storage.h"
#include "esp_timer.h"

#if FEATURE_TOTP
#include "totp_store.h"
#include "totp.h"
#endif

#if FEATURE_FIDO2
#include "fido2.h"
#include "ctaphid.h"
#endif

#if FEATURE_CA
#include "ca.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <time.h>

#define CMD_BUFFER_SIZE 256

static char cmd_buffer[CMD_BUFFER_SIZE];
static int cmd_buffer_pos = 0;
static serial_cmd_text_callback_t text_callback = nullptr;
static serial_cmd_time_callback_t time_callback = nullptr;

// ============================================================================
// Secure Serial Mode (compile-time flag FEATURE_SECURE_SERIAL)
// ============================================================================
#if FEATURE_SECURE_SERIAL
#define SERIAL_AUTH_TIMEOUT_MS (5 * 60 * 1000)  // 5 minute timeout

static bool g_serial_authenticated = false;
static int64_t g_serial_auth_time = 0;

static bool is_serial_authenticated(void) {
    if (!g_serial_authenticated) return false;

    // Check timeout
    int64_t now = esp_timer_get_time() / 1000;  // Convert to ms
    if ((now - g_serial_auth_time) > SERIAL_AUTH_TIMEOUT_MS) {
        g_serial_authenticated = false;
        LOG_I("CMD", "Serial auth timeout");
        return false;
    }
    return true;
}

static void serial_auth_success(void) {
    g_serial_authenticated = true;
    g_serial_auth_time = esp_timer_get_time() / 1000;
}

static void serial_logout(void) {
    g_serial_authenticated = false;
    g_serial_auth_time = 0;
}
#endif

// Show prompt
static void show_prompt(void) {
    console_print("> ");
    console_flush();
}

// Trim whitespace from string
static char* trim(char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    return str;
}

// Show help
static void show_help(void) {
    console_printf("=== CDC Badge Commands ===\r\n\r\n");

#if FEATURE_SECURE_SERIAL
    console_printf("Authentication:\r\n");
    console_printf("  AUTH <pin>           - Authenticate (5min timeout)\r\n");
    console_printf("  LOGOUT               - End authenticated session\r\n");
    console_printf("\r\n");
#endif

    console_printf("System:\r\n");
    console_printf("  HELP                 - Show this help\r\n");
    console_printf("  PING                 - Connection test\r\n");
    console_printf("  STATUS               - Badge status\r\n");
#if CDC_LOG_RING_BUFFER
    console_printf("  LOG_TAIL [n]         - Show last N log lines (max 100)\r\n");
#endif
    console_printf("\r\n");

    console_printf("Time:\r\n");
#if FEATURE_SECURE_SERIAL
    console_printf("  SET_TIME HH:MM:SS    - Set current time (auth req.)\r\n");
    console_printf("  SET_DATE YYYY-MM-DD  - Set current date (auth req.)\r\n");
    console_printf("  SET_DATE <timestamp> - Set from Unix timestamp (auth req.)\r\n");
#else
    console_printf("  SET_TIME HH:MM:SS    - Set current time\r\n");
    console_printf("  SET_DATE YYYY-MM-DD  - Set current date\r\n");
    console_printf("  SET_DATE <timestamp> - Set from Unix timestamp\r\n");
#endif
    console_printf("  GET_TIME             - Get current time\r\n");
    console_printf("  GET_DATE             - Get current date\r\n");
    console_printf("\r\n");

    console_printf("Display:\r\n");
#if FEATURE_SECURE_SERIAL
    console_printf("  SET_NAME text        - Set name on display (auth req.)\r\n");
    console_printf("  SET_INFO text        - Set info line (auth req.)\r\n");
    console_printf("  SET_INFO2 text       - Set info2 line (auth req.)\r\n");
#else
    console_printf("  SET_NAME text        - Set name on display\r\n");
    console_printf("  SET_INFO text        - Set info line\r\n");
    console_printf("  SET_INFO2 text       - Set info2 line\r\n");
#endif
    console_printf("\r\n");

#if FEATURE_TOTP
    console_printf("TOTP:\r\n");
    console_printf("  TOTP_LIST            - List all accounts\r\n");
#if FEATURE_SECURE_SERIAL
    console_printf("  TOTP_ADD ...         - Add account (auth req.)\r\n");
    console_printf("  TOTP_DEL <index>     - Delete account (auth req.)\r\n");
#else
    console_printf("  TOTP_ADD name secret [issuer] [digits] [period] [algo]\r\n");
    console_printf("                       - Add account (algo: sha1/sha256/sha512)\r\n");
    console_printf("  TOTP_DEL <index>     - Delete account\r\n");
#endif
    console_printf("  TOTP_GET <index>     - Generate code\r\n");
    console_printf("  TOTP_TYPE <index> [enter] - Type code via keyboard\r\n");
    console_printf("\r\n");
#endif

#if FEATURE_FIDO2
    console_printf("FIDO2:\r\n");
    console_printf("  FIDO_STATUS          - FIDO2 module status\r\n");
    console_printf("  FIDO_LIST            - List all credentials\r\n");
#if FEATURE_SECURE_SERIAL
    console_printf("  FIDO_DEL <index>     - Delete credential (auth req.)\r\n");
    console_printf("  FIDO_RESET           - Factory reset (auth req.)\r\n");
#else
    console_printf("  FIDO_DEL <index>     - Delete credential\r\n");
    console_printf("  FIDO_RESET           - Factory reset (deletes all!)\r\n");
#endif
    console_printf("\r\n");
#endif

    console_printf("TROPIC01:\r\n");
    console_printf("  TR01_STATUS          - Secure element status\r\n");
    console_printf("  TR01_INFO            - Chip ID and firmware version\r\n");
    console_printf("  TR01_SESSION         - Start/reconnect secure session\r\n");
    console_printf("  TR01_SLOTS           - Show slot usage\r\n");
    console_printf("  TR01_RESYNC          - Resync cache from chip\r\n");
    console_printf("  TR01_RMEM_READ <slot>- Read and dump R-memory slot\r\n");
#if FEATURE_SECURE_SERIAL
    console_printf("  TR01_ECC_DEL <slot>  - Delete ECC key (auth req.)\r\n");
    console_printf("  TR01_RMEM_DEL <slot> - Delete R-memory slot (auth req.)\r\n");
    console_printf("  TR01_WIPE            - Factory reset (CONFIRM req., auth req.)\r\n");
#else
    console_printf("  TR01_ECC_DEL <slot>  - Delete ECC key\r\n");
    console_printf("  TR01_RMEM_DEL <slot> - Delete R-memory slot\r\n");
    console_printf("  TR01_WIPE            - Factory reset (requires CONFIRM)\r\n");
#endif
    console_printf("\r\n");

#if FEATURE_CA
    console_printf("CA (Certificate Authority):\r\n");
    console_printf("  CA_STATUS            - CA status\r\n");
#if FEATURE_SECURE_SERIAL
    console_printf("  CA_INIT [cn]         - Initialize CA (auth req.)\r\n");
    console_printf("  CA_IMPORT            - Import existing CA (auth req.)\r\n");
    console_printf("  CA_SIGN_CSR          - Sign CSR (auth req., paste PEM)\r\n");
    console_printf("  CA_SIGN_CERT         - Re-sign cert (auth req., paste PEM)\r\n");
    console_printf("  CA_RESET             - Factory reset CA (auth req.)\r\n");
#else
    console_printf("  CA_INIT [cn]         - Initialize CA\r\n");
    console_printf("  CA_IMPORT            - Import existing CA (key+cert PEM)\r\n");
    console_printf("  CA_SIGN_CSR          - Sign CSR (paste PEM, end with ---)\r\n");
    console_printf("  CA_SIGN_CERT         - Re-sign certificate (paste PEM)\r\n");
    console_printf("  CA_RESET             - Factory reset CA\r\n");
#endif
    console_printf("  CA_LIST              - List issued certificates\r\n");
    console_printf("  CA_EXPORT_ROOT       - Export root certificate (PEM)\r\n");
    console_printf("\r\n");
#endif

    console_flush();
}

// ============================================================================
// Authentication Commands (only when FEATURE_SECURE_SERIAL)
// ============================================================================

#if FEATURE_SECURE_SERIAL
static void cmd_auth(char *args) {
    char *pin = trim(args);
    if (strlen(pin) == 0) {
        console_printf("ERROR: Usage: AUTH <pin>\r\n");
        return;
    }

    if (pin_storage_verify(pin)) {
        serial_auth_success();
        console_printf("OK: Authenticated (timeout: 5min)\r\n");
    } else {
        console_printf("ERROR: Invalid PIN\r\n");
    }
}

static void cmd_logout(void) {
    serial_logout();
    console_printf("OK: Logged out\r\n");
}

// Check if command requires authentication
// Returns true if auth required and NOT authenticated
static bool check_auth_required(const char *cmd) {
    // Always allowed commands (no auth required)
    if (strcasecmp(cmd, "HELP") == 0) return false;
    if (strcasecmp(cmd, "PING") == 0) return false;
    if (strcasecmp(cmd, "STATUS") == 0) return false;
    if (strcasecmp(cmd, "LOG_TAIL") == 0) return false;
    if (strncasecmp(cmd, "LOG_TAIL ", 9) == 0) return false;
    if (strcasecmp(cmd, "GET_TIME") == 0) return false;
    if (strcasecmp(cmd, "GET_DATE") == 0) return false;
    if (strcasecmp(cmd, "TR01_STATUS") == 0) return false;
    if (strcasecmp(cmd, "TR01_INFO") == 0) return false;
    if (strcasecmp(cmd, "TR01_SESSION") == 0) return false;
    if (strcasecmp(cmd, "TR01_SLOTS") == 0) return false;
    if (strcasecmp(cmd, "TR01_RESYNC") == 0) return false;
    if (strncasecmp(cmd, "AUTH ", 5) == 0) return false;
    if (strcasecmp(cmd, "LOGOUT") == 0) return false;
#if FEATURE_TOTP
    if (strcasecmp(cmd, "TOTP_LIST") == 0) return false;
    if (strncasecmp(cmd, "TOTP_GET ", 9) == 0) return false;
#endif
#if FEATURE_FIDO2
    if (strcasecmp(cmd, "FIDO_STATUS") == 0) return false;
    if (strcasecmp(cmd, "FIDO_LIST") == 0) return false;
#endif

    // All other commands require auth
    return !is_serial_authenticated();
}
#endif

// ============================================================================
// System Commands
// ============================================================================

static void cmd_ping(void) {
    console_printf("PONG\r\n");
}

static void cmd_status(void) {
    console_printf("=== Badge Status ===\r\n");

    // Time
    char time_str[16], date_str[16];
    cdc_rtc_get_time_str(time_str, sizeof(time_str));
    cdc_rtc_get_date_str(date_str, sizeof(date_str));
    console_printf("Time: %s %s\r\n", date_str, time_str);
#if FEATURE_TOTP
    console_printf("Time valid: %s\r\n", totp_time_valid() ? "yes" : "no (sync required)");
#endif

    // TROPIC01
    console_printf("TROPIC01 session: %s\r\n", tropic01_session_active() ? "active" : "inactive");
    console_printf("Cache loaded: %s\r\n", tropic01_cache_is_loaded() ? "yes" : "no");

#if FEATURE_TOTP
    console_printf("TOTP accounts: %d/%d\r\n", totp_store_count(), TOTP_MAX_ACCOUNTS);
#endif

#if FEATURE_FIDO2
    console_printf("FIDO2 credentials: %d/%d\r\n", fido2_get_credential_count(), FIDO2_MAX_CREDENTIALS);
    console_printf("FIDO2 initialized: %s\r\n", fido2_is_initialized() ? "yes" : "no");
#endif

    console_flush();
}

static void cmd_log_tail(char *args) {
#if CDC_LOG_RING_BUFFER
    int lines = 100;
    char *p = trim(args);
    if (strlen(p) > 0) {
        long requested = strtol(p, NULL, 10);
        if (requested > 0) {
            lines = (requested > 100) ? 100 : (int)requested;
        }
    }

    console_printf("=== Recent Logs (last %d) ===\r\n", lines);
    log_dump_recent((size_t)lines);
    console_flush();
#else
    (void)args;
    console_printf("ERROR: LOG_TAIL disabled (FEATURE_LOG_RING_BUFFER=0)\r\n");
#endif
}

// ============================================================================
// TOTP Commands
// ============================================================================

#if FEATURE_TOTP

static void cmd_totp_list(void) {
    uint8_t count = totp_store_count();
    if (count == 0) {
        console_printf("No TOTP accounts configured.\r\n");
        console_printf("Use TOTP_ADD to add accounts.\r\n");
        return;
    }

    console_printf("=== TOTP Accounts (%d) ===\r\n", count);
    for (uint8_t i = 0; i < count; i++) {
        totp_account_info_t info;
        if (totp_store_get_info(i, &info)) {
            const char *algo = "SHA1";
            if (info.algorithm == TOTP_ALG_SHA256) algo = "SHA256";
            else if (info.algorithm == TOTP_ALG_SHA512) algo = "SHA512";

            console_printf("[%d] %s", i, info.name);
            if (strlen(info.issuer) > 0) {
                console_printf(" (%s)", info.issuer);
            }
            console_printf(" - %d digits, %lus, %s\r\n", info.digits, info.period, algo);
        }
    }
    console_flush();
}

static void cmd_totp_add(char *args) {
    // Parse: name secret [issuer] [digits] [period] [algo]
    char name[32] = {0};
    char secret[64] = {0};
    char issuer[32] = {0};
    int digits = 6;
    int period = 30;
    char algo_str[8] = "sha1";
    uint8_t algorithm = TOTP_ALG_SHA1;

    // Parse arguments (space-separated)
    int parsed = sscanf(args, "%31s %63s %31s %d %d %7s",
                        name, secret, issuer, &digits, &period, algo_str);

    if (parsed < 2) {
        console_printf("ERROR: Usage: TOTP_ADD name secret [issuer] [digits] [period] [algo]\r\n");
        console_printf("  digits: 6, 7, or 8 (default: 6)\r\n");
        console_printf("  period: seconds (default: 30)\r\n");
        console_printf("  algo: sha1, sha256, sha512 (default: sha1)\r\n");
        return;
    }

    // Parse algorithm
    if (strcasecmp(algo_str, "sha256") == 0) {
        algorithm = TOTP_ALG_SHA256;
    } else if (strcasecmp(algo_str, "sha512") == 0) {
        algorithm = TOTP_ALG_SHA512;
    }

    // Validate
    if (digits < 6 || digits > 8) digits = 6;
    if (period < 1) period = 30;

    int8_t result = totp_store_add(name, issuer, secret, digits, period, algorithm);
    if (result >= 0) {
        console_printf("OK: Account '%s' added at index %d\r\n", name, result);
    } else {
        console_printf("ERROR: Failed to add account (invalid secret or storage full)\r\n");
    }
}

static void cmd_totp_del(char *args) {
    int index;
    if (sscanf(args, "%d", &index) != 1 || index < 0) {
        console_printf("ERROR: Usage: TOTP_DEL <index>\r\n");
        return;
    }

    if ((uint8_t)index >= totp_store_count()) {
        console_printf("ERROR: Invalid index (have %d accounts)\r\n", totp_store_count());
        return;
    }

    totp_account_info_t info;
    if (totp_store_get_info(index, &info)) {
        if (totp_store_delete(index)) {
            console_printf("OK: Deleted account '%s'\r\n", info.name);
        } else {
            console_printf("ERROR: Failed to delete account\r\n");
        }
    } else {
        console_printf("ERROR: Account not found\r\n");
    }
}

static void cmd_totp_get(char *args) {
    int index;
    if (sscanf(args, "%d", &index) != 1 || index < 0) {
        console_printf("ERROR: Usage: TOTP_GET <index>\r\n");
        return;
    }

    if ((uint8_t)index >= totp_store_count()) {
        console_printf("ERROR: Invalid index (have %d accounts)\r\n", totp_store_count());
        return;
    }

    char code[12];
    int8_t remaining = totp_store_generate_code(index, code);
    if (remaining >= 0) {
        totp_account_info_t info;
        totp_store_get_info(index, &info);
        console_printf("%s: %s (expires in %ds)\r\n", info.name, code, remaining);
    } else {
        console_printf("ERROR: Failed to generate code (time not synced?)\r\n");
    }
}

static void cmd_totp_type(char *args) {
    int index;
    char enter_str[8] = {0};
    int parsed = sscanf(args, "%d %7s", &index, enter_str);

    if (parsed < 1 || index < 0) {
        console_printf("ERROR: Usage: TOTP_TYPE <index> [enter]\r\n");
        return;
    }

    if ((uint8_t)index >= totp_store_count()) {
        console_printf("ERROR: Invalid index (have %d accounts)\r\n", totp_store_count());
        return;
    }

    bool press_enter = (strcasecmp(enter_str, "enter") == 0);

    if (totp_store_type_code(index, press_enter)) {
        console_printf("OK: Code typed%s\r\n", press_enter ? " (with Enter)" : "");
    } else {
        console_printf("ERROR: Failed to type code (USB not ready or time not synced)\r\n");
    }
}

#endif // FEATURE_TOTP

// ============================================================================
// FIDO2 Commands
// ============================================================================

#if FEATURE_FIDO2

// FIDO reset confirmation state
static bool g_fido_reset_pending = false;

static void cmd_fido_status(void) {
    console_printf("=== FIDO2 Status ===\r\n");
    console_printf("Initialized: %s\r\n", fido2_is_initialized() ? "yes" : "no");
    console_printf("Credentials: %d/%d\r\n", fido2_get_credential_count(), FIDO2_MAX_CREDENTIALS);
    console_printf("Available slots: %d\r\n", fido2_get_available_slots());
    console_printf("Auth counter: %lu\r\n", fido2_get_auth_counter());
    uint32_t cbor_count = 0, msg_count = 0;
    ctaphid_get_cmd_counts(&cbor_count, &msg_count);
    console_printf("CTAPHID cmds: CBOR=%lu, MSG=%lu\r\n",
                   (unsigned long)cbor_count, (unsigned long)msg_count);
    console_flush();
}

static void cmd_fido_list(void) {
    uint8_t count = fido2_get_credential_count();
    if (count == 0) {
        console_printf("No FIDO2 credentials stored.\r\n");
        console_printf("Register credentials via WebAuthn in your browser.\r\n");
        return;
    }

    console_printf("=== FIDO2 Credentials (%d) ===\r\n", count);
    for (uint8_t i = 0; i < count; i++) {
        fido2_credential_info_t info;
        if (fido2_get_credential_info(i, &info)) {
            console_printf("[%d] %s", i, info.rp_id);
            if (strlen(info.user_name) > 0) {
                console_printf(" - %s", info.user_name);
            }
            console_printf(" (slot %d, %lu auths%s)\r\n",
                   info.slot, info.sign_count,
                   info.resident_key ? ", resident" : "");
        }
    }
    console_flush();
}

static void cmd_fido_del(char *args) {
    int index;
    if (sscanf(args, "%d", &index) != 1 || index < 0) {
        console_printf("ERROR: Usage: FIDO_DEL <index>\r\n");
        return;
    }

    if ((uint8_t)index >= fido2_get_credential_count()) {
        console_printf("ERROR: Invalid index (have %d credentials)\r\n", fido2_get_credential_count());
        return;
    }

    fido2_credential_info_t info;
    if (fido2_get_credential_info(index, &info)) {
        if (fido2_delete_credential(info.slot)) {
            console_printf("OK: Deleted credential for '%s'\r\n", info.rp_id);
        } else {
            console_printf("ERROR: Failed to delete credential\r\n");
        }
    } else {
        console_printf("ERROR: Credential not found\r\n");
    }
}

static void cmd_fido_reset(void) {
    uint8_t count = fido2_get_credential_count();
    if (count == 0) {
        console_printf("No credentials to reset.\r\n");
        return;
    }

    if (!g_fido_reset_pending) {
        console_printf("WARNING: This will delete ALL %d FIDO2 credentials!\r\n", count);
        console_printf("Type 'CONFIRM' to proceed, anything else to cancel.\r\n");
        console_flush();
        g_fido_reset_pending = true;
        return;
    }

    console_printf("ERROR: Type 'CONFIRM' to proceed with reset\r\n");
}

#endif // FEATURE_FIDO2

// ============================================================================
// TROPIC01 Commands
// ============================================================================

static void cmd_tr01_status(void) {
    console_printf("=== TROPIC01 Status ===\r\n");
    console_printf("Session: %s\r\n", tropic01_session_active() ? "active" : "inactive");
    console_printf("Cache: %s\r\n", tropic01_cache_is_loaded() ? "loaded" : "not loaded");
    console_printf("ECC keys in cache: %d\r\n", tropic01_cache_ecc_count());
    console_flush();
}

static void cmd_tr01_session(void) {
    if (tropic01_session_active()) {
        console_printf("Session active - reconnecting...\r\n");
        tropic01_session_abort();
    } else {
        console_printf("Session inactive - starting...\r\n");
    }

    if (tropic01_session_start()) {
        console_printf("OK: Session active\r\n");
    } else {
        console_printf("ERROR: Session start failed\r\n");
    }
    console_flush();
}

static void cmd_tr01_slots(void) {
    console_printf("=== TROPIC01 Slot Usage ===\r\n\r\n");

    console_printf("ECC Keys (0-31):\r\n");
    for (int i = 0; i < TR01_ECC_SLOT_COUNT; i++) {
        if (tropic01_cache_ecc_exists(i)) {
            uint8_t pubkey[64], curve;
            tropic01_cache_ecc_get_pubkey(i, pubkey, &curve);
            const char *curve_name = (curve == CDC_CURVE_P256) ? "P-256" : "Ed25519";
            console_printf("  [%2d] %s", i, curve_name);
            if (i <= TR01_ECC_SLOT_FIDO_END) {
                console_printf(" (FIDO2)");
            } else if (i == TR01_ECC_SLOT_ATTEST) {
                console_printf(" (Attestation)");
            } else if (i == 31) {
                console_printf(" (CA Root)");
            }
            console_printf("\r\n");
        }
    }

    console_printf("\r\nR-Memory:\r\n");

    // FIDO2 slots
    uint16_t fido_count = tropic01_cache_rmem_count_range(TR01_RMEM_SLOT_FIDO_START, TR01_RMEM_SLOT_FIDO_END);
    console_printf("  FIDO2 (0-26): %d used\r\n", fido_count);

    // PIN slot
    console_printf("  PIN (30): %s\r\n", tropic01_cache_rmem_exists(TR01_RMEM_SLOT_PIN) ? "set" : "empty");

    // TOTP slots
    uint16_t totp_count = tropic01_cache_rmem_count_range(TR01_RMEM_SLOT_TOTP_START, TR01_RMEM_SLOT_TOTP_END);
    console_printf("  TOTP (33-132): %d used\r\n", totp_count);

    // CA slot
#if FEATURE_CA
    console_printf("  CA (133): %s\r\n", tropic01_cache_rmem_exists(133) ? "set" : "empty");
#endif

    console_flush();
}

static void cmd_tr01_info(void) {
    console_printf("=== TROPIC01 Info ===\r\n");

    // Chip ID (serial number)
    uint8_t chip_id[32];
    if (tropic01_get_chip_id(chip_id, sizeof(chip_id))) {
        console_printf("Chip ID: ");
        for (int i = 0; i < 8; i++) {
            console_printf("%02X", chip_id[i]);
        }
        console_printf("\r\n");
    } else {
        console_printf("Chip ID: (read failed)\r\n");
    }

    // Firmware version
    uint8_t riscv_ver = 0, spect_ver = 0;
    if (tropic01_get_fw_version(&riscv_ver, &spect_ver)) {
        console_printf("RISC-V FW: v%d\r\n", riscv_ver);
        console_printf("SPECT FW: v%d\r\n", spect_ver);
    } else {
        console_printf("FW Version: (read failed)\r\n");
    }

    console_flush();
}

static void cmd_tr01_resync(void) {
    console_printf("Resyncing TROPIC01 cache...\r\n");
    console_flush();
    tropic01_cache_init();
    console_printf("OK: Cache resynced (%d ECC keys, %d TOTP accounts)\r\n",
           tropic01_cache_ecc_count(),
           tropic01_cache_rmem_count_range(TR01_RMEM_SLOT_TOTP_START, TR01_RMEM_SLOT_TOTP_END));
}

static void cmd_tr01_ecc_del(char *args) {
    int slot;
    if (sscanf(args, "%d", &slot) != 1 || slot < 0 || slot >= TR01_ECC_SLOT_COUNT) {
        console_printf("ERROR: Usage: TR01_ECC_DEL <slot> (0-%d)\r\n", TR01_ECC_SLOT_COUNT - 1);
        return;
    }

    // Warn if slot appears empty (cache may be out of sync)
    if (!tropic01_cache_ecc_exists(slot)) {
        console_printf("NOTE: Slot %d appears empty in cache\r\n", slot);
    }

    // Warn about reserved slots
    if (slot <= TR01_ECC_SLOT_FIDO_END) {
        console_printf("WARNING: Slot %d is used by FIDO2\r\n", slot);
    } else if (slot == TR01_ECC_SLOT_ATTEST) {
        console_printf("WARNING: Slot %d is the FIDO2 attestation key\r\n", slot);
    } else if (slot == 31) {
        console_printf("WARNING: Slot %d is the CA root key\r\n", slot);
    }

    // Always try to delete (in case cache is out of sync with chip)
    if (tropic01_ecc_key_erase(slot)) {
        tropic01_cache_ecc_invalidate(slot);
        console_printf("OK: ECC key at slot %d deleted\r\n", slot);
    } else {
        console_printf("ERROR: Failed to delete ECC key\r\n");
    }
}

static void cmd_tr01_rmem_del(char *args) {
    int slot;
    if (sscanf(args, "%d", &slot) != 1 || slot < 0 || slot > 255) {
        console_printf("ERROR: Usage: TR01_RMEM_DEL <slot> (0-255)\r\n");
        return;
    }

    // Warn if slot appears empty (cache may be out of sync)
    if (!tropic01_cache_rmem_exists(slot)) {
        console_printf("NOTE: Slot %d appears empty in cache\r\n", slot);
    }

    // Warn about reserved slots
    if (slot >= TR01_RMEM_SLOT_FIDO_START && slot <= TR01_RMEM_SLOT_FIDO_END) {
        console_printf("WARNING: Slot %d is used by FIDO2\r\n", slot);
    } else if (slot == TR01_RMEM_SLOT_PIN) {
        console_printf("WARNING: Slot %d is the device PIN\r\n", slot);
    } else if (slot >= TR01_RMEM_SLOT_TOTP_START && slot <= TR01_RMEM_SLOT_TOTP_END) {
        console_printf("WARNING: Slot %d is used by TOTP\r\n", slot);
    }

    // Always try to delete (in case cache is out of sync with chip)
    if (tropic01_rmem_erase(slot)) {
        tropic01_cache_rmem_invalidate(slot);
        console_printf("OK: R-memory slot %d deleted\r\n", slot);
    } else {
        console_printf("ERROR: Failed to delete R-memory slot\r\n");
    }
}

static void cmd_tr01_rmem_read(char *args) {
    int slot;
    if (sscanf(args, "%d", &slot) != 1 || slot < 0 || slot > 255) {
        console_printf("ERROR: Usage: TR01_RMEM_READ <slot> (0-255)\r\n");
        return;
    }

    // Check if session is active
    if (!tropic01_session_active()) {
        console_printf("ERROR: TROPIC01 session not active\r\n");
        return;
    }

    // Read from R-Memory (max 256 bytes)
    uint8_t data[256];
    uint16_t read_size = 0;

    if (!tropic01_rmem_read(slot, data, sizeof(data), &read_size)) {
        console_printf("ERROR: Failed to read R-memory slot %d\r\n", slot);
        return;
    }

    if (read_size == 0) {
        console_printf("Slot %d: empty (0 bytes)\r\n", slot);
        return;
    }

    console_printf("=== R-Memory Slot %d ===\r\n", slot);
    console_printf("Size: %d bytes\r\n", read_size);
    console_printf("Hex:\r\n");

    // Print hex dump
    for (uint16_t i = 0; i < read_size; i++) {
        if (i % 16 == 0) {
            console_printf("  %04X: ", i);
        }
        console_printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0 || i == read_size - 1) {
            console_printf("\r\n");
        }
    }
    console_flush();
}

// Wipe confirmation state
static bool g_wipe_pending = false;

// CA R-memory slot (if not defined elsewhere)
#ifndef TR01_RMEM_SLOT_CA
#define TR01_RMEM_SLOT_CA 133
#endif

static void cmd_tr01_wipe(void) {
    // Require explicit CONFIRM command after TR01_WIPE
    if (!g_wipe_pending) {
        console_printf("=== TROPIC01 Factory Reset ===\r\n\r\n");
        console_printf("WARNING: This will DELETE ALL user data:\r\n");
        console_printf("  - All FIDO2 credentials (ECC 0-26, R-mem 0-26)\r\n");
        console_printf("  - FIDO2 Attestation key (ECC 30)\r\n");
        console_printf("  - All TOTP accounts (R-mem 33-132)\r\n");
        console_printf("  - Device PIN (R-mem 30)\r\n");
        console_printf("  - CA Root key (ECC 31) and CA data (R-mem 133)\r\n");
        console_printf("\r\nDevice pairing keys are NOT affected.\r\n");
        console_printf("\r\nType 'CONFIRM' to proceed, anything else to cancel.\r\n");
        console_flush();
        g_wipe_pending = true;
        return;
    }

    // This should not be called directly - handle via cmd_confirm
    console_printf("ERROR: Type 'CONFIRM' to proceed with wipe\r\n");
}

static void cmd_confirm(void) {
    #if FEATURE_FIDO2
    if (!g_wipe_pending && !g_fido_reset_pending) {
        console_printf("ERROR: Nothing to confirm\r\n");
        return;
    }
    #else
    if (!g_wipe_pending) {
        console_printf("ERROR: Nothing to confirm\r\n");
        return;
    }
    #endif

    if (g_wipe_pending) {
        g_wipe_pending = false;
        console_printf("Wiping all user data...\r\n");
        console_flush();

        int ecc_deleted = 0, rmem_deleted = 0;

        // Erase FIDO2 ECC keys (0-26)
        for (int i = TR01_ECC_SLOT_FIDO_START; i <= TR01_ECC_SLOT_FIDO_END; i++) {
            if (tropic01_cache_ecc_exists(i)) {
                if (tropic01_ecc_key_erase(i)) {
                    tropic01_cache_ecc_invalidate(i);
                    ecc_deleted++;
                }
            }
        }

        // Erase FIDO2 Attestation key (30)
        if (tropic01_cache_ecc_exists(TR01_ECC_SLOT_ATTEST)) {
            if (tropic01_ecc_key_erase(TR01_ECC_SLOT_ATTEST)) {
                tropic01_cache_ecc_invalidate(TR01_ECC_SLOT_ATTEST);
                ecc_deleted++;
            }
        }

        // Erase CA Root key (31)
        if (tropic01_cache_ecc_exists(31)) {
            if (tropic01_ecc_key_erase(31)) {
                tropic01_cache_ecc_invalidate(31);
                ecc_deleted++;
            }
        }

        // Erase FIDO2 R-memory (0-26)
        for (int i = TR01_RMEM_SLOT_FIDO_START; i <= TR01_RMEM_SLOT_FIDO_END; i++) {
            if (tropic01_cache_rmem_exists(i)) {
                if (tropic01_rmem_erase(i)) {
                    tropic01_cache_rmem_invalidate(i);
                    rmem_deleted++;
                }
            }
        }

        // Erase PIN (30)
        if (tropic01_cache_rmem_exists(TR01_RMEM_SLOT_PIN)) {
            if (tropic01_rmem_erase(TR01_RMEM_SLOT_PIN)) {
                tropic01_cache_rmem_invalidate(TR01_RMEM_SLOT_PIN);
                rmem_deleted++;
            }
        }

        // Erase TOTP accounts (33-132)
        for (int i = TR01_RMEM_SLOT_TOTP_START; i <= TR01_RMEM_SLOT_TOTP_END; i++) {
            if (tropic01_cache_rmem_exists(i)) {
                if (tropic01_rmem_erase(i)) {
                    tropic01_cache_rmem_invalidate(i);
                    rmem_deleted++;
                }
            }
        }

        // Erase CA data (133)
        if (tropic01_cache_rmem_exists(TR01_RMEM_SLOT_CA)) {
            if (tropic01_rmem_erase(TR01_RMEM_SLOT_CA)) {
                tropic01_cache_rmem_invalidate(TR01_RMEM_SLOT_CA);
                rmem_deleted++;
            }
        }

        console_printf("OK: Wiped %d ECC keys, %d R-memory slots\r\n", ecc_deleted, rmem_deleted);
        return;
    }

    #if FEATURE_FIDO2
    g_fido_reset_pending = false;
    console_printf("Resetting all FIDO2 credentials...\r\n");
    console_flush();
    if (fido2_factory_reset()) {
        console_printf("OK: All credentials deleted\r\n");
    } else {
        console_printf("ERROR: Reset failed\r\n");
    }
    #endif
}

static void cmd_cancel(void) {
    if (g_wipe_pending) {
        g_wipe_pending = false;
        console_printf("Wipe cancelled.\r\n");
    #if FEATURE_FIDO2
    } else if (g_fido_reset_pending) {
        g_fido_reset_pending = false;
        console_printf("FIDO2 reset cancelled.\r\n");
    #endif
    } else {
        console_printf("Nothing to cancel.\r\n");
    }
}

// ============================================================================
// CA Commands
// ============================================================================

#if FEATURE_CA

static void cmd_ca_status(void) {
    ca_status_t status;
    if (!ca_get_status(&status)) {
        console_printf("ERROR: Failed to get CA status\r\n");
        return;
    }

    console_printf("=== CA Status ===\r\n");
    console_printf("Initialized: %s\r\n", status.initialized ? "yes" : "no");

    if (status.initialized) {
        console_printf("Common Name: %s\r\n", status.common_name);
        console_printf("Serial counter: %lu\r\n", (unsigned long)status.serial_counter);
        console_printf("Certificates issued: %lu\r\n", (unsigned long)status.issued_count);

        // Format timestamps
        struct tm *tm_info;
        char time_buf[32];

        time_t created = status.created_at;
        tm_info = localtime(&created);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
        console_printf("Created: %s\r\n", time_buf);

        time_t valid = status.valid_until;
        tm_info = localtime(&valid);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
        console_printf("Valid until: %s\r\n", time_buf);
    }

    console_flush();
}

static void cmd_ca_init(char *args) {
    char *cn = trim(args);

    if (strlen(cn) == 0) {
        cn = (char*)"CDC Badge Root CA";  // Default CN
    }

    if (ca_is_initialized()) {
        console_printf("ERROR: CA already initialized. Use CA_RESET first.\r\n");
        return;
    }

    console_printf("Initializing CA with CN: %s\r\n", cn);
    console_printf("Generating root key in TROPIC01...\r\n");
    console_flush();

    if (ca_setup(cn)) {
        console_printf("OK: CA initialized successfully\r\n");
    } else {
        console_printf("ERROR: CA initialization failed\r\n");
    }
}

static void cmd_ca_list(void) {
    if (!ca_is_initialized()) {
        console_printf("CA not initialized. Use CA_INIT first.\r\n");
        return;
    }

    uint32_t count = ca_get_issued_count();
    if (count == 0) {
        console_printf("No certificates issued yet.\r\n");
        return;
    }

    console_printf("=== Issued Certificates (%lu) ===\r\n", (unsigned long)count);
    for (uint32_t i = 0; i < count; i++) {
        ca_issued_cert_t cert;
        if (ca_get_issued_cert(i, &cert)) {
            console_printf("[%lu] Serial: %lu, CN: %s%s\r\n",
                   (unsigned long)i,
                   (unsigned long)cert.serial,
                   cert.subject_cn,
                   cert.revoked ? " [REVOKED]" : "");
        }
    }
    console_flush();
}

static void cmd_ca_export_root(void) {
    if (!ca_is_initialized()) {
        console_printf("ERROR: CA not initialized\r\n");
        return;
    }

    static char pem_buf[2048];
    size_t pem_len;

    if (ca_export_root_cert_pem(pem_buf, sizeof(pem_buf), &pem_len)) {
        console_printf("%s", pem_buf);
    } else {
        console_printf("ERROR: Failed to export certificate\r\n");
    }
    console_flush();
}

// Multi-line input buffer (for CSR, cert import, etc.)
static char g_pem_buffer[4096];
static int g_pem_buffer_pos = 0;

// Input modes
typedef enum {
    PEM_INPUT_NONE = 0,
    PEM_INPUT_CSR,          // CA_SIGN_CSR
    PEM_INPUT_CERT,         // CA_SIGN_CERT (re-sign)
    PEM_INPUT_IMPORT_KEY,   // CA_IMPORT (waiting for key)
    PEM_INPUT_IMPORT_CERT   // CA_IMPORT (waiting for cert after key)
} pem_input_mode_t;

static pem_input_mode_t g_pem_input_mode = PEM_INPUT_NONE;

// For CA_IMPORT: store key while waiting for cert
static char g_import_key_buffer[2048];
static size_t g_import_key_len = 0;

// Legacy compatibility
static bool g_csr_input_mode = false;

static void cmd_ca_sign_csr_start(void) {
    if (!ca_is_initialized()) {
        console_printf("ERROR: CA not initialized\r\n");
        return;
    }

    console_printf("Paste CSR in PEM format, end with '---' on a new line:\r\n");
    g_pem_buffer_pos = 0;
    g_pem_input_mode = PEM_INPUT_CSR;
    g_csr_input_mode = true;  // Legacy compatibility
}

static void cmd_ca_sign_cert_start(void) {
    if (!ca_is_initialized()) {
        console_printf("ERROR: CA not initialized\r\n");
        return;
    }

    console_printf("Paste certificate in PEM format, end with '---' on a new line:\r\n");
    g_pem_buffer_pos = 0;
    g_pem_input_mode = PEM_INPUT_CERT;
    g_csr_input_mode = true;  // Legacy compatibility (reuse same input handler)
}

static void cmd_ca_import_start(void) {
    if (ca_is_initialized()) {
        console_printf("ERROR: CA already initialized. Use CA_RESET first.\r\n");
        return;
    }

    console_printf("CA Import - Step 1/2\r\n");
    console_printf("Paste PRIVATE KEY in PEM format, end with '---' on a new line:\r\n");
    g_pem_buffer_pos = 0;
    g_import_key_len = 0;
    g_pem_input_mode = PEM_INPUT_IMPORT_KEY;
    g_csr_input_mode = true;  // Legacy compatibility
}

static void cmd_pem_input_process(char *line) {
    // Check for end marker
    if (strncmp(line, "---", 3) == 0) {
        g_pem_buffer[g_pem_buffer_pos] = '\0';

        switch (g_pem_input_mode) {
            case PEM_INPUT_CSR: {
                g_csr_input_mode = false;
                g_pem_input_mode = PEM_INPUT_NONE;

                console_printf("Processing CSR...\r\n");
                console_flush();

                static char cert_pem[4096];
                size_t cert_len;

                if (ca_sign_csr(g_pem_buffer, cert_pem, sizeof(cert_pem), &cert_len, 365)) {
                    console_printf("OK: Certificate issued:\r\n");
                    console_printf("%s", cert_pem);
                } else {
                    console_printf("ERROR: Failed to sign CSR\r\n");
                }
                break;
            }

            case PEM_INPUT_CERT: {
                g_csr_input_mode = false;
                g_pem_input_mode = PEM_INPUT_NONE;

                console_printf("Processing certificate for re-signing...\r\n");
                console_flush();

                static char cert_pem[4096];
                size_t cert_len;

                if (ca_sign_cert(g_pem_buffer, cert_pem, sizeof(cert_pem), &cert_len, 365)) {
                    console_printf("OK: Certificate re-signed:\r\n");
                    console_printf("%s", cert_pem);
                } else {
                    console_printf("ERROR: Failed to re-sign certificate\r\n");
                }
                break;
            }

            case PEM_INPUT_IMPORT_KEY: {
                // Store key and prompt for certificate
                if ((size_t)g_pem_buffer_pos < sizeof(g_import_key_buffer)) {
                    memcpy(g_import_key_buffer, g_pem_buffer, g_pem_buffer_pos + 1);
                    g_import_key_len = g_pem_buffer_pos;

                    console_printf("Private key received (%d bytes).\r\n", g_pem_buffer_pos);
                    console_printf("CA Import - Step 2/2\r\n");
                    console_printf("Paste CERTIFICATE in PEM format, end with '---' on a new line:\r\n");
                    g_pem_buffer_pos = 0;
                    g_pem_input_mode = PEM_INPUT_IMPORT_CERT;
                } else {
                    console_printf("ERROR: Private key too large\r\n");
                    g_csr_input_mode = false;
                    g_pem_input_mode = PEM_INPUT_NONE;
                }
                break;
            }

            case PEM_INPUT_IMPORT_CERT: {
                g_csr_input_mode = false;
                g_pem_input_mode = PEM_INPUT_NONE;

                console_printf("Importing CA...\r\n");
                console_flush();

                if (ca_import_root_pem(g_import_key_buffer, g_pem_buffer)) {
                    console_printf("OK: CA imported successfully\r\n");
                    ca_status_t status;
                    if (ca_get_status(&status)) {
                        console_printf("Common Name: %s\r\n", status.common_name);
                    }
                } else {
                    console_printf("ERROR: Failed to import CA\r\n");
                }

                // Clear sensitive key data
                memset(g_import_key_buffer, 0, sizeof(g_import_key_buffer));
                g_import_key_len = 0;
                break;
            }

            default:
                g_csr_input_mode = false;
                g_pem_input_mode = PEM_INPUT_NONE;
                break;
        }

        g_pem_buffer_pos = 0;
        return;
    }

    // Append line to buffer
    size_t line_len = strlen(line);
    if (g_pem_buffer_pos + line_len + 2 < (int)sizeof(g_pem_buffer)) {
        memcpy(g_pem_buffer + g_pem_buffer_pos, line, line_len);
        g_pem_buffer_pos += line_len;
        g_pem_buffer[g_pem_buffer_pos++] = '\n';
    } else {
        console_printf("ERROR: Input too large\r\n");
        g_csr_input_mode = false;
        g_pem_input_mode = PEM_INPUT_NONE;
        g_pem_buffer_pos = 0;
    }
}

static void cmd_ca_reset(void) {
    if (!ca_is_initialized()) {
        console_printf("CA is not initialized.\r\n");
        return;
    }

    console_printf("WARNING: This will delete the CA root key and all records!\r\n");
    console_printf("Resetting CA...\r\n");
    console_flush();

    if (ca_factory_reset()) {
        console_printf("OK: CA reset complete\r\n");
    } else {
        console_printf("ERROR: CA reset failed\r\n");
    }
}

bool serial_cmd_in_csr_mode(void) {
    return g_csr_input_mode;
}

void serial_cmd_csr_line(char *line) {
    cmd_pem_input_process(line);
}

#endif // FEATURE_CA

// ============================================================================
// Time Commands
// ============================================================================

static void cmd_set_time(char *args) {
    int hour, minute, second;
    if (sscanf(args, "%d:%d:%d", &hour, &minute, &second) == 3) {
        if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 && second >= 0 && second <= 59) {
            cdc_rtc_set_time(hour, minute, second);
            console_printf("OK: Time set to %02d:%02d:%02d\r\n", hour, minute, second);
            if (time_callback) time_callback();
        } else {
            console_printf("ERROR: Invalid time values\r\n");
        }
    } else {
        console_printf("ERROR: Usage: SET_TIME HH:MM:SS\r\n");
    }
}

static void cmd_set_date(char *args) {
    char *arg = trim(args);
    int year, month, day;
    long timestamp;

    // Try Unix timestamp first (pure number)
    if (sscanf(arg, "%ld", &timestamp) == 1 && strchr(arg, '-') == nullptr) {
        struct timeval tv = { .tv_sec = timestamp, .tv_usec = 0 };
        settimeofday(&tv, NULL);
        cdc_rtc_mark_time_set();
        struct tm timeinfo;
        localtime_r(&tv.tv_sec, &timeinfo);
        console_printf("OK: DateTime set from timestamp %ld (%04d-%02d-%02d %02d:%02d:%02d)\r\n",
               timestamp,
               timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
               timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        if (time_callback) time_callback();
    }
    // Try YYYY-MM-DD format
    else if (sscanf(arg, "%d-%d-%d", &year, &month, &day) == 3) {
        if (year >= 2020 && year <= 2099 && month >= 1 && month <= 12 && day >= 1 && day <= 31) {
            cdc_rtc_set_date(year, month, day);
            console_printf("OK: Date set to %04d-%02d-%02d\r\n", year, month, day);
            if (time_callback) time_callback();
        } else {
            console_printf("ERROR: Invalid date values\r\n");
        }
    } else {
        console_printf("ERROR: Usage: SET_DATE YYYY-MM-DD or SET_DATE <timestamp>\r\n");
    }
}

static void cmd_get_time(void) {
    char time_str[16];
    cdc_rtc_get_time_str(time_str, sizeof(time_str));
    console_printf("TIME: %s\r\n", time_str);
}

static void cmd_get_date(void) {
    char date_str[16];
    cdc_rtc_get_date_str(date_str, sizeof(date_str));
    console_printf("DATE: %s\r\n", date_str);
}

// ============================================================================
// Display Commands
// ============================================================================

static void cmd_set_name(char *args) {
    char *text = trim(args);
    if (strlen(text) > 0) {
        console_printf("OK: Name set to: %s\r\n", text);
        if (text_callback) text_callback(0, text);
    } else {
        console_printf("ERROR: Usage: SET_NAME text\r\n");
    }
}

static void cmd_set_info(char *args) {
    char *text = trim(args);
    if (strlen(text) > 0) {
        console_printf("OK: Info set to: %s\r\n", text);
        if (text_callback) text_callback(1, text);
    } else {
        console_printf("ERROR: Usage: SET_INFO text\r\n");
    }
}

static void cmd_set_info2(char *args) {
    char *text = trim(args);
    if (strlen(text) > 0) {
        console_printf("OK: Info2 set to: %s\r\n", text);
        if (text_callback) text_callback(2, text);
    } else {
        console_printf("ERROR: Usage: SET_INFO2 text\r\n");
    }
}

// ============================================================================
// Command Parser
// ============================================================================

static void execute_command(char *cmd) {
    cmd = trim(cmd);
    if (strlen(cmd) == 0) return;

    LOG_I("CMD", "Received: %s", cmd);

    // Handle CONFIRM/CANCEL for pending wipe
    if (g_wipe_pending) {
        if (strcasecmp(cmd, "CONFIRM") == 0) { cmd_confirm(); return; }
        // Any other command cancels the pending wipe
        cmd_cancel();
        // Continue processing the command
    }

#if FEATURE_SECURE_SERIAL
    // AUTH and LOGOUT commands (always allowed)
    if (strncasecmp(cmd, "AUTH ", 5) == 0) { cmd_auth(cmd + 5); return; }
    if (strcasecmp(cmd, "LOGOUT") == 0) { cmd_logout(); return; }

    // Check if command requires authentication
    if (check_auth_required(cmd)) {
        console_printf("ERROR: Authentication required. Use AUTH <pin>\r\n");
        return;
    }
#endif

    // System commands
    if (strcasecmp(cmd, "HELP") == 0) { show_help(); return; }
    if (strcasecmp(cmd, "PING") == 0) { cmd_ping(); return; }
    if (strcasecmp(cmd, "STATUS") == 0) { cmd_status(); return; }
    if (strcasecmp(cmd, "LOG_TAIL") == 0) { cmd_log_tail((char *)""); return; }
    if (strncasecmp(cmd, "LOG_TAIL ", 9) == 0) { cmd_log_tail(cmd + 9); return; }

    // Time commands
    if (strncasecmp(cmd, "SET_TIME ", 9) == 0) { cmd_set_time(cmd + 9); return; }
    if (strncasecmp(cmd, "SET_DATE ", 9) == 0) { cmd_set_date(cmd + 9); return; }
    if (strcasecmp(cmd, "GET_TIME") == 0) { cmd_get_time(); return; }
    if (strcasecmp(cmd, "GET_DATE") == 0) { cmd_get_date(); return; }

    // Display commands
    if (strncasecmp(cmd, "SET_NAME ", 9) == 0) { cmd_set_name(cmd + 9); return; }
    if (strncasecmp(cmd, "SET_INFO2 ", 10) == 0) { cmd_set_info2(cmd + 10); return; }
    if (strncasecmp(cmd, "SET_INFO ", 9) == 0) { cmd_set_info(cmd + 9); return; }

#if FEATURE_TOTP
    // TOTP commands
    if (strcasecmp(cmd, "TOTP_LIST") == 0) { cmd_totp_list(); return; }
    if (strncasecmp(cmd, "TOTP_ADD ", 9) == 0) { cmd_totp_add(cmd + 9); return; }
    if (strncasecmp(cmd, "TOTP_DEL ", 9) == 0) { cmd_totp_del(cmd + 9); return; }
    if (strncasecmp(cmd, "TOTP_GET ", 9) == 0) { cmd_totp_get(cmd + 9); return; }
    if (strncasecmp(cmd, "TOTP_TYPE ", 10) == 0) { cmd_totp_type(cmd + 10); return; }
#endif

#if FEATURE_FIDO2
    // FIDO2 commands
    if (strcasecmp(cmd, "FIDO_STATUS") == 0) { cmd_fido_status(); return; }
    if (strcasecmp(cmd, "FIDO_LIST") == 0) { cmd_fido_list(); return; }
    if (strncasecmp(cmd, "FIDO_DEL ", 9) == 0) { cmd_fido_del(cmd + 9); return; }
    if (strcasecmp(cmd, "FIDO_RESET") == 0) { cmd_fido_reset(); return; }
#endif

    // TROPIC01 commands
    if (strcasecmp(cmd, "TR01_STATUS") == 0) { cmd_tr01_status(); return; }
    if (strcasecmp(cmd, "TR01_INFO") == 0) { cmd_tr01_info(); return; }
    if (strcasecmp(cmd, "TR01_SESSION") == 0) { cmd_tr01_session(); return; }
    if (strcasecmp(cmd, "TR01_SLOTS") == 0) { cmd_tr01_slots(); return; }
    if (strcasecmp(cmd, "TR01_RESYNC") == 0) { cmd_tr01_resync(); return; }
    if (strncasecmp(cmd, "TR01_ECC_DEL ", 13) == 0) { cmd_tr01_ecc_del(cmd + 13); return; }
    if (strncasecmp(cmd, "TR01_RMEM_DEL ", 14) == 0) { cmd_tr01_rmem_del(cmd + 14); return; }
    if (strncasecmp(cmd, "TR01_RMEM_READ ", 15) == 0) { cmd_tr01_rmem_read(cmd + 15); return; }
    if (strcasecmp(cmd, "TR01_WIPE") == 0) { cmd_tr01_wipe(); return; }

#if FEATURE_CA
    // CA commands
    if (strcasecmp(cmd, "CA_STATUS") == 0) { cmd_ca_status(); return; }
    if (strncasecmp(cmd, "CA_INIT", 7) == 0) {
        if (strlen(cmd) > 8) {
            cmd_ca_init(cmd + 8);
        } else {
            cmd_ca_init((char*)"");
        }
        return;
    }
    if (strcasecmp(cmd, "CA_LIST") == 0) { cmd_ca_list(); return; }
    if (strcasecmp(cmd, "CA_EXPORT_ROOT") == 0) { cmd_ca_export_root(); return; }
    if (strcasecmp(cmd, "CA_SIGN_CSR") == 0) { cmd_ca_sign_csr_start(); return; }
    if (strcasecmp(cmd, "CA_SIGN_CERT") == 0) { cmd_ca_sign_cert_start(); return; }
    if (strcasecmp(cmd, "CA_IMPORT") == 0) { cmd_ca_import_start(); return; }
    if (strcasecmp(cmd, "CA_RESET") == 0) { cmd_ca_reset(); return; }
#endif

    console_printf("ERROR: Unknown command. Type HELP for available commands.\r\n");
}

// ============================================================================
// Public API
// ============================================================================

void serial_cmd_init(void) {
    cmd_buffer_pos = 0;
    memset(cmd_buffer, 0, sizeof(cmd_buffer));
    LOG_I("CMD", "Serial command interface initialized");
    show_prompt();
}

bool serial_cmd_process(void) {
    int c = console_getchar();
    if (c < 0) {
        return false;
    }

    // Handle newline (CR or LF) - command complete
    if (c == '\r' || c == '\n') {
        console_printf("\r\n");
        console_flush();
        if (cmd_buffer_pos > 0) {
            cmd_buffer[cmd_buffer_pos] = '\0';

#if FEATURE_CA
            // In CSR input mode, pass line to CSR handler
            if (serial_cmd_in_csr_mode()) {
                serial_cmd_csr_line(cmd_buffer);
                cmd_buffer_pos = 0;
                memset(cmd_buffer, 0, sizeof(cmd_buffer));
                if (!serial_cmd_in_csr_mode()) {
                    show_prompt();
                }
                return true;
            }
#endif

            execute_command(cmd_buffer);
            cmd_buffer_pos = 0;
            memset(cmd_buffer, 0, sizeof(cmd_buffer));
            show_prompt();
            return true;
        }
        // Empty line - just show new prompt
#if FEATURE_CA
        if (!serial_cmd_in_csr_mode())
#endif
            show_prompt();
        return false;
    }

    // Handle backspace (DEL=0x7F or BS=0x08)
    if (c == 0x7F || c == 0x08) {
        if (cmd_buffer_pos > 0) {
            cmd_buffer_pos--;
            cmd_buffer[cmd_buffer_pos] = '\0';
            // Erase character on terminal: backspace, space, backspace
            console_printf("\b \b");
            console_flush();
        }
        return false;
    }

    // Handle Ctrl+C - cancel current line
    if (c == 0x03) {
        console_printf("^C\r\n");
        cmd_buffer_pos = 0;
        memset(cmd_buffer, 0, sizeof(cmd_buffer));
        show_prompt();
        return false;
    }

    // Handle Ctrl+U - clear line
    if (c == 0x15) {
        while (cmd_buffer_pos > 0) {
            console_printf("\b \b");
            cmd_buffer_pos--;
        }
        memset(cmd_buffer, 0, sizeof(cmd_buffer));
        console_flush();
        return false;
    }

    // Add printable character to buffer
    if (c >= 0x20 && c < 0x7F && cmd_buffer_pos < CMD_BUFFER_SIZE - 1) {
        cmd_buffer[cmd_buffer_pos++] = (char)c;
        console_putchar((char)c);
        console_flush();
    }

    return false;
}

void serial_cmd_set_text_callback(serial_cmd_text_callback_t callback) {
    text_callback = callback;
}

void serial_cmd_set_time_callback(serial_cmd_time_callback_t callback) {
    time_callback = callback;
}
