// PIN Storage Module for CDC Badge
// Stores PIN hash securely on TROPIC01 R-Memory
// Format v2 (0xCC): [Magic (1)] [Badge hash (32)] [FIDO2 hash (16)] = 49 bytes
// - Badge hash = SHA256(MAC || PIN) - salted for rainbow table protection
// - FIDO2 hash = LEFT(SHA256(PIN), 16) - for CTAP2 ClientPIN protocol

#include "pin_storage.h"
#include "tropic01.h"
#include "cdc_log.h"
#include "psa/crypto.h"
#include "esp_mac.h"
#include <cstring>

#define PIN_HASH_SIZE 32         // SHA-256 output (badge hash)
#define PIN_FIDO2_HASH_SIZE 16   // LEFT(SHA256(PIN), 16) for FIDO2
#define PIN_MAGIC_V1 0xBB        // Old format (badge hash only)
#define PIN_MAGIC_V2 0xCC        // New format (badge + FIDO2 hash)
#define PIN_STORAGE_SIZE_V1 33   // Magic + Badge hash
#define PIN_STORAGE_SIZE_V2 49   // Magic + Badge hash + FIDO2 hash
#define PIN_SALT_SIZE 6          // ESP32 MAC address size

// Internal buffers (cached for fast validation)
static uint8_t g_pin_hash[PIN_HASH_SIZE] = {0};          // Badge hash
static uint8_t g_fido2_hash[PIN_FIDO2_HASH_SIZE] = {0};  // FIDO2 hash
static bool g_pin_loaded = false;
static bool g_fido2_hash_valid = false;  // True if FIDO2 hash is available

// Compute salted SHA-256 hash of PIN (for badge unlock)
// Hash = SHA256(MAC_ADDRESS || PIN)
static bool compute_pin_hash(const char *pin, uint8_t *hash_out) {
    if (!pin || !hash_out) return false;

    // Get device-unique salt (MAC address)
    uint8_t salt[PIN_SALT_SIZE];
    if (esp_efuse_mac_get_default(salt) != ESP_OK) {
        LOG_E("PIN", "Failed to get MAC for salt");
        return false;
    }

    size_t pin_len = strlen(pin);
    psa_status_t status;
    psa_hash_operation_t op = PSA_HASH_OPERATION_INIT;

    status = psa_hash_setup(&op, PSA_ALG_SHA_256);
    if (status != PSA_SUCCESS) {
        LOG_E("PIN", "Hash setup failed: %d", status);
        return false;
    }

    // Hash salt first, then PIN
    status = psa_hash_update(&op, salt, PIN_SALT_SIZE);
    if (status != PSA_SUCCESS) {
        psa_hash_abort(&op);
        LOG_E("PIN", "Hash update (salt) failed: %d", status);
        return false;
    }

    status = psa_hash_update(&op, (const uint8_t*)pin, pin_len);
    if (status != PSA_SUCCESS) {
        psa_hash_abort(&op);
        LOG_E("PIN", "Hash update (pin) failed: %d", status);
        return false;
    }

    size_t hash_len = PIN_HASH_SIZE;
    status = psa_hash_finish(&op, hash_out, PIN_HASH_SIZE, &hash_len);
    if (status != PSA_SUCCESS) {
        LOG_E("PIN", "Hash finish failed: %d", status);
        return false;
    }

    return true;
}

// Compute FIDO2 PIN hash (for CTAP2 ClientPIN)
// Hash = LEFT(SHA256(PIN), 16) - unsalted, truncated to 16 bytes
static bool compute_fido2_hash(const char *pin, uint8_t *hash_out) {
    if (!pin || !hash_out) return false;

    uint8_t full_hash[32];
    size_t hash_len = 32;

    psa_status_t status = psa_hash_compute(PSA_ALG_SHA_256,
                                            (const uint8_t*)pin, strlen(pin),
                                            full_hash, sizeof(full_hash), &hash_len);
    if (status != PSA_SUCCESS) {
        LOG_E("PIN", "FIDO2 hash failed: %d", status);
        return false;
    }

    // Take left 16 bytes
    memcpy(hash_out, full_hash, PIN_FIDO2_HASH_SIZE);
    return true;
}

// Load default PIN hashes (both badge and FIDO2)
static void load_default_hash(void) {
    compute_pin_hash(PIN_DEFAULT, g_pin_hash);
    compute_fido2_hash(PIN_DEFAULT, g_fido2_hash);
    g_fido2_hash_valid = true;
    LOG_I("PIN", "Using default PIN (both hashes)");
}

const char* pin_storage_load(void) {
    // This function ensures hash is loaded
    // Returns empty string as we don't store plain PIN

    if (g_pin_loaded) return "";

    // Check if TROPIC01 session is active
    if (!tropic01_session_active()) {
        LOG_W("PIN", "TROPIC01 session not active, using default");
        load_default_hash();
        g_pin_loaded = true;
        return "";
    }

    // Try to read PIN data from R-Memory (try v2 size first)
    uint8_t data[PIN_STORAGE_SIZE_V2];
    uint16_t read_size = 0;

    if (tropic01_rmem_read(TR01_RMEM_SLOT_PIN, data, PIN_STORAGE_SIZE_V2, &read_size)) {
        // Check for v2 format (badge + FIDO2 hash)
        if (read_size == PIN_STORAGE_SIZE_V2 && data[0] == PIN_MAGIC_V2) {
            memcpy(g_pin_hash, &data[1], PIN_HASH_SIZE);
            memcpy(g_fido2_hash, &data[1 + PIN_HASH_SIZE], PIN_FIDO2_HASH_SIZE);
            g_fido2_hash_valid = true;
            LOG_I("PIN", "Loaded PIN v2 (badge + FIDO2 hash) from TROPIC01");
            g_pin_loaded = true;
            return "";
        }
        // Check for v1 format (badge hash only)
        if (read_size == PIN_STORAGE_SIZE_V1 && data[0] == PIN_MAGIC_V1) {
            memcpy(g_pin_hash, &data[1], PIN_HASH_SIZE);
            g_fido2_hash_valid = false;  // No FIDO2 hash in v1
            LOG_W("PIN", "Loaded PIN v1 (badge only) - FIDO2 requires PIN reset");
            g_pin_loaded = true;
            return "";
        }
        LOG_W("PIN", "Invalid PIN data (size=%d, magic=0x%02X)", read_size, data[0]);
    }

    // Use default PIN hash if read failed or invalid
    LOG_I("PIN", "No valid PIN stored, using default");
    load_default_hash();
    g_pin_loaded = true;
    return "";
}

