#pragma once

#include <cstdint>
#include <cstddef>

namespace cdc::core {

/**
 * PIN Manager - Manages all device PINs in TROPIC01 R-Memory Slot 0
 *
 * Storage Format (106 bytes):
 * [Magic 0xDD]           (1)  - Format identifier
 * [Badge/FIDO2 Hash]     (16) - LEFT(SHA256(PIN), 16)
 * [Badge Retries]        (1)  - Remaining attempts for Badge PIN
 * [KDF Algorithm]        (1)  - 0x03 = KDF_ITERSALTED_S2K
 * [Hash Algorithm]       (1)  - 0x08 = SHA256
 * [Iteration Count]      (4)  - Default 100000
 * [PW1 Salt]             (8)  - Random salt for User PIN
 * [PW3 Salt]             (8)  - Random salt for Admin PIN
 * [PW1 Hash]             (32) - KDF hash of User PIN
 * [PW3 Hash]             (32) - KDF hash of Admin PIN
 * [PW1 Retries]          (1)  - Remaining attempts
 * [PW3 Retries]          (1)  - Remaining attempts
 *
 * Defaults:
 * - Badge/FIDO2: "123456"
 * - OpenPGP PW1 (User): "123456" (min 6 digits)
 * - OpenPGP PW3 (Admin): "12345678" (min 8 digits)
 */
class PinManager {
public:
    // PIN constraints
    static constexpr uint8_t BADGE_PIN_MIN = 4;
    static constexpr uint8_t BADGE_PIN_MAX = 8;
    static constexpr uint8_t PW1_MIN = 6;
    static constexpr uint8_t PW3_MIN = 8;
    static constexpr uint8_t PIN_MAX = 16;

    // Storage
    static constexpr uint16_t RMEM_SLOT_PIN = 0;

    // Hash sizes
    static constexpr uint8_t BADGE_HASH_SIZE = 16;  // LEFT(SHA256, 16)
    static constexpr uint8_t KDF_HASH_SIZE = 32;    // Full SHA256
    static constexpr uint8_t SALT_SIZE = 8;

    // KDF parameters (OpenPGP spec)
    static constexpr uint8_t KDF_ITERSALTED_S2K = 0x03;
    static constexpr uint8_t HASH_SHA256 = 0x08;
    static constexpr uint32_t DEFAULT_ITERATIONS = 100000;

    // Defaults
    static constexpr const char* DEFAULT_BADGE_PIN = "123456";
    static constexpr const char* DEFAULT_PW1 = "123456";
    static constexpr const char* DEFAULT_PW3 = "12345678";

    static PinManager& instance();
    bool init();

    // === Badge/FIDO2 PIN ===
    bool verifyBadgePin(const char* pin);
    bool changeBadgePin(const char* currentPin, const char* newPin);
    bool setBadgePin(const char* newPin);
    bool getBadgePinHash(uint8_t* hashOut) const;
    bool verifyBadgePinHash(const uint8_t* hashIn) const;
    uint8_t getBadgeRetries() const { return badgeRetries_; }
    bool isBadgeBlocked() const;  // Checks retries=0 OR time lockout active
    void resetBadgeRetries();

    // === Lockout Timer (RAM only, not persistent) ===
    static constexpr uint32_t LOCKOUT_DURATION_MS = 60000;  // 60 seconds
    void startLockout();
    uint32_t getLockoutRemainingMs() const;
    bool isLockoutActive() const;

    /**
     * \brief Clears expired lockout state and resets retry counter.
     *
     * Call this from non-const contexts to perform the lazy state update
     * that `isLockoutActive()` only observes.
     */
    void checkAndResetExpiredLockout();

    // === OpenPGP PW1 (User PIN) ===
    bool verifyPW1(const char* pin);
    bool changePW1(const char* currentPin, const char* newPin);
    bool setPW1(const char* newPin);
    bool getPW1Hash(uint8_t* hashOut) const;
    bool getPW1Salt(uint8_t* saltOut) const;
    uint8_t getPW1Retries() const { return pw1Retries_; }
    bool isPW1Blocked() const { return pw1Retries_ == 0; }
    void resetPW1Retries();

    // === OpenPGP PW3 (Admin PIN) ===
    bool verifyPW3(const char* pin);
    bool changePW3(const char* currentPin, const char* newPin);
    bool setPW3(const char* newPin);
    bool getPW3Hash(uint8_t* hashOut) const;
    bool getPW3Salt(uint8_t* saltOut) const;
    uint8_t getPW3Retries() const { return pw3Retries_; }
    bool isPW3Blocked() const { return pw3Retries_ == 0; }
    void resetPW3Retries();

    // === KDF Parameters (for OpenPGP KDF-DO) ===
    uint8_t getKdfAlgorithm() const { return KDF_ITERSALTED_S2K; }
    uint8_t getHashAlgorithm() const { return HASH_SHA256; }
    uint32_t getIterationCount() const { return iterations_; }

    // === Status ===
    bool isPinSet() const { return badgePinIsSet_; }
    bool isStorageAvailable() const;

private:
    PinManager() = default;

    static constexpr uint8_t MAX_RETRIES = 3;
    static constexpr uint8_t MAGIC_V3 = 0xDD;
    static constexpr uint8_t STORAGE_SIZE = 106;

    // Badge/FIDO2
    uint8_t badgeHash_[BADGE_HASH_SIZE] = {};
    uint8_t badgeRetries_ = MAX_RETRIES;

    // OpenPGP KDF data
    uint32_t iterations_ = DEFAULT_ITERATIONS;
    uint8_t pw1Salt_[SALT_SIZE] = {};
    uint8_t pw3Salt_[SALT_SIZE] = {};
    uint8_t pw1Hash_[KDF_HASH_SIZE] = {};
    uint8_t pw3Hash_[KDF_HASH_SIZE] = {};
    uint8_t pw1Retries_ = MAX_RETRIES;
    uint8_t pw3Retries_ = MAX_RETRIES;

    bool pinLoaded_ = false;
    bool badgePinIsSet_ = false;

    // Lockout timer (RAM only, resets on power cycle)
    uint32_t lockoutStartMs_ = 0;
    bool lockoutActive_ = false;

    bool loadFromStorage();
    bool saveToStorage();

    // Badge hash: LEFT(SHA256(PIN), 16)
    bool computeBadgeHash(const char* pin, uint8_t* hashOut);

    // OpenPGP KDF hash: SHA256 iterated with salt
    bool computeKdfHash(const char* pin, const uint8_t* salt, uint8_t* hashOut);

    bool compareHash(const uint8_t* h1, const uint8_t* h2, size_t len) const;
    void generateSalt(uint8_t* salt);
    void loadDefaults();

    /**
     * \brief Identifies the PIN slot operated on by `verifyPin()`.
     */
    enum class PinSlot : uint8_t {
        BADGE,
        PW1,
        PW3
    };

    /**
     * \brief Unified PIN verification routine handling counters and lockout.
     * \param slot Target PIN slot.
     * \param pin Candidate PIN string.
     * \return `true` if PIN matches the stored hash.
     */
    bool verifyPin(PinSlot slot, const char* pin);
};

} // namespace cdc::core
