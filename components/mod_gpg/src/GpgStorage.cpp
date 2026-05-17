/**
 * GPG Storage Layer
 *
 * Manages ECC slots and R-Memory for GPG keys.
 * DEC private key is stored encrypted in R-Memory for software ECDH.
 *
 * SECURITY NOTE:
 * The TROPIC01 secure element does NOT support native ECDH operations.
 * See docs/GPG_ECDH_SECURITY.md for security analysis.
 */

#include "mod_gpg/GpgStorage.h"
#include "cdc_hal/ISecureElement.h"
#include "cdc_log.h"
#include <mbedtls/gcm.h>
#include <mbedtls/sha256.h>
#include <mbedtls/hkdf.h>
#include <mbedtls/md.h>
#include <mbedtls/platform_util.h>
#include <esp_random.h>
#include <string.h>

static const char* TAG = "GPGStorage";

/**
 * \brief R-Memory slot index used for DEC private key payload within module range.
 */
static constexpr uint16_t RMEM_SLOT_DEC_KEY = 0;

/**
 * \brief Magic marker used to validate encrypted DEC key records.
 */
static constexpr uint8_t DEC_KEY_MAGIC[4] = {'E', 'C', 'D', 'H'};

/**
 * \brief Serialized encrypted DEC key record field sizes.
 */
static constexpr size_t MAGIC_SIZE = 4;
static constexpr size_t NONCE_SIZE = 12;
static constexpr size_t PRIVKEY_SIZE = 32;
static constexpr size_t TAG_SIZE = 16;
static constexpr size_t TOTAL_SIZE = MAGIC_SIZE + NONCE_SIZE + PRIVKEY_SIZE + TAG_SIZE;

/**
 * \brief HKDF info string for PIN-derived DEC key encryption context.
 */
static constexpr char HKDF_INFO[] = "GPG-DEC-KEY-V1";

#ifdef __DOXYGEN__
namespace cdc::mod_gpg {
#endif

#pragma pack(push, 1)
struct DecKeyStorage {
    uint8_t magic[MAGIC_SIZE];     // "ECDH"
    uint8_t nonce[NONCE_SIZE];     // AES-GCM nonce
    uint8_t encrypted[PRIVKEY_SIZE]; // Encrypted private key
    uint8_t tag[TAG_SIZE];         // GCM authentication tag
};
#pragma pack(pop)

#ifdef __DOXYGEN__
} // namespace cdc::mod_gpg
#endif

static_assert(sizeof(DecKeyStorage) == TOTAL_SIZE, "DecKeyStorage size mismatch");

namespace {

/**
 * \brief RAII wrapper around `mbedtls_gcm_context`.
 *
 * Ensures `mbedtls_gcm_init()` is paired with `mbedtls_gcm_free()` on
 * scope exit, removing the need for `goto cleanup` constructs.
 */
class GcmContext {
public:
    /** \brief Initializes the underlying mbedTLS GCM context. */
    GcmContext() { mbedtls_gcm_init(&ctx_); }

    /** \brief Releases mbedTLS GCM resources. */
    ~GcmContext() { mbedtls_gcm_free(&ctx_); }

    GcmContext(const GcmContext&) = delete;
    GcmContext& operator=(const GcmContext&) = delete;
    GcmContext(GcmContext&&) = delete;
    GcmContext& operator=(GcmContext&&) = delete;

    /** \brief Returns mutable pointer to the wrapped mbedTLS context. */
    mbedtls_gcm_context* get() { return &ctx_; }

private:
    mbedtls_gcm_context ctx_;
};

/**
 * \brief Securely zeroizes a fixed-size buffer.
 * \tparam N Buffer length in bytes.
 * \param buf Buffer to wipe.
 */
template <size_t N>
inline void secureWipe(uint8_t (&buf)[N]) {
    mbedtls_platform_zeroize(buf, N);
}

/**
 * \brief Securely zeroizes a typed object.
 * \tparam T Object type.
 * \param obj Object reference to wipe.
 */
template <typename T>
inline void secureWipeObject(T& obj) {
    mbedtls_platform_zeroize(&obj, sizeof(obj));
}

} // namespace

static struct {
    bool ready = false;
    uint16_t eccStart = 0;
    uint16_t eccEnd = 0;
    uint16_t rmemStart = 0;
    uint16_t rmemEnd = 0;
    uint8_t sigSlot = 0;
    uint8_t decSlot = 0;
    uint8_t autSlot = 0;

