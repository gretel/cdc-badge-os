---
title: "[MEDIUM] TotpStore class missing Doxygen documentation"
severity: MEDIUM
domain: documentation/code-docs
lens: code-docs
labels:
  - "audit:documentation/code-docs"
---

## Summary

The `TotpStore` class in `components/mod_totp/include/mod_totp/TotpStore.h` lacks Doxygen-style documentation for its public methods and structs. While the class is self-contained and follows a similar pattern to `PasswordStore`, it has no documentation explaining its purpose, usage, or method contracts.

**File:** `components/mod_totp/include/mod_totp/TotpStore.h:7-73`

## Impact

- Developers must read implementation to understand how to use the class
- Unclear what parameters are expected (e.g., `secretBase32` format)
- Missing documentation on algorithm constants and flags
- Harder to maintain and extend the TOTP module

## Evidence

The following members lack documentation:

```cpp
// File: components/mod_totp/include/mod_totp/TotpStore.h
// Line 7-73: No class-level or method documentation

namespace cdc::mod_totp {

enum class TotpAlgorithm : uint8_t {  // No docs
    SHA1 = 0,
    SHA256 = 1,
    SHA512 = 2
};

struct TotpAccount {  // No docs
    char name[16 + 1];
    char issuer[32 + 1];
    uint8_t secret[32];
    uint8_t secretLen;
    uint8_t digits;
    uint32_t period;
    uint8_t algorithm;
    uint8_t flags;  // What do flags mean?
};

class TotpStore {
public:
    // Constants - no docs
    static constexpr uint8_t NAME_LEN = 16;
    static constexpr uint8_t ISSUER_LEN = 32;
    static constexpr uint8_t SECRET_LEN = 32;
    static constexpr uint8_t DEFAULT_DIGITS = 6;
    static constexpr uint32_t DEFAULT_PERIOD = 30;

    // Methods - no docs
    bool readAccount(uint16_t slot, TotpAccount* out);
    bool addAccount(const char* name, const char* issuer, const char* secretBase32,
                    uint8_t digits, uint32_t period, uint8_t algorithm);
    bool updateAccount(uint16_t slot, const char* name, const char* issuer, const char* secretBase32,
                       uint8_t digits, uint32_t period, uint8_t algorithm);
    bool deleteAccount(uint16_t slot);

    int8_t generateCode(uint16_t slot, char* codeOut);  // Returns -1 on error?

    bool isTimeValid() const;  // What does "time valid" mean?
    uint8_t timeRemaining(uint32_t period) const;  // Returns seconds?

    static TotpStore& instance();

    void setSlotRange(uint16_t start, uint16_t end, uint8_t moduleId);
    uint16_t capacity() const;
    bool toPhysicalSlot(uint16_t logicalIndex, uint16_t* slotOut) const;
    bool toLogicalSlot(uint16_t slot, uint16_t* logicalIndexOut) const;
    // ... rest undocumented
};
```

Compare to well-documented interface like `ISecureElement.h`:

```cpp
/**
 * Read from R-Memory slot
 * @param slot Slot number (0-511)
 * @param data Output buffer
 * @param maxLen Buffer size
 * @param actualLen Output: actual data length
 */
virtual SeResult rmemRead(uint16_t slot, uint8_t* data, uint16_t maxLen,
                          uint16_t* actualLen) = 0;
```

## Recommended Fix

Add Doxygen-style documentation to all public members:

