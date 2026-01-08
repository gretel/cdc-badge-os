// FIDO2 Storage Layer (TROPIC01 + NVS)
// Handles credential storage in ECC slots and R-Memory

#include "fido2_storage.h"
#include "tropic01.h"
#include "tropic01_cache.h"
#include "cdc_log.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <string.h>

// ============================================================================
// Storage Layout
// ============================================================================

#define FIDO2_RMEM_MAGIC        "FID2"
#define FIDO2_RMEM_MAGIC_LEN    4
#define NVS_NAMESPACE           "fido2"
#define NVS_KEY_COUNTER         "auth_cnt"

#pragma pack(push, 1)
typedef struct {
    uint8_t magic[FIDO2_RMEM_MAGIC_LEN];    // "FID2"
    uint8_t rp_id_hash[32];                 // SHA-256 of RP ID
    char rp_id[FIDO2_RP_ID_MAX_LEN];        // RP ID string (for display)
    uint8_t user_id[FIDO2_USER_ID_MAX_LEN]; // User handle
    uint8_t user_id_len;                    // Length of user ID
    char user_name[FIDO2_USER_NAME_MAX_LEN];// Display name
    uint32_t sign_count;                    // Per-credential counter
    uint8_t cred_id_nonce[16];              // Random nonce for credential ID
    uint8_t flags;                          // Flags (resident, cred_protect, etc.)
    uint8_t cred_protect;                   // Credential protection level
    uint8_t reserved[8];                    // Reserved for future use
} fido2_stored_cred_t;                      // Total: ~180 bytes
#pragma pack(pop)

#define FIDO2_STORED_SIZE sizeof(fido2_stored_cred_t)

// Flag bits
#define FIDO2_FLAG_RESIDENT     0x01

// ============================================================================
// State
// ============================================================================

static struct {
    bool initialized;
    uint32_t auth_counter;
    bool counter_loaded;

    // Cached credential info (from TROPIC01 cache)
    struct {
        bool valid;
        uint8_t rp_id_hash[32];
        char rp_id[FIDO2_RP_ID_MAX_LEN];
        char user_name[FIDO2_USER_NAME_MAX_LEN];
        uint8_t user_id_len;
        uint32_t sign_count;
        bool resident;
        uint8_t cred_protect;
    } creds[FIDO2_MAX_CREDENTIALS];

    uint8_t cred_count;
} g_storage = {};

// ============================================================================
// NVS Counter Operations
// ============================================================================

void fido2_storage_counter_load(void) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        nvs_get_u32(nvs, NVS_KEY_COUNTER, &g_storage.auth_counter);
        nvs_close(nvs);
        LOG_I("FIDO2", "Loaded auth counter: %lu", g_storage.auth_counter);
    } else {
        g_storage.auth_counter = 0;
    }
    g_storage.counter_loaded = true;
}

uint32_t fido2_storage_counter_get(void) {
    if (!g_storage.counter_loaded) {
        fido2_storage_counter_load();
    }
    return g_storage.auth_counter;
}