    // Session state for verified PIN
    bool sessionActive = false;
    uint8_t sessionKey[32];  // HKDF-derived key from PIN
} s_storage;

/**
 * \brief Returns secure-element instance used by storage helpers.
 * \return Pointer to secure-element abstraction.
 */
static cdc::hal::ISecureElement* get_se() {
    return cdc::hal::getSecureElementInstance();
}

/**
 * \brief Derives a device-bound encryption key using HKDF-SHA256.
 * \param key_out Output buffer receiving the 32-byte derived key.
 * \return `true` on successful key derivation, otherwise `false`.
 */
static bool derive_device_key(uint8_t* key_out) {
    if (!key_out) return false;

    // Get chip ID as IKM (Input Keying Material)
    uint8_t chip_id[16] = {};
    auto* se = get_se();
    if (se) {
        se->getChipId(chip_id, sizeof(chip_id));
    }

    // Fixed salt for device key (different from PIN key)
    static constexpr char DEVICE_KEY_INFO[] = "GPG-DEC-DEVICE-KEY-V1";
    static const uint8_t DEVICE_SALT[16] = {
        0x47, 0x50, 0x47, 0x2D, 0x44, 0x45, 0x43, 0x2D,  // "GPG-DEC-"
        0x53, 0x41, 0x4C, 0x54, 0x2D, 0x56, 0x31, 0x00   // "SALT-V1."
    };

    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md) return false;

    int ret = mbedtls_hkdf(
        md,
        DEVICE_SALT, sizeof(DEVICE_SALT),
        chip_id, sizeof(chip_id),
        reinterpret_cast<const uint8_t*>(DEVICE_KEY_INFO), strlen(DEVICE_KEY_INFO),
        key_out, 32
    );

    mbedtls_platform_zeroize(chip_id, sizeof(chip_id));
    return ret == 0;
}

/**
 * \brief Derives an encryption key from the user PIN or falls back to a device key.
 * \param pin PIN string used as input key material; may be `nullptr`.
 * \param key_out Output buffer receiving the 32-byte derived key.
 * \return `true` if derivation succeeded, otherwise `false`.
 */
static bool derive_key_from_pin(const char* pin, uint8_t* key_out) {
    if (!key_out) return false;

    // If no PIN provided, use device-specific key
    if (!pin || pin[0] == '\0') {
        return derive_device_key(key_out);
    }

    // Get chip ID as salt (unique per device)
    uint8_t salt[16] = {};
    auto* se = get_se();
    if (se) {
        se->getChipId(salt, sizeof(salt));
    }

    // HKDF: PIN -> 32-byte key
    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md) return false;

    int ret = mbedtls_hkdf(
        md,
        salt, sizeof(salt),
        reinterpret_cast<const uint8_t*>(pin), strlen(pin),
        reinterpret_cast<const uint8_t*>(HKDF_INFO), strlen(HKDF_INFO),
        key_out, 32
    );

    return ret == 0;
}

/**
 * \brief Configures ECC slot range and derives SIG/DEC/AUT slot assignments.
 * \param eccStart First ECC slot assigned to OpenPGP module.
 * \param eccEnd Last ECC slot assigned to OpenPGP module.
 */
void gpg_storage_set_slot_range(uint16_t eccStart, uint16_t eccEnd) {
    s_storage.ready = false;
    s_storage.eccStart = eccStart;
    s_storage.eccEnd = eccEnd;

    if (eccStart == 0 || eccEnd == 0) {
        return;
    }
    if (eccStart > eccEnd) {
        return;
    }
    if ((eccEnd - eccStart + 1) < 3) {
        return;
    }

    s_storage.sigSlot = static_cast<uint8_t>(eccStart);
    s_storage.decSlot = static_cast<uint8_t>(eccStart + 1);
    s_storage.autSlot = static_cast<uint8_t>(eccStart + 2);
    s_storage.ready = true;
}

/**
 * \brief Configures R-Memory slot range used by OpenPGP storage.
 * \param rmemStart First assigned R-Memory slot.
 * \param rmemEnd Last assigned R-Memory slot.
 */
void gpg_storage_set_rmem_range(uint16_t rmemStart, uint16_t rmemEnd) {
    s_storage.rmemStart = rmemStart;
    s_storage.rmemEnd = rmemEnd;
}

/**
 * \brief Returns whether storage slot configuration is complete and usable.
 * \return `true` when storage ranges are valid.
 */
bool gpg_storage_ready(void) {
    return s_storage.ready;
}

/**
 * \brief Returns configured slot ID for signature key material.
 * \return Signature ECC slot index.
 */
