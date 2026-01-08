// TROPIC01 Cache Module
// Prevents chip lockout by caching metadata in RAM
//
// SECURITY: This cache NEVER stores secrets!
// - ECC slots: Only public keys (private keys never leave TROPIC01)
// - FIDO2 R-Memory (0-26): Metadata only (no private keys)
// - TOTP R-Memory (33-132): Metadata cached, but SECRET IS ZEROED OUT!
// - PIN R-Memory (30): NEVER cached (handled by pin_storage.cpp)

#include "tropic01_cache.h"
#include "tropic01.h"
#include "cdc_log.h"
#include <string.h>

// ECC cache structure
typedef struct {
    bool loaded;
    bool slot_used[TR01_CACHE_ECC_SLOTS];
    uint8_t pubkey[TR01_CACHE_ECC_SLOTS][64];
    uint8_t curve[TR01_CACHE_ECC_SLOTS];
} ecc_cache_t;

// R-Memory cache structure
typedef struct {
    bool loaded;
    bool slot_used[TR01_CACHE_RMEM_SLOTS];
    uint8_t data[TR01_CACHE_RMEM_SLOTS][TR01_CACHE_DATA_SIZE];
    uint16_t data_size[TR01_CACHE_RMEM_SLOTS];
} rmem_cache_t;

static ecc_cache_t g_ecc_cache = {};
static rmem_cache_t g_rmem_cache = {};

void tropic01_cache_init(void) {
    LOG_I("TR01_CACHE", "Loading cache from TROPIC01...");

    memset(&g_ecc_cache, 0, sizeof(g_ecc_cache));
    memset(&g_rmem_cache, 0, sizeof(g_rmem_cache));

    // Scan ECC slots (0-31)
    for (int i = 0; i < TR01_CACHE_ECC_SLOTS; i++) {
        uint8_t pubkey[64], curve, origin;
        if (tropic01_ecc_key_read(i, pubkey, 64, &curve, &origin)) {
            g_ecc_cache.slot_used[i] = true;
            memcpy(g_ecc_cache.pubkey[i], pubkey, 64);
            g_ecc_cache.curve[i] = curve;
            // Log with slot type for easier debugging
            const char* slot_type = (i <= 26) ? "FIDO2" : (i <= 29) ? "SSH" : (i == 30) ? "Attest" : "CA";
            LOG_I("TR01_CACHE", "ECC slot %d (%s): curve=%d", i, slot_type, curve);
        }
    }

    // Load R-Memory for FIDO2 (0-26)
    for (int i = TR01_RMEM_SLOT_FIDO_START; i <= TR01_RMEM_SLOT_FIDO_END; i++) {
        uint16_t read_size = 0;
        if (tropic01_rmem_read(i, g_rmem_cache.data[i], TR01_CACHE_DATA_SIZE, &read_size)) {
            if (read_size > 0) {
                g_rmem_cache.slot_used[i] = true;
                g_rmem_cache.data_size[i] = read_size;
                // Log magic bytes for debugging
                LOG_I("TR01_CACHE", "R-Memory slot %d: %d bytes, magic='%.4s'",
                      i, read_size, (char*)g_rmem_cache.data[i]);
            } else if (g_ecc_cache.slot_used[i]) {
                // ECC key exists but R-Memory is empty - data loss!
                LOG_W("TR01_CACHE", "R-Memory slot %d EMPTY but ECC key exists!", i);
            }
        }
    }

    // Load R-Memory for TOTP (33-132)
    // SECURITY: Cache metadata but ZERO OUT the HMAC secret!
    // TOTP structure: magic(1) + name(32) + issuer(32) + SECRET(32) + secret_len(1) + digits(1) + period(4) + algo(1) + flags(1)
    //                 Offset 0    1-32       33-64        65-96        97            98          99-102      103       104
    for (int i = TR01_RMEM_SLOT_TOTP_START; i <= TR01_RMEM_SLOT_TOTP_END; i++) {
        uint16_t read_size = 0;
        if (tropic01_rmem_read(i, g_rmem_cache.data[i], TR01_CACHE_DATA_SIZE, &read_size)) {
            if (read_size > 0 && g_rmem_cache.data[i][0] == 0xAA) {  // 0xAA = valid TOTP entry
                g_rmem_cache.slot_used[i] = true;
                g_rmem_cache.data_size[i] = read_size;
                // SECURITY: Zero out the secret (bytes 65-96) and secret_len (byte 97)
                memset(&g_rmem_cache.data[i][65], 0, 33);  // 32 bytes secret + 1 byte secret_len
                LOG_D("TR01_CACHE", "TOTP slot %d: %s", i, (char*)&g_rmem_cache.data[i][1]);  // Log name only
            }
        }
    }

    g_ecc_cache.loaded = true;
    g_rmem_cache.loaded = true;

    uint8_t ecc_count = tropic01_cache_ecc_count();
    uint16_t fido_count = tropic01_cache_rmem_count_range(TR01_RMEM_SLOT_FIDO_START, TR01_RMEM_SLOT_FIDO_END);
    uint16_t totp_count = tropic01_cache_rmem_count_range(TR01_RMEM_SLOT_TOTP_START, TR01_RMEM_SLOT_TOTP_END);

    LOG_I("TR01_CACHE", "Cache loaded: %d ECC keys, %d FIDO2, %d TOTP",
          ecc_count, fido_count, totp_count);
}

