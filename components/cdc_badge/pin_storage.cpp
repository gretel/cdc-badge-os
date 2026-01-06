// PIN Storage Module for CDC Badge
// Stores PIN hash securely on TROPIC01 R-Memory
// Format: [Magic 0xAA (1 byte)] [SHA-256 hash (32 bytes)] = 33 bytes

#include "pin_storage.h"
#include "tropic01.h"
#include "cdc_log.h"
#include "psa/crypto.h"
#include <cstring>

#define PIN_HASH_SIZE 32      // SHA-256 output
#define PIN_MAGIC 0xAA        // Magic byte to validate stored data
#define PIN_STORAGE_SIZE 33   // Magic + Hash

// Internal buffer for PIN hash (cached for fast validation)
static uint8_t g_pin_hash[PIN_HASH_SIZE] = {0};
static bool g_pin_loaded = false;

// Compute SHA-256 hash of PIN
static bool compute_pin_hash(const char *pin, uint8_t *hash_out) {
    if (!pin || !hash_out) return false;

    size_t pin_len = strlen(pin);
    size_t hash_len = PIN_HASH_SIZE;
    psa_status_t status;

    status = psa_hash_compute(PSA_ALG_SHA_256,
                              (const uint8_t*)pin, pin_len,
                              hash_out, PIN_HASH_SIZE, &hash_len);

    if (status != PSA_SUCCESS) {
        LOG_E("PIN", "Hash compute failed: %d", status);
        return false;
    }

    return true;
}

// Load default PIN hash
static void load_default_hash(void) {
    compute_pin_hash(PIN_DEFAULT, g_pin_hash);
    LOG_I("PIN", "Using default PIN hash");
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

    // Try to read PIN data from R-Memory
    uint8_t data[PIN_STORAGE_SIZE];
    uint16_t read_size = 0;

    if (tropic01_rmem_read(TR01_RMEM_SLOT_PIN, data, PIN_STORAGE_SIZE, &read_size)) {
        // Validate magic byte and size
        if (read_size == PIN_STORAGE_SIZE && data[0] == PIN_MAGIC) {
            memcpy(g_pin_hash, &data[1], PIN_HASH_SIZE);
            LOG_I("PIN", "Loaded PIN hash from TROPIC01");
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

    // Compute hash
    uint8_t new_hash[PIN_HASH_SIZE];
    if (!compute_pin_hash(pin, new_hash)) {
        return false;
    }

    // Check if TROPIC01 session is active
    if (!tropic01_session_active()) {
        LOG_E("PIN", "TROPIC01 session not active");
        return false;
    }

    // Erase old data first
    tropic01_rmem_erase(TR01_RMEM_SLOT_PIN);

    // Build storage buffer: [Magic] [Hash]
    uint8_t storage[PIN_STORAGE_SIZE];
    storage[0] = PIN_MAGIC;
    memcpy(&storage[1], new_hash, PIN_HASH_SIZE);

    // Write to R-Memory
    if (tropic01_rmem_write(TR01_RMEM_SLOT_PIN, storage, PIN_STORAGE_SIZE)) {
        memcpy(g_pin_hash, new_hash, PIN_HASH_SIZE);
        LOG_I("PIN", "PIN hash saved to TROPIC01");
        return true;
    }

    LOG_E("PIN", "Failed to save PIN hash");
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
