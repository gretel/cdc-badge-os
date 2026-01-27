// TOTP Account Storage (TROPIC01 R-Memory)
//
// IMPORTANT: Uses TROPIC01 cache for metadata to prevent chip lockout!
// Only accesses TROPIC01 directly when reading secrets for code generation.

#include "totp_store.h"
#include "totp.h"
#include "base32.h"
#include "tropic01.h"
#include "tropic01_cache.h"  // Used for cache queries during init (not for updates - tropic01.cpp handles that)
#include "keyboard_typing.h"
#include "cdc_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// Storage Layout (R-Memory structure)
// ============================================================================

#pragma pack(push, 1)
typedef struct {
    uint8_t magic;                      // 0xAA = valid account
    char name[TOTP_NAME_LEN];           // 32 bytes
    char issuer[TOTP_ISSUER_LEN];       // 32 bytes
    uint8_t secret[TOTP_SECRET_LEN];    // 32 bytes
    uint8_t secretLen;                  // 1 byte
    uint8_t digits;                     // 1 byte
    uint32_t period;                    // 4 bytes
    uint8_t algorithm;                  // 1 byte
    uint8_t flags;                      // 1 byte
} totp_stored_t;                        // Total: 105 bytes
#pragma pack(pop)

#define TOTP_MAGIC 0xAA
#define TOTP_STORED_SIZE sizeof(totp_stored_t)

// ============================================================================
// Internal State
// ============================================================================

// Cached account info (from TROPIC01 cache - no secrets!)
typedef struct {
    char name[TOTP_NAME_LEN];
    char issuer[TOTP_ISSUER_LEN];
    uint8_t digits;
    uint32_t period;
    uint8_t algorithm;
    bool valid;
} totp_cache_entry_t;

static totp_cache_entry_t s_account_cache[TOTP_MAX_ACCOUNTS];
static uint8_t s_account_count = 0;
static bool s_initialized = false;

// Last accessed account (with secret - for code generation)
static struct {
    bool valid;
    uint8_t index;
    totp_account_t account;
} s_active_account = {};

// ============================================================================
// Helper Functions
// ============================================================================

static uint16_t index_to_slot(uint8_t index) {
    return TOTP_SLOT_BASE + index;
}

// Map logical index to physical slot (nth valid entry)
static int8_t get_physical_slot(uint8_t index) {
    uint8_t validIdx = 0;
    for (uint8_t i = 0; i < TOTP_MAX_ACCOUNTS; i++) {
        if (s_account_cache[i].valid) {
            if (validIdx == index) {
                return i;
            }
            validIdx++;
        }
    }
    return -1;
}

static void invalidate_active_cache(void) {
    // Clear secret from RAM
    memset(&s_active_account.account.secret, 0, TOTP_SECRET_LEN);
    s_active_account.valid = false;
}

// Read full account from TROPIC01 (including secret)
static bool read_account_from_tropic(uint8_t physical_slot, totp_stored_t *stored) {
    if (physical_slot >= TOTP_MAX_ACCOUNTS) return false;

    uint16_t slot = index_to_slot(physical_slot);
    uint8_t buffer[TOTP_STORED_SIZE];
    uint16_t readSize = 0;

    if (!tropic01_rmem_read(slot, buffer, TOTP_STORED_SIZE, &readSize)) {
        LOG_E("TOTP", "Failed to read slot %d", slot);
        return false;
    }

    if (readSize == 0) {
        LOG_E("TOTP", "Slot %d: empty", slot);
        return false;
    }

    // Zero-init then copy (handles any size mismatch gracefully)
    memset(stored, 0, TOTP_STORED_SIZE);
    memcpy(stored, buffer, readSize > TOTP_STORED_SIZE ? TOTP_STORED_SIZE : readSize);
    return stored->magic == TOTP_MAGIC;
}

static bool write_account(uint8_t physical_slot, const totp_stored_t *stored) {
    if (physical_slot >= TOTP_MAX_ACCOUNTS) return false;

    uint16_t slot = index_to_slot(physical_slot);

    // First erase existing data
    tropic01_rmem_erase(slot);

    // Write new data (cache is automatically updated by tropic01_rmem_write)
    if (!tropic01_rmem_write(slot, (const uint8_t *)stored, TOTP_STORED_SIZE)) {
        LOG_E("TOTP", "Failed to write slot %d", slot);
        return false;
    }

    LOG_I("TOTP", "Wrote account to slot %d", slot);
    return true;
}

// ============================================================================
// Initialization
// ============================================================================

