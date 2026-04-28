---
title: "[MEDIUM] R-Memory write with header performs erase+write without atomicity"
severity: MEDIUM
domain: transaction-concurrency
lens: transaction-concurrency
labels:
  - audit:concurrency/transaction-concurrency
---

## Summary

In `components/cdc_hal/src/Tropic01Element.cpp`, the `rmemWriteWithHeader()` method (lines 685-719) performs two separate operations: first `rmemErase()`, then `rmemWrite()`. Between these operations, if another task writes to the same slot, the data can become corrupted. Additionally, if the second write fails, the slot is left empty.

**Location:** `components/cdc_hal/src/Tropic01Element.cpp:685-719`

```cpp
SeResult Tropic01Element::rmemWriteWithHeader(uint16_t slot, uint8_t moduleId,
                                              const char* name, uint8_t flags,
                                              const uint8_t* payload, uint16_t payloadLen) {
    // ... validation ...
    
    // R-Memory requires erase before write
    SeResult eraseRes = rmemErase(slot);  // Line 694
    if (eraseRes != SeResult::OK && eraseRes != SeResult::SLOT_EMPTY) {
        return eraseRes;
    }

    // ... prepare header and buffer ...
    
    return rmemWrite(slot, buffer, static_cast<uint16_t>(sizeof(header) + payloadLen));  // Line 719
}
```

## Impact

**Partial Write / Data Corruption:**

