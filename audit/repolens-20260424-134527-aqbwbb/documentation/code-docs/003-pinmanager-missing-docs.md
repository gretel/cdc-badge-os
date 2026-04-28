---
title: "[MEDIUM] PinManager missing documentation"
severity: MEDIUM
domain: documentation/code-docs
lens: code-docs
labels:
  - "audit:documentation/code-docs"
---

## Summary

The `PinManager` class in `components/cdc_core/include/cdc_core/PinManager.h` has a good storage format comment but lacks documentation for most public methods. The class manages PIN storage and verification for Badge, FIDO2, and OpenPGP (PW1/PW3) PINs.

**File:** `components/cdc_core/include/cdc_core/PinManager.h:30-145`

## Impact

- Unclear which PINs use different algorithms (badge vs OpenPGP)
- Lockout timer behavior not documented
- Retry counts and blocking conditions unclear

## Evidence

```cpp
// Line 30-145: Methods lack documentation
class PinManager {
public:
    // PIN constraints (constants documented inline)
    static constexpr uint8_t BADGE_PIN_MIN = 4;
    static constexpr uint8_t BADGE_PIN_MAX = 8;
    static constexpr uint8_t PW1_MIN = 6;
    static constexpr uint8_t PW3_MIN = 8;
    static constexpr uint8_t PIN_MAX = 16;

    // Storage (no docs)
    static constexpr uint16_t RMEM_SLOT_PIN = 0;

    // Hash sizes (no docs)
    static constexpr uint8_t BADGE_HASH_SIZE = 16;
    static constexpr uint8_t KDF_HASH_SIZE = 32;
    static constexpr uint8_t SALT_SIZE = 8;

    // KDF parameters (no docs)
    static constexpr uint8_t KDF_ITERSALTED_S2K = 0x03;
    static constexpr uint8_t HASH_SHA256 = 0x08;
    static constexpr uint32_t DEFAULT_ITERATIONS = 100000;

    // Defaults (no docs)
    static constexpr const char* DEFAULT_BADGE_PIN = "123456";
    static constexpr const char* DEFAULT_PW1 = "123456";
    static constexpr const char* DEFAULT_PW3 = "12345678";

    static PinManager& instance();
    bool init();

    // === Badge/FIDO2 PIN === (no method docs)
    bool verifyBadgePin(const char* pin);
    bool changeBadgePin(const char* currentPin, const char* newPin);
    bool setBadgePin(const char* newPin);
    bool getBadgePinHash(uint8_t* hashOut) const;
    bool verifyBadgePinHash(const uint8_t* hashIn) const;
    uint8_t getBadgeRetries() const { ... }
    bool isBadgeBlocked() const;
    void resetBadgeRetries();

    // === Lockout Timer === (no docs)
    static constexpr uint32_t LOCKOUT_DURATION_MS = 60000;
    void startLockout();
    uint32_t getLockoutRemainingMs() const;
    bool isLockoutActive() const;

    // === OpenPGP PW1 === (no docs)
    bool verifyPW1(const char* pin);
    bool changePW1(const char* currentPin, const char* newPin);
    bool setPW1(const char* newPin);
    bool getPW1Hash(uint8_t* hashOut) const;
    bool getPW1Salt(uint8_t* saltOut) const;
    uint8_t getPW1Retries() const { ... }
    bool isPW1Blocked() const { ... }
    void resetPW1Retries();

    // === OpenPGP PW3 === (no docs)
    bool verifyPW3(const char* pin);
    bool changePW3(const char* currentPin, const char* newPin);
    bool setPW3(const char* newPin);
    bool getPW3Hash(uint8_t* hashOut) const;
    bool getPW3Salt(uint8_t* saltOut) const;
    uint8_t getPW3Retries() const { ... }
    bool isPW3Blocked() const { ... }
    void resetPW3Retries();

    // === KDF Parameters === (no docs)
    uint8_t getKdfAlgorithm() const { ... }
    uint8_t getHashAlgorithm() const { ... }
    uint32_t getIterationCount() const { ... }

    // === Status === (no docs)
    bool isPinSet() const { ... }
    bool isStorageAvailable() const;
```

