// TOTP Account Storage (TROPIC01 R-Memory)

#ifndef TOTP_STORE_H
#define TOTP_STORE_H

#include <stdint.h>
#include <stdbool.h>
#include "totp.h"

#ifdef __cplusplus
extern "C" {
#endif

// Storage configuration (R-Memory slots 33-132)
#define TOTP_SLOT_BASE    33
#define TOTP_SLOT_MAX     132
#define TOTP_MAX_ACCOUNTS 100

// Account structure sizes
#define TOTP_NAME_LEN     32
#define TOTP_ISSUER_LEN   32
#define TOTP_SECRET_LEN   32

// Account structure (for reading full account with secret)
typedef struct {
    char name[TOTP_NAME_LEN];
    char issuer[TOTP_ISSUER_LEN];
    uint8_t secret[TOTP_SECRET_LEN];
    uint8_t secretLen;
    uint8_t digits;
    uint32_t period;
    uint8_t algorithm;      // totp_algorithm_t
    uint8_t flags;
} totp_account_t;

// Account info (metadata only, no secret)
typedef struct {
    char name[TOTP_NAME_LEN];
    char issuer[TOTP_ISSUER_LEN];
    uint8_t digits;
    uint32_t period;
    uint8_t algorithm;
} totp_account_info_t;

// ============================================================================
// Initialization
// ============================================================================

/**
 * Initialize TOTP store.
 * Uses TROPIC01 cache, no direct chip access.
 *
 * @return Number of accounts found
 */
uint8_t totp_store_init(void);

// ============================================================================
// Account Management
// ============================================================================

/**
 * Get number of stored accounts.
 */
uint8_t totp_store_count(void);

/**
 * Get full account by index (including secret from TROPIC01).
 * WARNING: Accesses TROPIC01 to retrieve secret!
 *
 * @param index Account index (0 to count-1)
 * @param account Output structure
 * @return true if account exists
 */
bool totp_store_get(uint8_t index, totp_account_t *account);

/**
 * Get metadata for display (no TROPIC01 access - uses cache).
 */
bool totp_store_get_info(uint8_t index, totp_account_info_t *info);

/**
 * Clear secret from RAM cache after use.
 */
void totp_store_clear_secret_cache(void);

/**
 * Add a new TOTP account.
 *
 * @param name Account name
 * @param issuer Issuer name (can be NULL)
 * @param secretBase32 Base32-encoded secret
 * @param digits Number of digits (6, 7, 8; 0 for default)
 * @param period Period in seconds (0 for default 30s)
 * @param algorithm HMAC algorithm (0 for default SHA-1)
 * @return Index of new account, or -1 on error
 */
int8_t totp_store_add(const char *name, const char *issuer,
                      const char *secretBase32, uint8_t digits,
                      uint32_t period, uint8_t algorithm);

/**
 * Delete an account by index.
 *
 * @param index Account index
 * @return true on success
 */
bool totp_store_delete(uint8_t index);

/**
 * Get the R-Memory slot for an account index.
 */
uint16_t totp_store_slot(uint8_t index);

// ============================================================================
// Code Generation
// ============================================================================

/**
 * Generate TOTP code for an account.
 * WARNING: Accesses TROPIC01 to retrieve secret!
 *
 * @param index Account index
 * @param codeOut Output for the code string (must be at least 9 chars)
 * @return Seconds remaining until code changes, or -1 on error
 */
int8_t totp_store_generate_code(uint8_t index, char *codeOut);

/**
 * Type TOTP code via USB HID keyboard.
 *
 * @param index Account index
 * @param press_enter Also press Enter after typing the code
 * @return true on success
 */
bool totp_store_type_code(uint8_t index, bool press_enter);

#ifdef __cplusplus
}
#endif

#endif // TOTP_STORE_H