bool tropic01_cache_is_loaded(void) {
    return g_ecc_cache.loaded && g_rmem_cache.loaded;
}

bool tropic01_cache_ecc_exists(uint8_t slot) {
    if (slot >= TR01_CACHE_ECC_SLOTS) return false;
    return g_ecc_cache.loaded && g_ecc_cache.slot_used[slot];
}

bool tropic01_cache_ecc_get_pubkey(uint8_t slot, uint8_t *pubkey, uint8_t *curve) {
    if (slot >= TR01_CACHE_ECC_SLOTS || !g_ecc_cache.loaded || !g_ecc_cache.slot_used[slot]) {
        return false;
    }
    if (pubkey) memcpy(pubkey, g_ecc_cache.pubkey[slot], 64);
    if (curve) *curve = g_ecc_cache.curve[slot];
    return true;
}

uint8_t tropic01_cache_ecc_count(void) {
    uint8_t count = 0;
    for (int i = 0; i < TR01_CACHE_ECC_SLOTS; i++) {
        if (g_ecc_cache.slot_used[i]) count++;
    }
    return count;
}

void tropic01_cache_ecc_update(uint8_t slot, const uint8_t *pubkey, uint8_t curve) {
    if (slot >= TR01_CACHE_ECC_SLOTS) return;
    g_ecc_cache.slot_used[slot] = true;
    if (pubkey) memcpy(g_ecc_cache.pubkey[slot], pubkey, 64);
    g_ecc_cache.curve[slot] = curve;
}

void tropic01_cache_ecc_invalidate(uint8_t slot) {
    if (slot >= TR01_CACHE_ECC_SLOTS) return;
    g_ecc_cache.slot_used[slot] = false;
    memset(g_ecc_cache.pubkey[slot], 0, 64);
    g_ecc_cache.curve[slot] = 0;
}

bool tropic01_cache_rmem_exists(uint16_t slot) {
    if (slot >= TR01_CACHE_RMEM_SLOTS) return false;
    return g_rmem_cache.loaded && g_rmem_cache.slot_used[slot];
}

bool tropic01_cache_rmem_get(uint16_t slot, uint8_t *data, uint16_t *size) {
    if (slot >= TR01_CACHE_RMEM_SLOTS || !g_rmem_cache.loaded || !g_rmem_cache.slot_used[slot]) {
        return false;
    }
    // SECURITY: TOTP slots have metadata but secret is zeroed out
    // Caller must read from TROPIC01 directly if they need the actual secret for TOTP generation
    if (data) memcpy(data, g_rmem_cache.data[slot], g_rmem_cache.data_size[slot]);
    if (size) *size = g_rmem_cache.data_size[slot];
    return true;
}

uint16_t tropic01_cache_rmem_count_range(uint16_t start, uint16_t end) {
    uint16_t count = 0;
    if (end >= TR01_CACHE_RMEM_SLOTS) end = TR01_CACHE_RMEM_SLOTS - 1;
    for (uint16_t i = start; i <= end; i++) {
        if (g_rmem_cache.slot_used[i]) count++;
    }
    return count;
}

void tropic01_cache_rmem_update(uint16_t slot, const uint8_t *data, uint16_t size) {
    if (slot >= TR01_CACHE_RMEM_SLOTS) return;
    g_rmem_cache.slot_used[slot] = true;

    uint16_t copy_size = (size > TR01_CACHE_DATA_SIZE) ? TR01_CACHE_DATA_SIZE : size;
    if (data) memcpy(g_rmem_cache.data[slot], data, copy_size);
    g_rmem_cache.data_size[slot] = copy_size;

    // SECURITY: Zero out TOTP secrets after caching metadata
    if (slot >= TR01_RMEM_SLOT_TOTP_START && slot <= TR01_RMEM_SLOT_TOTP_END) {
        memset(&g_rmem_cache.data[slot][65], 0, 33);  // Zero secret + secret_len
    }
}

void tropic01_cache_rmem_invalidate(uint16_t slot) {
    if (slot >= TR01_CACHE_RMEM_SLOTS) return;
    g_rmem_cache.slot_used[slot] = false;
    memset(g_rmem_cache.data[slot], 0, TR01_CACHE_DATA_SIZE);
    g_rmem_cache.data_size[slot] = 0;
}
