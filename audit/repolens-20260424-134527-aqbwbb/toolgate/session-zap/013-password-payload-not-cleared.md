---
title: "[MEDIUM] Password entry payloads not cleared from stack"
severity: MEDIUM
domain: security
lens: toolgate/session-zap
labels:
  - "audit:toolgate/session-zap"
---

## Summary

The password module stores password entries in stack-allocated `PasswordPayload` structures but does not clear them after writing to secure element storage. The full password entry (including the password itself) persists in stack memory until overwritten.

**Locations:** `components/mod_password/src/PasswordStore.cpp`
- `addEntry()` - lines 215-245
- `updateEntry()` - lines 263-293

**Affected variables:**
- `PasswordPayload payload` - Contains title, username, password, URL, notes, TOTP slot

## Impact

**Security implications:**

1. **Stack persistence**: Full password entries remain on the stack after function return:
   - `addEntry()` - payload at line 215
   - `updateEntry()` - payload at line 263

2. **Complete entry exposure**: The payload contains:
   - Password (up to 64 characters)
   - Username (up to 64 characters)
   - URL (up to 64 characters)
   - Title (up to 64 characters)
   - Notes (up to 128 characters)

3. **Multiple copies**: A password entry exists in:
   - Caller's `PasswordEntry` structure (passed by reference)
   - Stack `PasswordPayload payload`
   - Secure element R-Memory (persistent)

4. **Extended attack window**: An attacker with memory access can extract:
   - The actual password (not a hash)
   - Associated metadata (username, URL, notes)
   - Account information for targeted attacks

**Context:** Password entries are typically long-lived credentials (unlike TOTP which changes every 30 seconds). They may be used for banking, email, social media - high-value targets.

## Evidence

**File: `components/mod_password/src/PasswordStore.cpp`**

`addEntry()` function (lines 207-248):
```cpp
bool PasswordStore::addEntry(const PasswordEntry& entry) {
    if (!hasSlotRange_) return false;
    uint16_t slot = 0;
    if (!findFreeSlot(&slot)) {
        LOG_W(TAG, "No free password slots");
        return false;
    }

    PasswordPayload payload = {};
    copyText(payload.title, sizeof(payload.title), entry.title);
    copyText(payload.username, sizeof(payload.username), entry.username);
    copyText(payload.password, sizeof(payload.password), entry.password);
    copyText(payload.url, sizeof(payload.url), entry.url);
    payload.totpSlot = entry.totpSlot;
    copyText(payload.notes, sizeof(payload.notes), entry.notes);

    char headerName[cdc::hal::ISecureElement::RMEM_NAME_LEN + 1] = {};
    // ...

    auto res = se->rmemWriteWithHeader(slot, moduleId_, headerName, 0,
                                        reinterpret_cast<const uint8_t*>(&payload),
                                        sizeof(payload));

    // ... error handling ...

    return true;  // No clearing of payload
}
```

`updateEntry()` function (lines 258-298):
```cpp
bool PasswordStore::updateEntry(uint16_t slot, const PasswordEntry& entry) {
    // ...

    PasswordPayload payload = {};
    copyText(payload.title, sizeof(payload.title), entry.title);
    copyText(payload.username, sizeof(payload.username), entry.username);
    copyText(payload.password, sizeof(payload.password), entry.password);
    copyText(payload.url, sizeof(payload.url), entry.url);
    payload.totpSlot = entry.totpSlot;
    copyText(payload.notes, sizeof(payload.notes), entry.notes);

    // ... write to secure element ...

    return true;  // No clearing of payload
}
```

**No clearing code found:** The `payload` structure is never cleared with `memset()` before the function returns.

**Payload structure** (from PasswordStore.h line 18):
```cpp
struct PasswordPayload {
    char title[64];
    char username[64];
    char password[64];
    char url[64];
    uint8_t totpSlot;
    char notes[128];
};
// Total: ~388 bytes on stack
```