```cpp
namespace cdc::mod_totp {

/**
 * \brief TOTP algorithm types.
 */
enum class TotpAlgorithm : uint8_t {
    SHA1 = 0,       // HMAC-SHA1 (standard)
    SHA256 = 1,     // HMAC-SHA256
    SHA512 = 2      // HMAC-SHA512
};

/**
 * \brief TOTP account data structure.
 */
struct TotpAccount {
    char name[16 + 1];      ///< Account name (e.g., "john@example.com")
    char issuer[32 + 1];    ///< Issuer name (e.g., "Google")
    uint8_t secret[32];     ///< Base32-decoded secret key
    uint8_t secretLen;      ///< Actual secret length
    uint8_t digits;         ///< Code digits (6 or 8)
    uint32_t period;        ///< Time period in seconds (usually 30)
    uint8_t algorithm;      ///< TotpAlgorithm enum value
    uint8_t flags;          ///< Account flags (bitmask)
};

/**
 * \brief TOTP account storage and code generation.
 *
 * Manages TOTP accounts stored in TROPIC01 R-Memory slots.
 * Provides add/edit/delete operations and code generation.
 */
class TotpStore {
public:
    static constexpr uint8_t NAME_LEN = 16;       ///< Max account name length
    static constexpr uint8_t ISSUER_LEN = 32;     ///< Max issuer name length
    static constexpr uint8_t SECRET_LEN = 32;     ///< Max secret key length
    static constexpr uint8_t DEFAULT_DIGITS = 6;  ///< Default code digits
    static constexpr uint32_t DEFAULT_PERIOD = 30; ///< Default time period (seconds)

    /**
     * \brief Read account from slot.
     * \param slot R-Memory slot number.
     * \param out Output structure.
     * \return true on success.
     */
    bool readAccount(uint16_t slot, TotpAccount* out);

    /**
     * \brief Add new TOTP account.
     * \param name Account name.
     * \param issuer Issuer name.
     * \param secretBase32 Secret key in Base32 format.
     * \param digits Code digits (6 or 8).
     * \param period Time period in seconds.
     * \param algorithm HMAC algorithm.
     * \return true on success.
     */
    bool addAccount(const char* name, const char* issuer, const char* secretBase32,
                    uint8_t digits, uint32_t period, uint8_t algorithm);

    /**
     * \brief Update existing account.
     * \param slot Slot number to update.
     * \param name Account name.
     * \param issuer Issuer name.
     * \param secretBase32 Secret key in Base32 format.
     * \param digits Code digits.
     * \param period Time period.
     * \param algorithm HMAC algorithm.
     * \return true on success.
     */
    bool updateAccount(uint16_t slot, const char* name, const char* issuer, const char* secretBase32,
                       uint8_t digits, uint32_t period, uint8_t algorithm);

    /**
     * \brief Delete account from slot.
     * \param slot Slot number.
     * \return true on success.
     */
    bool deleteAccount(uint16_t slot);

    /**
     * \brief Generate TOTP code for account.
     * \param slot Slot number.
     * \param codeOut Output buffer (at least 9 bytes for "NNNNNN\0").
     * \return Code as string, -1 on error.
     */
    int8_t generateCode(uint16_t slot, char* codeOut);

    /**
     * \brief Check if system time is valid for TOTP.
     * \return true if time has been set (year >= 2024).
     */
    bool isTimeValid() const;

    /**
     * \brief Get seconds remaining for current TOTP code.
     * \param period Time period in seconds.
     * \return Seconds until next code (0-30).
     */
    uint8_t timeRemaining(uint32_t period) const;

    /**
     * \brief Get singleton instance.
     */
    static TotpStore& instance();

    /**
     * \brief Set R-Memory slot range for this module.
     * \param start Start slot (inclusive).
     * \param end End slot (inclusive).
     * \param moduleId Module ID for storage header.
     */
    void setSlotRange(uint16_t start, uint16_t end, uint8_t moduleId);

    /**
     * \brief Get maximum account capacity.
     * \return Number of accounts that can be stored.
     */
    uint16_t capacity() const;

    /**
     * \brief Convert logical index to physical slot.
     * \param logicalIndex Zero-based account index.
     * \param slotOut Output: physical R-Memory slot.
     * \return true if slot is in range.
     */
    bool toPhysicalSlot(uint16_t logicalIndex, uint16_t* slotOut) const;

    /**
     * \brief Convert physical slot to logical index.
     * \param slot Physical R-Memory slot.
     * \param logicalIndexOut Output: zero-based account index.
     * \return true if slot belongs to this module.
     */
    bool toLogicalSlot(uint16_t slot, uint16_t* logicalIndexOut) const;

    // ... document remaining methods
};
```

## References

- Similar documented class: `PasswordStore.h` (same pattern, but also needs docs)
- Project documentation style: `components/cdc_hal/include/cdc_hal/ISecureElement.h`
- Doxygen backslash convention: `\brief`, `\param`, `\return` (per project guidelines)
- Related issue: `PasswordStore` class also lacks documentation (see issue #11)