## Recommended Fix

Add documentation to all public methods:

```cpp
/**
 * \brief PIN Manager - Manages all device PINs in TROPIC01 R-Memory Slot 0.
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
    static constexpr uint8_t BADGE_PIN_MIN = 4;  ///< Badge PIN min length
    static constexpr uint8_t BADGE_PIN_MAX = 8;  ///< Badge PIN max length
    static constexpr uint8_t PW1_MIN = 6;        ///< PW1 min length
    static constexpr uint8_t PW3_MIN = 8;        ///< PW3 min length
    static constexpr uint8_t PIN_MAX = 16;       ///< Max PIN length

    // Storage
    static constexpr uint16_t RMEM_SLOT_PIN = 0;  ///< TROPIC01 R-Memory slot

    // Hash sizes
    static constexpr uint8_t BADGE_HASH_SIZE = 16;  ///< Badge hash (LEFT SHA256)
    static constexpr uint8_t KDF_HASH_SIZE = 32;    ///< Full SHA256
    static constexpr uint8_t SALT_SIZE = 8;         ///< Salt size

    // KDF parameters (OpenPGP spec)
    static constexpr uint8_t KDF_ITERSALTED_S2K = 0x03;
    static constexpr uint8_t HASH_SHA256 = 0x08;
    static constexpr uint32_t DEFAULT_ITERATIONS = 100000;

    // Defaults
    static constexpr const char* DEFAULT_BADGE_PIN = "123456";
    static constexpr const char* DEFAULT_PW1 = "123456";
    static constexpr const char* DEFAULT_PW3 = "12345678";

    /**
     * \brief Get singleton instance.
     */
    static PinManager& instance();

    /**
     * \brief Initialize PIN manager from storage.
     * \return true if PIN data loaded successfully.
     */
    bool init();

    // === Badge/FIDO2 PIN ===
    /**
     * \brief Verify Badge PIN.
     * \param pin PIN to verify.
     * \return true if correct.
     */
    bool verifyBadgePin(const char* pin);

    /**
     * \brief Change Badge PIN.
     * \param currentPin Current PIN.
     * \param newPin New PIN.
     * \return true on success.
     */
    bool changeBadgePin(const char* currentPin, const char* newPin);

    /**
     * \brief Set Badge PIN (initial setup).
     * \param newPin New PIN.
     * \return true on success.
     */
    bool setBadgePin(const char* newPin);

    /**
     * \brief Get Badge PIN hash.
     * \param hashOut Output buffer (16 bytes).
     * \return true on success.
     */
    bool getBadgePinHash(uint8_t* hashOut) const;

    /**
     * \brief Verify Badge PIN hash (for FIDO2).
     * \param hashIn Hash to verify.
     * \return true if matches.
     */
    bool verifyBadgePinHash(const uint8_t* hashIn) const;

    /**
     * \brief Get remaining Badge PIN attempts.
     */
    uint8_t getBadgeRetries() const;

    /**
     * \brief Check if Badge PIN is blocked (retries=0 or lockout active).
     */
    bool isBadgeBlocked() const;

    /**
     * \brief Reset Badge PIN retries.
     */
    void resetBadgeRetries();

    // === Lockout Timer ===
    static constexpr uint32_t LOCKOUT_DURATION_MS = 60000;  ///< 60s lockout

    /**
     * \brief Start lockout timer.
     */
    void startLockout();

    /**
     * \brief Get remaining lockout time.
     * \return Milliseconds remaining (0 if not locked).
     */
    uint32_t getLockoutRemainingMs() const;

    /**
     * \brief Check if lockout is active.
     */
    bool isLockoutActive() const;

    // === OpenPGP PW1 (User PIN) ===
    /**
     * \brief Verify PW1 (User PIN).
     * \param pin PIN to verify.
     * \return true if correct.
     */
    bool verifyPW1(const char* pin);

    // ... (document all remaining methods similarly)
```

## References

- OpenPGP Card specification for KDF-DO format
- Related: `components/cdc_os_ui/src/AppUi.cpp` uses PinManager for unlock