## Recommended Fix

Clear the payload after writing to secure element storage:

**Option 1: Add memset before return**

```cpp
bool PasswordStore::addEntry(const PasswordEntry& entry) {
    // ... existing code ...

    PasswordPayload payload = {};
    copyText(payload.title, sizeof(payload.title), entry.title);
    copyText(payload.username, sizeof(payload.username), entry.username);
    copyText(payload.password, sizeof(payload.password), entry.password);
    copyText(payload.url, sizeof(payload.url), entry.url);
    payload.totpSlot = entry.totpSlot;
    copyText(payload.notes, sizeof(payload.notes), entry.notes);

    char headerName[cdc::hal::ISecureElement::RMEM_NAME_LEN + 1] = {};
    // ...

    auto res = se->rmemWriteWithHeader(slot, moduleId_, headerName, 0,
                                        reinterpret_cast<const uint8_t*>(&payload),
                                        sizeof(payload));

    // Clear payload before returning
    memset(&payload, 0, sizeof(payload));

    if (res != cdc::hal::SeResult::OK) {
        LOG_E(TAG, "Failed to write slot %u", slot);
        return false;
    }

    cdc::core::TropicStorage::instance().writeSlot(moduleId_, slot, headerName, 0);
    return true;
}
```

**Option 2: Use scope block**

```cpp
bool PasswordStore::addEntry(const PasswordEntry& entry) {
    // ... validation ...

    char headerName[cdc::hal::ISecureElement::RMEM_NAME_LEN + 1] = {};
    // ... prepare headerName ...

    {  // Start scope for payload
        PasswordPayload payload = {};
        copyText(payload.title, sizeof(payload.title), entry.title);
        copyText(payload.username, sizeof(payload.username), entry.username);
        copyText(payload.password, sizeof(payload.password), entry.password);
        copyText(payload.url, sizeof(payload.url), entry.url);
        payload.totpSlot = entry.totpSlot;
        copyText(payload.notes, sizeof(payload.notes), entry.notes);

        auto res = se->rmemWriteWithHeader(slot, moduleId_, headerName, 0,
                                            reinterpret_cast<const uint8_t*>(&payload),
                                            sizeof(payload));

        // Clear before leaving scope
        memset(&payload, 0, sizeof(payload));

        if (res != cdc::hal::SeResult::OK) {
            LOG_E(TAG, "Failed to write slot %u", slot);
            return false;
        }
    }  // payload goes out of scope

    cdc::core::TropicStorage::instance().writeSlot(moduleId_, slot, headerName, 0);
    return true;
}
```

**Option 3: Clear on both success and error paths**

```cpp
bool PasswordStore::addEntry(const PasswordEntry& entry) {
    PasswordPayload payload = {};
    // ... populate payload ...

    char headerName[cdc::hal::ISecureElement::RMEM_NAME_LEN + 1] = {};
    // ...

    auto res = se->rmemWriteWithHeader(slot, moduleId_, headerName, 0,
                                        reinterpret_cast<const uint8_t*>(&payload),
                                        sizeof(payload));

    // Always clear payload
    memset(&payload, 0, sizeof(payload));

    if (res != cdc::hal::SeResult::OK) {
        LOG_E(TAG, "Failed to write slot %u", slot);
        return false;
    }

    cdc::core::TropicStorage::instance().writeSlot(moduleId_, slot, headerName, 0);
    return true;
}
```

**Recommended:** Option 3 is most robust - clears the payload regardless of success or failure, ensuring no copy remains on the stack.

**Additional considerations:**

1. The `headerName` buffer may contain sensitive data (title) - consider clearing it too
2. Apply the same pattern to `updateEntry()`
3. Consider if `PasswordEntry` passed by caller should be cleared by caller after use

## References

- CWE-200: Exposure of Sensitive Information to an Unauthorized Actor
- CWE-312: Secure Data Removal
- [Password Management Best Practices](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html)