uint8_t gpg_storage_sig_slot(void) {
    return s_storage.sigSlot;
}

/**
 * \brief Returns configured slot ID for decryption key material.
 * \return Decryption ECC slot index.
 */
uint8_t gpg_storage_dec_slot(void) {
    return s_storage.decSlot;
}

/**
 * \brief Returns configured slot ID for authentication key material.
 * \return Authentication ECC slot index.
 */
uint8_t gpg_storage_aut_slot(void) {
    return s_storage.autSlot;
}

/**
 * \brief Encrypted DEC private-key storage operations.
 */

/**
 * \brief Encrypts and stores DEC private key into module R-Memory.
 * \param privkey Raw 32-byte DEC private key.
 * \param pin Optional PIN string for key derivation context.
 * \return `true` if encrypted record was written successfully.
 */
bool gpg_storage_save_dec_privkey(const uint8_t* privkey, const char* pin) {
    if (!privkey) {
        LOG_E(TAG, "Invalid parameters for save_dec_privkey");
        return false;
    }
    // Note: pin can be NULL - derive_key_from_pin will use device key in that case

    auto* se = get_se();
    if (!se) {
        LOG_E(TAG, "Secure element not available");
        return false;
    }

    // Derive encryption key from PIN. Wiped on scope exit via secureWipe().
    uint8_t enc_key[32];
    if (!derive_key_from_pin(pin, enc_key)) {
        LOG_E(TAG, "Failed to derive encryption key");
        return false;
    }

    // Prepare storage structure (wiped on scope exit).
    DecKeyStorage storage = {};
    memcpy(storage.magic, DEC_KEY_MAGIC, MAGIC_SIZE);

    const uint16_t rmem_slot = s_storage.rmemStart + RMEM_SLOT_DEC_KEY;

    // Generate random nonce
    if (!se->getRandom(storage.nonce, NONCE_SIZE)) {
        esp_fill_random(storage.nonce, NONCE_SIZE);
    }

    // RAII-managed GCM context: mbedtls_gcm_free() runs automatically.
    GcmContext gcm;
    bool success = false;

    int ret = mbedtls_gcm_setkey(gcm.get(), MBEDTLS_CIPHER_ID_AES, enc_key, 256);
    if (ret == 0) {
        ret = mbedtls_gcm_crypt_and_tag(
            gcm.get(),
            MBEDTLS_GCM_ENCRYPT,
            PRIVKEY_SIZE,
            storage.nonce, NONCE_SIZE,
            storage.magic, MAGIC_SIZE,  // AAD = magic bytes
            privkey,
            storage.encrypted,
            TAG_SIZE,
            storage.tag
        );
        if (ret != 0) {
            LOG_E(TAG, "GCM encrypt failed: %d", ret);
        }
    } else {
        LOG_E(TAG, "GCM setkey failed: %d", ret);
    }

    if (ret == 0) {
        // Erase existing data first
        se->rmemErase(rmem_slot);

        // Write encrypted key to R-Memory
        if (se->rmemWrite(rmem_slot, reinterpret_cast<uint8_t*>(&storage), TOTAL_SIZE)
                == cdc::hal::SeResult::OK) {
            LOG_I(TAG, "Saved encrypted DEC private key to R-Memory slot %d", rmem_slot);
            success = true;
        } else {
            LOG_E(TAG, "Failed to write encrypted DEC key to R-Memory slot %d", rmem_slot);
        }
    }

    secureWipe(enc_key);
    secureWipeObject(storage);
    return success;
}

/**
 * \brief Loads and decrypts DEC private key from module R-Memory.
 * \param privkey_out Output buffer for decrypted 32-byte private key.
 * \param pin Optional PIN string for key derivation context.
 * \return `true` if key was successfully decrypted and validated.
 */