uint8_t totp_store_init(void) {
    LOG_I("TOTP", "Initializing store...");

    s_account_count = 0;
    memset(s_account_cache, 0, sizeof(s_account_cache));
    invalidate_active_cache();

    // Use TROPIC01 cache to enumerate accounts (NO chip access!)
    for (uint8_t i = 0; i < TOTP_MAX_ACCOUNTS; i++) {
        uint16_t slot = index_to_slot(i);

        bool exists = tropic01_cache_rmem_exists(slot);
        if (i < 3) {  // Debug first 3 slots
            LOG_D("TOTP", "Slot %d (idx %d): cache_exists=%d", slot, i, exists);
        }

        if (exists) {
            // Get cached metadata (secret is zeroed in cache!)
            uint8_t data[128];
            uint16_t size;
            if (tropic01_cache_rmem_get(slot, data, &size)) {
                totp_stored_t *stored = (totp_stored_t *)data;

                LOG_D("TOTP", "Slot %d: size=%d, magic=0x%02X (expected 0x%02X)",
                      slot, size, stored->magic, TOTP_MAGIC);

                if (stored->magic == TOTP_MAGIC) {
                    // Copy metadata to local cache
                    strncpy(s_account_cache[i].name, stored->name, TOTP_NAME_LEN - 1);
                    s_account_cache[i].name[TOTP_NAME_LEN - 1] = '\0';
                    strncpy(s_account_cache[i].issuer, stored->issuer, TOTP_ISSUER_LEN - 1);
                    s_account_cache[i].issuer[TOTP_ISSUER_LEN - 1] = '\0';
                    s_account_cache[i].digits = stored->digits ? stored->digits : TOTP_DEFAULT_DIGITS;
                    s_account_cache[i].period = stored->period ? stored->period : TOTP_DEFAULT_PERIOD;
                    s_account_cache[i].algorithm = stored->algorithm;
                    s_account_cache[i].valid = true;
                    s_account_count++;

                    LOG_D("TOTP", "Found account %d: %s (%s)", i,
                          s_account_cache[i].name, s_account_cache[i].issuer);
                }
            }
        }
    }

    s_initialized = true;
    LOG_I("TOTP", "Found %d accounts", s_account_count);
    return s_account_count;
}

// ============================================================================
// Account Management
// ============================================================================

uint8_t totp_store_count(void) {
    return s_account_count;
}

bool totp_store_get(uint8_t index, totp_account_t *account) {
    if (!account || index >= s_account_count) return false;

    int8_t phys_slot = get_physical_slot(index);
    if (phys_slot < 0) return false;

    // Check if we have it cached
    if (s_active_account.valid && s_active_account.index == index) {
        *account = s_active_account.account;
        return true;
    }

    // Read full account from TROPIC01 (including secret!)
    totp_stored_t stored;
    if (!read_account_from_tropic(phys_slot, &stored)) {
        return false;
    }

    // Copy to output (explicit null-termination for safety)
    strncpy(account->name, stored.name, TOTP_NAME_LEN - 1);
    account->name[TOTP_NAME_LEN - 1] = '\0';
    strncpy(account->issuer, stored.issuer, TOTP_ISSUER_LEN - 1);
    account->issuer[TOTP_ISSUER_LEN - 1] = '\0';
    memcpy(account->secret, stored.secret, TOTP_SECRET_LEN);
    account->secretLen = stored.secretLen;
    account->digits = stored.digits ? stored.digits : TOTP_DEFAULT_DIGITS;
    account->period = stored.period ? stored.period : TOTP_DEFAULT_PERIOD;
    account->algorithm = stored.algorithm;
    account->flags = stored.flags;

    // Cache for repeated access
    s_active_account.valid = true;
    s_active_account.index = index;
    s_active_account.account = *account;

    return true;
}

bool totp_store_get_info(uint8_t index, totp_account_info_t *info) {
    if (!info || index >= s_account_count) return false;

    int8_t phys_slot = get_physical_slot(index);
    if (phys_slot < 0) return false;

    memset(info, 0, sizeof(*info));
    strncpy(info->name, s_account_cache[phys_slot].name, TOTP_NAME_LEN - 1);
    strncpy(info->issuer, s_account_cache[phys_slot].issuer, TOTP_ISSUER_LEN - 1);
    info->digits = s_account_cache[phys_slot].digits;
    info->period = s_account_cache[phys_slot].period;
    info->algorithm = s_account_cache[phys_slot].algorithm;
    return true;
}

void totp_store_clear_secret_cache(void) {
    invalidate_active_cache();
}