void fido2_storage_counter_increment(void) {
    if (!g_storage.counter_loaded) {
        fido2_storage_counter_load();
    }
    g_storage.auth_counter++;

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u32(nvs, NVS_KEY_COUNTER, g_storage.auth_counter);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

// ============================================================================
// Initialization
// ============================================================================

uint8_t fido2_storage_init(void) {
    LOG_I("FIDO2", "Initializing storage...");

    memset(&g_storage, 0, sizeof(g_storage));
    fido2_storage_counter_load();

    // Load credential metadata from TROPIC01 cache
    for (uint8_t i = 0; i <= FIDO2_ECC_SLOT_MAX; i++) {
        uint16_t rmem_slot = FIDO2_RMEM_SLOT_BASE + i;

        if (tropic01_cache_rmem_exists(rmem_slot)) {
            uint8_t data[256];
            uint16_t size;

            if (tropic01_cache_rmem_get(rmem_slot, data, &size)) {
                if (size >= FIDO2_STORED_SIZE) {
                    fido2_stored_cred_t *stored = (fido2_stored_cred_t *)data;

                    // Verify magic
                    if (memcmp(stored->magic, FIDO2_RMEM_MAGIC, FIDO2_RMEM_MAGIC_LEN) == 0) {
                        // Cache credential info
                        g_storage.creds[i].valid = true;
                        memcpy(g_storage.creds[i].rp_id_hash, stored->rp_id_hash, 32);
                        strncpy(g_storage.creds[i].rp_id, stored->rp_id, FIDO2_RP_ID_MAX_LEN - 1);
                        strncpy(g_storage.creds[i].user_name, stored->user_name, FIDO2_USER_NAME_MAX_LEN - 1);
                        g_storage.creds[i].user_id_len = stored->user_id_len;
                        g_storage.creds[i].sign_count = stored->sign_count;
                        g_storage.creds[i].resident = (stored->flags & FIDO2_FLAG_RESIDENT) != 0;
                        g_storage.creds[i].cred_protect = stored->cred_protect;
                        g_storage.cred_count++;

                        LOG_D("FIDO2", "Found credential %d: %s", i, stored->rp_id);
                    }
                }
            }
        }
    }

    g_storage.initialized = true;
    LOG_I("FIDO2", "Found %d credentials", g_storage.cred_count);
    return g_storage.cred_count;
}

// ============================================================================
// Lookup Operations (use cache - no TROPIC01 access)
// ============================================================================

uint8_t fido2_storage_count(void) {
    return g_storage.cred_count;
}

bool fido2_storage_slot_used(uint8_t slot) {
    if (slot > FIDO2_ECC_SLOT_MAX) return false;
    return g_storage.creds[slot].valid;
}

int8_t fido2_storage_find_free_slot(void) {
    for (uint8_t i = 0; i <= FIDO2_ECC_SLOT_MAX; i++) {
        if (!g_storage.creds[i].valid) {
            return i;
        }
    }
    return -1;
}

uint8_t fido2_storage_find_by_rp(const uint8_t *rp_id_hash,
                                  uint8_t *out_slots, uint8_t max_slots) {
    uint8_t count = 0;

    for (uint8_t i = 0; i <= FIDO2_ECC_SLOT_MAX && count < max_slots; i++) {
        if (g_storage.creds[i].valid &&
            memcmp(g_storage.creds[i].rp_id_hash, rp_id_hash, 32) == 0) {
            out_slots[count++] = i;
        }
    }

    return count;
}

uint8_t fido2_storage_find_by_rp_resident(const uint8_t *rp_id_hash,
                                          uint8_t *out_slots, uint8_t max_slots) {
    uint8_t count = 0;

    LOG_D("FIDO2", "Searching for resident creds, total=%d", g_storage.cred_count);
    for (uint8_t i = 0; i <= FIDO2_ECC_SLOT_MAX && count < max_slots; i++) {
        if (g_storage.creds[i].valid) {
            bool rp_match = memcmp(g_storage.creds[i].rp_id_hash, rp_id_hash, 32) == 0;
            LOG_D("FIDO2", "Slot %d: valid=%d resident=%d rp_match=%d rp=%s",
                  i, g_storage.creds[i].valid, g_storage.creds[i].resident,
                  rp_match, g_storage.creds[i].rp_id);
            if (g_storage.creds[i].resident && rp_match) {
                out_slots[count++] = i;
            }
        }
    }

    return count;
}

bool fido2_storage_is_resident(uint8_t slot) {
    if (slot > FIDO2_ECC_SLOT_MAX) return false;
    return g_storage.creds[slot].valid && g_storage.creds[slot].resident;
}

int8_t fido2_storage_find_slot_by_cred_id(const uint8_t *cred_id, uint16_t cred_id_len) {
    if (!cred_id || cred_id_len != FIDO2_CRED_ID_LEN) return -1;

    uint8_t slot = cred_id[0];
    if (slot > FIDO2_ECC_SLOT_MAX || !g_storage.creds[slot].valid) {
        return -1;
    }

    uint8_t stored_id[FIDO2_CRED_ID_LEN];
    if (!fido2_storage_get_cred_id(slot, stored_id)) {
        return -1;
    }

    if (memcmp(stored_id, cred_id, FIDO2_CRED_ID_LEN) != 0) {
        return -1;
    }

    return (int8_t)slot;
}

bool fido2_storage_get_user(uint8_t slot,
                            uint8_t *user_id,
                            uint8_t *user_id_len,
                            char *user_name,
                            size_t user_name_max) {
    if (slot > FIDO2_ECC_SLOT_MAX || !g_storage.creds[slot].valid) {
        return false;
    }

    uint16_t rmem_slot = FIDO2_RMEM_SLOT_BASE + slot;
    uint8_t data[256];
    uint16_t size = 0;

    if (!tropic01_cache_rmem_get(rmem_slot, data, &size)) {
        return false;
    }

    if (size < FIDO2_STORED_SIZE) {
        return false;
    }

    fido2_stored_cred_t *stored = (fido2_stored_cred_t *)data;
    if (memcmp(stored->magic, FIDO2_RMEM_MAGIC, FIDO2_RMEM_MAGIC_LEN) != 0) {
        return false;
    }

    if (user_id && user_id_len) {
        uint8_t len = stored->user_id_len;
        if (len > FIDO2_USER_ID_MAX_LEN) len = FIDO2_USER_ID_MAX_LEN;
        memcpy(user_id, stored->user_id, len);
        *user_id_len = len;
    }

    if (user_name && user_name_max > 0) {
        size_t copy_len = strnlen(stored->user_name, FIDO2_USER_NAME_MAX_LEN);
        if (copy_len >= user_name_max) copy_len = user_name_max - 1;
        memcpy(user_name, stored->user_name, copy_len);
        user_name[copy_len] = '\0';
    }

    return true;
}

bool fido2_storage_verify_cred_id(uint8_t slot, const uint8_t *cred_id) {
    if (!cred_id) return false;
    uint8_t stored_id[FIDO2_CRED_ID_LEN];
    if (!fido2_storage_get_cred_id(slot, stored_id)) {
        return false;
    }
    return memcmp(stored_id, cred_id, FIDO2_CRED_ID_LEN) == 0;
}

bool fido2_storage_get_cred_id(uint8_t slot, uint8_t *out_cred_id) {
    if (slot > FIDO2_ECC_SLOT_MAX || !g_storage.creds[slot].valid || !out_cred_id) {
        return false;
    }

    // Read stored credential data from TROPIC01 to get nonce
    fido2_stored_cred_t stored;
    uint16_t read_size;
    if (!tropic01_rmem_read(slot, (uint8_t*)&stored, sizeof(stored), &read_size)) {
        LOG_E("FIDO2", "Failed to read credential %d", slot);
        return false;
    }

    if (read_size < sizeof(stored) || memcmp(stored.magic, "FID2", 4) != 0) {
        LOG_E("FIDO2", "Invalid credential data in slot %d", slot);
        return false;
    }

    // Build credential ID: slot (1) + nonce (16) + padding (47) = 64 bytes
    memset(out_cred_id, 0, FIDO2_CRED_ID_LEN);
    out_cred_id[0] = slot;
    memcpy(out_cred_id + 1, stored.cred_id_nonce, 16);

    return true;
}

// ============================================================================
// Credential Operations
// ============================================================================

bool fido2_storage_get_credential(uint8_t slot, fido2_credential_info_t *info) {
    if (slot > FIDO2_ECC_SLOT_MAX || !g_storage.creds[slot].valid || !info) {
        return false;
    }

    memset(info, 0, sizeof(*info));
    info->slot = slot;
    memcpy(info->rp_id_hash, g_storage.creds[slot].rp_id_hash, 32);
    strncpy(info->rp_id, g_storage.creds[slot].rp_id, FIDO2_RP_ID_MAX_LEN - 1);
    strncpy(info->user_name, g_storage.creds[slot].user_name, FIDO2_USER_NAME_MAX_LEN - 1);
    info->user_id_len = 0;
    info->sign_count = g_storage.creds[slot].sign_count;
    info->resident_key = g_storage.creds[slot].resident;
    info->cred_protect = g_storage.creds[slot].cred_protect;

    // Load user ID from R-Memory (not cached)
    uint8_t user_id_len = 0;
    if (fido2_storage_get_user(slot, info->user_id, &user_id_len,
                               info->user_name, FIDO2_USER_NAME_MAX_LEN)) {
        info->user_id_len = user_id_len;
    }

    return true;
}

bool fido2_storage_create_credential(
    const char *rp_id,
    const uint8_t *rp_id_hash,
    const uint8_t *user_id,
    uint8_t user_id_len,
    const char *user_name,
    bool resident_key,
    uint8_t cred_protect,
    uint8_t *out_slot,
    uint8_t *out_cred_id,
    uint8_t *out_pubkey
) {
    if (user_id && user_id_len > FIDO2_USER_ID_MAX_LEN) {
        LOG_E("FIDO2", "User ID too long: %u", user_id_len);
        return false;
    }

    // Find free slot
    int8_t slot = fido2_storage_find_free_slot();
    if (slot < 0) {
        LOG_E("FIDO2", "No free slots");
        return false;
    }

    LOG_I("FIDO2", "Creating credential in slot %d for %s", slot, rp_id);

    // Explicitly erase slot first to ensure it's empty
    // (handles cache/chip state mismatch)
    LOG_D("FIDO2", "Erasing slot %d before key generation", slot);
    tropic01_ecc_key_erase(slot);

    // Generate ECC key (P-256)
    if (!tropic01_ecc_key_generate(slot, CDC_CURVE_P256)) {
        LOG_E("FIDO2", "Failed to generate key in slot %d", slot);
        return false;
    }

    // Read public key
    uint8_t pubkey[64];  // TROPIC01 returns X||Y without 0x04 prefix
    uint8_t curve, origin;
    if (!tropic01_ecc_key_read(slot, pubkey, 64, &curve, &origin)) {
        LOG_E("FIDO2", "Failed to read public key from slot %d", slot);
        tropic01_ecc_key_erase(slot);
        return false;
    }

    // Generate random nonce for credential ID
    uint8_t nonce[16];
    if (!tropic01_get_random(nonce, 16)) {
        LOG_E("FIDO2", "Failed to generate nonce");
        tropic01_ecc_key_erase(slot);
        return false;
    }

    // Build credential ID (64 bytes)
    // Format: slot (1) + nonce (16) + padding (47)
    // In production, use HMAC for binding
    memset(out_cred_id, 0, FIDO2_CRED_ID_LEN);
    out_cred_id[0] = slot;
    memcpy(out_cred_id + 1, nonce, 16);

    // Prepare stored credential
    fido2_stored_cred_t stored;
    memset(&stored, 0, sizeof(stored));
    memcpy(stored.magic, FIDO2_RMEM_MAGIC, FIDO2_RMEM_MAGIC_LEN);
    memcpy(stored.rp_id_hash, rp_id_hash, 32);
    if (rp_id) {
        strncpy(stored.rp_id, rp_id, FIDO2_RP_ID_MAX_LEN - 1);
    }
    if (user_id && user_id_len > 0) {
        memcpy(stored.user_id, user_id, user_id_len);
        stored.user_id_len = user_id_len;
    }
    if (user_name) {
        strncpy(stored.user_name, user_name, FIDO2_USER_NAME_MAX_LEN - 1);
    }
    stored.sign_count = 0;
    memcpy(stored.cred_id_nonce, nonce, 16);
    stored.flags = resident_key ? FIDO2_FLAG_RESIDENT : 0;
    stored.cred_protect = cred_protect;

    // Write to R-Memory (cache is automatically updated)
    uint16_t rmem_slot = FIDO2_RMEM_SLOT_BASE + slot;

    // Erase R-Memory slot first (handles case where slot has stale data)
    LOG_D("FIDO2", "Erasing R-Memory slot %d before write", rmem_slot);
    tropic01_rmem_erase(rmem_slot);

    if (!tropic01_rmem_write(rmem_slot, (const uint8_t *)&stored, FIDO2_STORED_SIZE)) {
        LOG_E("FIDO2", "Failed to write credential metadata");
        tropic01_ecc_key_erase(slot);
        return false;
    }

    // Update local cache
    g_storage.creds[slot].valid = true;
    memcpy(g_storage.creds[slot].rp_id_hash, rp_id_hash, 32);
    strncpy(g_storage.creds[slot].rp_id, stored.rp_id, FIDO2_RP_ID_MAX_LEN - 1);
    strncpy(g_storage.creds[slot].user_name, stored.user_name, FIDO2_USER_NAME_MAX_LEN - 1);
    g_storage.creds[slot].user_id_len = stored.user_id_len;
    g_storage.creds[slot].sign_count = 0;
    g_storage.creds[slot].resident = resident_key;
    g_storage.creds[slot].cred_protect = cred_protect;
    g_storage.cred_count++;

    // Copy public key output
    memcpy(out_pubkey, pubkey, 64);
    *out_slot = slot;

    LOG_I("FIDO2", "Created credential in slot %d", slot);
    return true;
}

bool fido2_storage_delete_credential(uint8_t slot) {
    if (slot > FIDO2_ECC_SLOT_MAX || !g_storage.creds[slot].valid) {
        return false;
    }

    LOG_I("FIDO2", "Deleting credential in slot %d", slot);

    // Erase ECC key (cache is automatically updated)
    if (!tropic01_ecc_key_erase(slot)) {
        LOG_W("FIDO2", "Failed to erase ECC key in slot %d", slot);
    }

    // Erase R-Memory (cache is automatically updated)
    uint16_t rmem_slot = FIDO2_RMEM_SLOT_BASE + slot;
    if (!tropic01_rmem_erase(rmem_slot)) {
        LOG_W("FIDO2", "Failed to erase R-Memory slot %d", rmem_slot);
    }

    // Update local cache
    g_storage.creds[slot].valid = false;
    g_storage.cred_count--;

    LOG_I("FIDO2", "Deleted credential in slot %d", slot);
    return true;
}

uint32_t fido2_storage_increment_sign_count(uint8_t slot) {
    if (slot > FIDO2_ECC_SLOT_MAX || !g_storage.creds[slot].valid) {
        return 0;
    }

    // Increment local cache
    g_storage.creds[slot].sign_count++;
    uint32_t new_count = g_storage.creds[slot].sign_count;

    // Read current stored data from TROPIC01
    uint16_t rmem_slot = FIDO2_RMEM_SLOT_BASE + slot;
    uint8_t data[256];
    uint16_t size;

    if (tropic01_rmem_read(rmem_slot, data, sizeof(data), &size)) {
        if (size >= FIDO2_STORED_SIZE) {
            fido2_stored_cred_t *stored = (fido2_stored_cred_t *)data;
            stored->sign_count = new_count;

            // Erase before write (R-Memory requires empty slot)
            if (!tropic01_rmem_erase(rmem_slot)) {
                LOG_E("FIDO2", "Failed to erase R-Memory slot %d for sign count update", rmem_slot);
                return new_count;  // Return updated count but don't persist
            }

            // Write back updated data
            if (!tropic01_rmem_write(rmem_slot, data, FIDO2_STORED_SIZE)) {
                LOG_E("FIDO2", "CRITICAL: Failed to write R-Memory slot %d after erase! Credential metadata LOST!", rmem_slot);
                // The R-Memory is now empty but ECC key still exists
                // This is a data loss situation
            }
        }
    }

    return new_count;
}

// ============================================================================
// Signing Operations (requires TROPIC01 access)
// ============================================================================

bool fido2_storage_sign(uint8_t slot, const uint8_t *hash,
                        uint8_t *signature, uint8_t *sig_len) {
    if (slot > FIDO2_ECC_SLOT_MAX || !g_storage.creds[slot].valid) {
        return false;
    }

    // ECDSA sign returns raw R||S (64 bytes)
    uint8_t raw_sig[64];
    if (!tropic01_ecdsa_sign(slot, hash, 32, raw_sig)) {
        LOG_E("FIDO2", "ECDSA sign failed for slot %d", slot);
        return false;
    }

    // Convert to DER format
    // DER: 0x30 LEN 0x02 R_LEN R 0x02 S_LEN S
    uint8_t *p = signature;
    *p++ = 0x30;  // SEQUENCE

    // Calculate lengths (handle leading zero for positive integers)
    uint8_t r_len = 32;
    uint8_t s_len = 32;
    uint8_t r_pad = (raw_sig[0] & 0x80) ? 1 : 0;
    uint8_t s_pad = (raw_sig[32] & 0x80) ? 1 : 0;

    // Skip leading zeros in R and S (but keep at least one byte)
    uint8_t r_skip = 0;
    while (r_skip < 31 && raw_sig[r_skip] == 0 && !(raw_sig[r_skip + 1] & 0x80)) {
        r_skip++;
    }
    uint8_t s_skip = 0;
    while (s_skip < 31 && raw_sig[32 + s_skip] == 0 && !(raw_sig[32 + s_skip + 1] & 0x80)) {
        s_skip++;
    }

    r_len = 32 - r_skip + r_pad;
    s_len = 32 - s_skip + s_pad;

    uint8_t total_len = 2 + r_len + 2 + s_len;
    *p++ = total_len;

    // R
    *p++ = 0x02;  // INTEGER
    *p++ = r_len;
    if (r_pad) *p++ = 0x00;
    memcpy(p, raw_sig + r_skip, 32 - r_skip);
    p += 32 - r_skip;

    // S
    *p++ = 0x02;  // INTEGER
    *p++ = s_len;
    if (s_pad) *p++ = 0x00;
    memcpy(p, raw_sig + 32 + s_skip, 32 - s_skip);
    p += 32 - s_skip;

    *sig_len = p - signature;

    LOG_D("FIDO2", "Signed with slot %d, sig_len=%d", slot, *sig_len);
    return true;
}

bool fido2_storage_sign_raw(uint8_t slot, const uint8_t *msg, uint16_t msg_len,
                            uint8_t *signature, uint8_t *sig_len) {
    if (slot > FIDO2_ECC_SLOT_MAX || !g_storage.creds[slot].valid) {
        return false;
    }

    // ECDSA sign: TROPIC01 hashes the message internally with SHA-256
    // Returns raw signature: r (32 bytes) || s (32 bytes) = 64 bytes
    // This is the format required by CTAP2/WebAuthn (IEEE P1363)
    if (!tropic01_ecdsa_sign(slot, msg, msg_len, signature)) {
        LOG_E("FIDO2", "ECDSA sign failed for slot %d", slot);
        return false;
    }

    *sig_len = 64;  // Raw signature is always 64 bytes for P-256
    LOG_D("FIDO2", "Signed raw %d bytes with slot %d, sig_len=%d", msg_len, slot, *sig_len);
    return true;
}

// Sign with DER-encoded output (for U2F compatibility)
bool fido2_storage_sign_der(uint8_t slot, const uint8_t *msg, uint16_t msg_len,
                            uint8_t *signature, uint8_t *sig_len) {
    if (slot > FIDO2_ECC_SLOT_MAX || !g_storage.creds[slot].valid) {
        return false;
    }

    // ECDSA sign: TROPIC01 hashes the message internally with SHA-256
    uint8_t raw_sig[64];
    if (!tropic01_ecdsa_sign(slot, msg, msg_len, raw_sig)) {
        LOG_E("FIDO2", "ECDSA sign failed for slot %d", slot);
        return false;
    }

    // Convert to DER format for U2F
    // DER: 0x30 LEN 0x02 R_LEN R 0x02 S_LEN S
    uint8_t *p = signature;
    *p++ = 0x30;  // SEQUENCE

    // Calculate lengths (handle leading zero for positive integers)
    uint8_t r_pad = (raw_sig[0] & 0x80) ? 1 : 0;
    uint8_t s_pad = (raw_sig[32] & 0x80) ? 1 : 0;

    // Skip leading zeros in R and S (but keep at least one byte)
    uint8_t r_skip = 0;
    while (r_skip < 31 && raw_sig[r_skip] == 0 && !(raw_sig[r_skip + 1] & 0x80)) {
        r_skip++;
    }
    uint8_t s_skip = 0;
    while (s_skip < 31 && raw_sig[32 + s_skip] == 0 && !(raw_sig[32 + s_skip + 1] & 0x80)) {
        s_skip++;
    }

    uint8_t r_len = 32 - r_skip + r_pad;
    uint8_t s_len = 32 - s_skip + s_pad;

    uint8_t total_len = 2 + r_len + 2 + s_len;
    *p++ = total_len;

    // R
    *p++ = 0x02;  // INTEGER
    *p++ = r_len;
    if (r_pad) *p++ = 0x00;
    memcpy(p, raw_sig + r_skip, 32 - r_skip);
    p += 32 - r_skip;

    // S
    *p++ = 0x02;  // INTEGER
    *p++ = s_len;
    if (s_pad) *p++ = 0x00;
    memcpy(p, raw_sig + 32 + s_skip, 32 - s_skip);
    p += 32 - s_skip;

    *sig_len = p - signature;

    LOG_D("FIDO2", "Signed DER %d bytes with slot %d, sig_len=%d", msg_len, slot, *sig_len);
    return true;
}

bool fido2_storage_get_pubkey(uint8_t slot, uint8_t *pubkey) {
    if (slot > FIDO2_ECC_SLOT_MAX) {
        return false;
    }

    // Try cache first
    if (tropic01_cache_ecc_exists(slot)) {
        uint8_t curve;
        return tropic01_cache_ecc_get_pubkey(slot, pubkey, &curve);
    }

    // Fall back to direct read
    uint8_t curve, origin;
    return tropic01_ecc_key_read(slot, pubkey, 65, &curve, &origin);
}