bool pin_storage_save(const char *pin) {
    if (!pin) return false;

    size_t len = strlen(pin);
    if (len < PIN_MIN_LEN || len > PIN_MAX_LEN) {
        LOG_E("PIN", "Invalid PIN length: %zu (must be %d-%d)", len, PIN_MIN_LEN, PIN_MAX_LEN);
        return false;
    }

    // Validate digits only
    for (size_t i = 0; i < len; i++) {
        if (pin[i] < '0' || pin[i] > '9') {
            LOG_E("PIN", "PIN must contain only digits");
            return false;
        }
    }

    // Compute both hashes
    uint8_t new_badge_hash[PIN_HASH_SIZE];
    uint8_t new_fido2_hash[PIN_FIDO2_HASH_SIZE];

    if (!compute_pin_hash(pin, new_badge_hash)) {
        return false;
    }
    if (!compute_fido2_hash(pin, new_fido2_hash)) {
        return false;
    }

    // Check if TROPIC01 session is active
    if (!tropic01_session_active()) {
        LOG_E("PIN", "TROPIC01 session not active");
        return false;
    }

    // Erase old data first
    tropic01_rmem_erase(TR01_RMEM_SLOT_PIN);

    // Build storage buffer v2: [Magic] [Badge Hash] [FIDO2 Hash]
    uint8_t storage[PIN_STORAGE_SIZE_V2];
    storage[0] = PIN_MAGIC_V2;
    memcpy(&storage[1], new_badge_hash, PIN_HASH_SIZE);
    memcpy(&storage[1 + PIN_HASH_SIZE], new_fido2_hash, PIN_FIDO2_HASH_SIZE);

    // Write to R-Memory
    if (tropic01_rmem_write(TR01_RMEM_SLOT_PIN, storage, PIN_STORAGE_SIZE_V2)) {
        memcpy(g_pin_hash, new_badge_hash, PIN_HASH_SIZE);
        memcpy(g_fido2_hash, new_fido2_hash, PIN_FIDO2_HASH_SIZE);
        g_fido2_hash_valid = true;
        LOG_I("PIN", "PIN v2 saved to TROPIC01 (badge + FIDO2 hash)");
        return true;
    }

    LOG_E("PIN", "Failed to save PIN");
    return false;
}

bool pin_storage_verify(const char *pin) {
    if (!pin) return false;

    // Load PIN hash if not already loaded
    if (!g_pin_loaded) {
        pin_storage_load();
    }

    // Compute hash of input PIN
    uint8_t input_hash[PIN_HASH_SIZE];
    if (!compute_pin_hash(pin, input_hash)) {
        return false;
    }

    // Constant-time comparison to prevent timing attacks
    uint8_t diff = 0;
    for (int i = 0; i < PIN_HASH_SIZE; i++) {
        diff |= g_pin_hash[i] ^ input_hash[i];
    }

    return diff == 0;
}

bool pin_storage_is_set(void) {
    if (!g_pin_loaded) {
        pin_storage_load();
    }

    // Compare with default PIN hash
    uint8_t default_hash[PIN_HASH_SIZE];
    if (!compute_pin_hash(PIN_DEFAULT, default_hash)) {
        return false;
    }

    uint8_t diff = 0;
    for (int i = 0; i < PIN_HASH_SIZE; i++) {
        diff |= g_pin_hash[i] ^ default_hash[i];
    }

    return diff != 0;  // Returns true if PIN is NOT the default
}

// ============================================================================
// FIDO2 ClientPIN Support
// ============================================================================

bool pin_storage_fido2_available(void) {
    if (!g_pin_loaded) {
        pin_storage_load();
    }
    return g_fido2_hash_valid;
}

bool pin_storage_get_fido2_hash(uint8_t *hash_out) {
    if (!hash_out) return false;

    if (!g_pin_loaded) {
        pin_storage_load();
    }

    if (!g_fido2_hash_valid) {
        LOG_W("PIN", "FIDO2 hash not available - PIN needs to be reset");
        return false;
    }

    memcpy(hash_out, g_fido2_hash, PIN_FIDO2_HASH_SIZE);
    return true;
}

bool pin_storage_verify_fido2_hash(const uint8_t *hash_in) {
    if (!hash_in) return false;

    if (!g_pin_loaded) {
        pin_storage_load();
    }

    if (!g_fido2_hash_valid) {
        LOG_W("PIN", "FIDO2 hash not available for verification");
        return false;
    }

    // Constant-time comparison
    uint8_t diff = 0;
    for (int i = 0; i < PIN_FIDO2_HASH_SIZE; i++) {
        diff |= g_fido2_hash[i] ^ hash_in[i];
    }

    return diff == 0;
}