int8_t totp_store_add(const char *name, const char *issuer,
                      const char *secretBase32, uint8_t digits,
                      uint32_t period, uint8_t algorithm) {
    if (!name || !secretBase32) return -1;

    // Find first free slot
    int8_t freeSlot = -1;
    for (uint8_t i = 0; i < TOTP_MAX_ACCOUNTS; i++) {
        if (!s_account_cache[i].valid) {
            freeSlot = i;
            break;
        }
    }

    if (freeSlot < 0) {
        LOG_E("TOTP", "No free slots");
        return -1;
    }

    // Decode secret
    uint8_t secret[TOTP_SECRET_LEN];
    int secretLen = base32_decode(secretBase32, secret, TOTP_SECRET_LEN);
    if (secretLen <= 0) {
        LOG_E("TOTP", "Invalid Base32 secret");
        return -1;
    }

    // Prepare stored structure
    totp_stored_t stored;
    memset(&stored, 0, sizeof(stored));
    stored.magic = TOTP_MAGIC;

    strncpy(stored.name, name, TOTP_NAME_LEN - 1);
    stored.name[TOTP_NAME_LEN - 1] = '\0';

    if (issuer) {
        strncpy(stored.issuer, issuer, TOTP_ISSUER_LEN - 1);
        stored.issuer[TOTP_ISSUER_LEN - 1] = '\0';
    }

    memcpy(stored.secret, secret, secretLen);
    stored.secretLen = secretLen;
    stored.digits = digits > 0 ? digits : TOTP_DEFAULT_DIGITS;
    stored.period = period > 0 ? period : TOTP_DEFAULT_PERIOD;
    stored.algorithm = algorithm;
    stored.flags = 0;

    // Write to storage
    if (!write_account(freeSlot, &stored)) {
        return -1;
    }

    // Update local cache (without secret!)
    strncpy(s_account_cache[freeSlot].name, stored.name, TOTP_NAME_LEN);
    strncpy(s_account_cache[freeSlot].issuer, stored.issuer, TOTP_ISSUER_LEN);
    s_account_cache[freeSlot].digits = stored.digits;
    s_account_cache[freeSlot].period = stored.period;
    s_account_cache[freeSlot].algorithm = stored.algorithm;
    s_account_cache[freeSlot].valid = true;
    s_account_count++;
    invalidate_active_cache();

    LOG_I("TOTP", "Added account '%s' at slot %d", name, index_to_slot(freeSlot));
    return freeSlot;
}

bool totp_store_delete(uint8_t index) {
    if (index >= s_account_count) return false;

    int8_t phys_slot = get_physical_slot(index);
    if (phys_slot < 0) return false;

    uint16_t slot = index_to_slot(phys_slot);

    // Erase from TROPIC01 (cache is automatically invalidated by tropic01_rmem_erase)
    if (!tropic01_rmem_erase(slot)) {
        LOG_E("TOTP", "Failed to erase slot %d", slot);
        return false;
    }

    // Update local cache
    s_account_cache[phys_slot].valid = false;
    s_account_cache[phys_slot].name[0] = '\0';
    s_account_cache[phys_slot].issuer[0] = '\0';
    s_account_count--;
    invalidate_active_cache();

    LOG_I("TOTP", "Deleted account at slot %d", slot);
    return true;
}

uint16_t totp_store_slot(uint8_t index) {
    int8_t phys_slot = get_physical_slot(index);
    if (phys_slot < 0) return 0;
    return index_to_slot(phys_slot);
}

// ============================================================================
// Code Generation
// ============================================================================

int8_t totp_store_generate_code(uint8_t index, char *codeOut) {
    if (!codeOut) {
        LOG_E("TOTP", "generate_code: codeOut is NULL");
        return -1;
    }

    totp_account_t account;
    if (!totp_store_get(index, &account)) {
        LOG_E("TOTP", "generate_code: failed to get account %d", index);
        return -1;
    }

    if (!totp_time_valid()) {
        LOG_W("TOTP", "generate_code: time not valid");
        strcpy(codeOut, "------");
        return -1;
    }

    // Generate code
    uint32_t code = totp_generate(account.secret, account.secretLen,
                                  time(NULL), account.period, account.digits,
                                  (totp_algorithm_t)account.algorithm);

    // Format as string with leading zeros
    if (account.digits == 8) {
        snprintf(codeOut, 9, "%08lu", (unsigned long)code);
    } else if (account.digits == 7) {
        snprintf(codeOut, 8, "%07lu", (unsigned long)code);
    } else {
        snprintf(codeOut, 7, "%06lu", (unsigned long)code);
    }

    return totp_time_remaining(account.period);
}

bool totp_store_type_code(uint8_t index, bool press_enter) {
    char code[9];

    int8_t remaining = totp_store_generate_code(index, code);
    if (remaining < 0) {
        LOG_E("TOTP", "Cannot type code - generation failed");
        return false;
    }

    LOG_I("TOTP", "Typing code %s for account %d", code, index);

    // Small delay for host to be ready
    vTaskDelay(pdMS_TO_TICKS(100));

    // Type the code via available keyboard (USB or BLE HID)
    bool success = keyboard_type(code, press_enter);
    if (!success) {
        LOG_W("TOTP", "No keyboard available (USB/BLE)");
        return false;
    }

    return true;
}