bool gpg_storage_load_dec_privkey(uint8_t* privkey_out, const char* pin) {
    if (!privkey_out) {
        LOG_E(TAG, "Invalid parameters for load_dec_privkey");
        return false;
    }
    // Note: pin can be NULL - derive_key_from_pin will use device key in that case

    auto* se = get_se();
    if (!se) {
        LOG_E(TAG, "Secure element not available");
        return false;
    }

    // Read from R-Memory
    uint16_t rmem_slot = s_storage.rmemStart + RMEM_SLOT_DEC_KEY;
    uint8_t data[128];
    uint16_t data_len = 0;

    if (se->rmemRead(rmem_slot, data, sizeof(data), &data_len) != cdc::hal::SeResult::OK ||
        data_len < TOTAL_SIZE) {
        LOG_W(TAG, "No DEC private key in R-Memory slot %d", rmem_slot);
        return false;
    }

    auto* storage = reinterpret_cast<DecKeyStorage*>(data);

    // Verify magic
    if (memcmp(storage->magic, DEC_KEY_MAGIC, MAGIC_SIZE) != 0) {
        LOG_W(TAG, "Invalid magic in DEC key storage");
        return false;
    }

    // Derive decryption key from PIN. Wiped on scope exit via secureWipe().
    uint8_t dec_key[32];
    if (!derive_key_from_pin(pin, dec_key)) {
        LOG_E(TAG, "Failed to derive decryption key");
        return false;
    }

    // RAII-managed GCM context: mbedtls_gcm_free() runs automatically.
    GcmContext gcm;
    bool success = false;

    int ret = mbedtls_gcm_setkey(gcm.get(), MBEDTLS_CIPHER_ID_AES, dec_key, 256);
    if (ret == 0) {
        ret = mbedtls_gcm_auth_decrypt(
            gcm.get(),
            PRIVKEY_SIZE,
            storage->nonce, NONCE_SIZE,
            storage->magic, MAGIC_SIZE,  // AAD = magic bytes
            storage->tag, TAG_SIZE,
            storage->encrypted,
            privkey_out
        );
        if (ret == 0) {
            LOG_D(TAG, "Successfully decrypted DEC private key");
            success = true;
        } else {
            LOG_W(TAG, "GCM decrypt failed (wrong PIN or corrupted data): %d", ret);
            mbedtls_platform_zeroize(privkey_out, PRIVKEY_SIZE);
        }
    } else {
        LOG_E(TAG, "GCM setkey failed: %d", ret);
    }

    secureWipe(dec_key);
    mbedtls_platform_zeroize(data, sizeof(data));
    return success;
}

/**
 * \brief Checks whether encrypted DEC private key record exists in R-Memory.
 * \return `true` if a valid magic marker is present.
 */
bool gpg_storage_has_dec_privkey(void) {
    auto* se = get_se();
    if (!se) return false;

    uint16_t rmem_slot = s_storage.rmemStart + RMEM_SLOT_DEC_KEY;
    uint8_t data[MAGIC_SIZE + 1];
    uint16_t data_len = 0;

    if (se->rmemRead(rmem_slot, data, sizeof(data), &data_len) != cdc::hal::SeResult::OK ||
        data_len < MAGIC_SIZE) {
        return false;
    }

    return memcmp(data, DEC_KEY_MAGIC, MAGIC_SIZE) == 0;
}

/**
 * \brief Deletes stored encrypted DEC private key from R-Memory.
 * \return `true` if erase operation succeeded.
 */
bool gpg_storage_delete_dec_privkey(void) {
    auto* se = get_se();
    if (!se) return false;

    uint16_t rmem_slot = s_storage.rmemStart + RMEM_SLOT_DEC_KEY;

    if (se->rmemErase(rmem_slot) != cdc::hal::SeResult::OK) {
        LOG_E(TAG, "Failed to erase DEC key from R-Memory slot %d", rmem_slot);
        return false;
    }

    LOG_I(TAG, "Deleted DEC private key from R-Memory slot %d", rmem_slot);
    return true;
}

/**
 * \brief Session key management for verified PIN context.
 */

/**
 * \brief Derives and stores session key from verified PIN.
 * \param pin Verified PIN string, or `nullptr` to clear session.
 */
void gpg_storage_set_session_pin(const char* pin) {
    if (!pin) {
        gpg_storage_clear_session();
        return;
    }

    if (derive_key_from_pin(pin, s_storage.sessionKey)) {
        s_storage.sessionActive = true;
        LOG_D(TAG, "Session key derived from PIN");
    }
}

/**
 * \brief Returns current session key if session is active.
 * \param key_out Output buffer for 32-byte session key.
 * \return `true` when session key is available.
 */
bool gpg_storage_get_session_key(uint8_t* key_out) {
    if (!s_storage.sessionActive || !key_out) {
        return false;
    }

    memcpy(key_out, s_storage.sessionKey, 32);
    return true;
}

/**
 * \brief Clears session key material from memory.
 */
void gpg_storage_clear_session(void) {
    mbedtls_platform_zeroize(s_storage.sessionKey, sizeof(s_storage.sessionKey));
    s_storage.sessionActive = false;
    LOG_D(TAG, "Session cleared");
}