1. Task A calls `rmemWriteWithHeader(slot, ...)`
2. Task A erases slot
3. Task B calls `rmemWriteWithHeader(slot, ...)`
4. Task B erases slot (redundant but OK)
5. Task B writes data successfully
6. Task A writes data (overwrites B's data)
7. Result: Both operations complete, but order is non-deterministic

**Slot Left Empty:**

1. Task A erases slot
2. Task A's `rmemWrite()` fails (timeout, alarm mode, etc.)
3. Slot is now empty, requiring recovery logic

**Impact on Modules:**

- **PasswordStore** (`components/mod_password/src/PasswordStore.cpp:230-242`): Adding passwords
- **TotpStore** (`components/mod_totp/src/TotpStore.cpp:273-288`): Adding TOTP accounts
- **GpgStorage** (`components/mod_gpg/src/GpgStorage.cpp:304-312`): Storing encrypted DEC key

## Evidence

**rmemWriteWithHeader() - Non-atomic erase+write:**
```cpp
// Line 685-719: components/cdc_hal/src/Tropic01Element.cpp
SeResult Tropic01Element::rmemWriteWithHeader(uint16_t slot, uint8_t moduleId,
                                              const char* name, uint8_t flags,
                                              const uint8_t* payload, uint16_t payloadLen) {
    if (slot >= RMEM_SLOT_COUNT) {
        return SeResult::INVALID_PARAM;
    }
    if (payloadLen > (RMEM_SLOT_SIZE - sizeof(RMemHeader))) {
        return SeResult::INVALID_PARAM;
    }

    // R-Memory requires erase before write
    SeResult eraseRes = rmemErase(slot);  // Line 694 - First operation
    if (eraseRes != SeResult::OK && eraseRes != SeResult::SLOT_EMPTY) {
        return eraseRes;
    }

    RMemHeader header = {};
    header.magic = RMEM_HEADER_MAGIC;
    header.moduleId = moduleId;
    header.flags = flags;
    header.payloadLen = payloadLen;
    if (name) {
        strncpy(header.name, sizeof(header.name) - 1, name);
        header.name[sizeof(header.name) - 1] = '\0';
    }
    header.checksum = computeHeaderChecksum(header);

    uint8_t buffer[RMEM_SLOT_SIZE] = {};
    memcpy(buffer, &header, sizeof(header));
    if (payloadLen > 0 && payload) {
        memcpy(buffer + sizeof(header), payload, payloadLen);
    }

    return rmemWrite(slot, buffer, static_cast<uint16_t>(sizeof(header) + payloadLen));  // Line 719 - Second operation
}
```

**rmemErase() - Releases mutex between operations:**
```cpp
// Line 608-624: components/cdc_hal/src/Tropic01Element.cpp
SeResult Tropic01Element::rmemErase(uint16_t slot) {
    if (slot >= RMEM_SLOT_COUNT) {
        return SeResult::INVALID_PARAM;
    }

    lock();  // Line 616 - Acquires mutex

    if (!ensureSession("rmemErase")) {
        unlock();  // Line 619 - Releases mutex
        return SeResult::SESSION_REQUIRED;
    }

    lt_ret_t ret = lt_r_mem_data_erase(&handle_, slot);
    handleSessionError(ret);

    unlock();  // Line 623 - Releases mutex BEFORE write
    return mapResult(ret);
}
```

**rmemWrite() - Separate mutex acquisition:**
```cpp
// Line 578-600: components/cdc_hal/src/Tropic01Element.cpp
SeResult Tropic01Element::rmemWrite(uint16_t slot, const uint8_t* data, uint16_t len) {
    if (slot >= RMEM_SLOT_COUNT || !data || len == 0 || len > RMEM_SLOT_SIZE) {
        return SeResult::INVALID_PARAM;
    }

    lock();  // Line 587 - Acquires mutex (separate from erase)

    if (!ensureSession("rmemWrite")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    lt_ret_t ret = lt_r_mem_data_write(&handle_, slot, data, len);
    handleSessionError(ret);

    unlock();  // Line 598 - Releases mutex
    return mapResult(ret);
}
```

## Recommended Fix

**Add an internal method that performs erase+write atomically:**

```cpp
// In Tropic01Element.h (add private method):
SeResult rmemWriteWithHeaderAtomic(uint16_t slot, uint8_t moduleId,
                                   const char* name, uint8_t flags,
                                   const uint8_t* payload, uint16_t payloadLen);

// In Tropic01Element.cpp:
SeResult Tropic01Element::rmemWriteWithHeader(uint16_t slot, uint8_t moduleId,
                                              const char* name, uint8_t flags,
                                              const uint8_t* payload, uint16_t payloadLen) {
    // Use atomic version
    return rmemWriteWithHeaderAtomic(slot, moduleId, name, flags, payload, payloadLen);
}

SeResult Tropic01Element::rmemWriteWithHeaderAtomic(uint16_t slot, uint8_t moduleId,
                                                    const char* name, uint8_t flags,
                                                    const uint8_t* payload, uint16_t payloadLen) {
    if (slot >= RMEM_SLOT_COUNT) {
        return SeResult::INVALID_PARAM;
    }
    if (payloadLen > (RMEM_SLOT_SIZE - sizeof(RMemHeader))) {
        return SeResult::INVALID_PARAM;
    }

    lock();  // Single lock for entire operation

    if (!ensureSession("rmemWriteWithHeaderAtomic")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    // Erase
    lt_ret_t ret = lt_r_mem_data_erase(&handle_, slot);
    if (ret != LT_OK) {
        handleSessionError(ret);
        unlock();
        return mapResult(ret);
    }

    // Prepare buffer
    RMemHeader header = {};
    header.magic = RMEM_HEADER_MAGIC;
    header.moduleId = moduleId;
    header.flags = flags;
    header.payloadLen = payloadLen;
    if (name) {
        strncpy(header.name, name, sizeof(header.name) - 1);
        header.name[sizeof(header.name) - 1] = '\0';
    }
    header.checksum = computeHeaderChecksum(header);

    uint8_t buffer[RMEM_SLOT_SIZE] = {};
    memcpy(buffer, &header, sizeof(header));
    if (payloadLen > 0 && payload) {
        memcpy(buffer + sizeof(header), payload, payloadLen);
    }

    // Write
    ret = lt_r_mem_data_write(&handle_, slot, buffer, static_cast<uint16_t>(sizeof(header) + payloadLen));
    handleSessionError(ret);

    unlock();  // Single unlock
    return mapResult(ret);
}
```

**Alternative: Add a higher-level transaction wrapper**

```cpp
// In ISecureElement.h:
struct RMemTransaction {
    uint16_t slot;
    uint8_t moduleId;
    const char* name;
    uint8_t flags;
    const uint8_t* payload;
    uint16_t payloadLen;
};

virtual SeResult rmemTransaction(const RMemTransaction& tx) = 0;
```

## References

1. **TROPIC01 Datasheet** - R-Memory erase/write timing and requirements
2. **libtropic API** - [lt_r_mem_data_erase](https://github.com/libtropic/libtropic), [lt_r_mem_data_write](https://github.com/libtropic/libtropic)
3. **ESP32 SPI Flash** - Similar erase+write patterns require atomic protection
