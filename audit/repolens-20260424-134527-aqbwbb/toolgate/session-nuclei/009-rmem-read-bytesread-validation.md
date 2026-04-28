---
title: "[MEDIUM] rmemRead does not validate actualLen against maxLen parameter"
severity: MEDIUM
domain: cdc-badge-os
lens: library/cdc-badge-os
labels:
  - "audit:toolgate/session-nuclei"
---

## Summary
The `rmemRead` function in `components/cdc_hal/src/Tropic01Element.cpp` does not validate that the `actualLen` (bytesRead) returned from the secure element does not exceed the `maxLen` buffer size. While the function passes `maxLen` to the libtropic call, it trusts the returned `bytesRead` value without re-validation, potentially leading to buffer overread if the secure element returns more bytes than requested.

**Location**: `components/cdc_hal/src/Tropic01Element.cpp:547-576`

## Impact
- **Buffer Overread**: If the secure element returns `bytesRead` > `maxLen`, the caller's buffer may be overread
- **Caller Trust**: Callers trust `actualLen` without knowing if it was capped correctly
- **Stack Corruption**: In functions like `cmdTr01RmemRead` where a local buffer is used, this could lead to reading beyond intended bounds
- **Data Integrity**: The function doesn't verify that the returned data length is consistent with what was requested

## Evidence
```cpp
// components/cdc_hal/src/Tropic01Element.cpp:547-576
SeResult Tropic01Element::rmemRead(uint16_t slot, uint8_t* data, uint16_t maxLen,
                                    uint16_t* actualLen) {
    if (slot >= RMEM_SLOT_COUNT || !data || maxLen == 0) {
        return SeResult::INVALID_PARAM;
    }

    lock();

    if (!ensureSession("rmemRead")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    uint16_t bytesRead = 0;
    lt_ret_t ret = lt_r_mem_data_read(&handle_, slot, data, maxLen, &bytesRead);

    if (ret == LT_OK && actualLen) {
        *actualLen = bytesRead;  // Trusts bytesRead without validation
    }
    handleSessionError(ret);

    unlock();

    if (ret == LT_L3_R_MEM_DATA_READ_SLOT_EMPTY) {
        if (actualLen) *actualLen = 0;
        return SeResult::SLOT_EMPTY;
    }

    return mapResult(ret);
}
```

The issue is that:
1. Line 561: `lt_r_mem_data_read` is called with `maxLen` as the buffer size
2. Line 564: `bytesRead` is directly assigned to `*actualLen` without checking if `bytesRead <= maxLen`
3. If the secure element has a bug or is manipulated to return `bytesRead > maxLen`, the function doesn't catch it

This is used in `cmdTr01RmemRead`:
```cpp
// components/serial_cmd/src/SerialCmd.cpp:992-1014
static void cmdTr01RmemRead(const char* args) {
    // ...
    uint8_t data[256];
    uint16_t actualLen = 0;

    hal::SeResult seResult = se->rmemRead(slot, data, sizeof(data), &actualLen);
    // ...
    printHexDump(data, actualLen, actualLen);  // Uses actualLen which may be > 256
}
```

## Recommended Fix
Add validation to ensure `bytesRead` does not exceed `maxLen`:

```cpp
SeResult Tropic01Element::rmemRead(uint16_t slot, uint8_t* data, uint16_t maxLen,
                                    uint16_t* actualLen) {
    if (slot >= RMEM_SLOT_COUNT || !data || maxLen == 0) {
        return SeResult::INVALID_PARAM;
    }

    lock();

    if (!ensureSession("rmemRead")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    uint16_t bytesRead = 0;
    lt_ret_t ret = lt_r_mem_data_read(&handle_, slot, data, maxLen, &bytesRead);

    if (ret == LT_OK) {
        // Validate bytesRead against maxLen
        if (bytesRead > maxLen) {
            LOG_W(TAG, "Secure element returned bytesRead (%u) > maxLen (%u)", 
                  bytesRead, maxLen);
            bytesRead = maxLen;  // Cap to safe value
        }
        if (actualLen) {
            *actualLen = bytesRead;
        }
    }
    handleSessionError(ret);

    unlock();

    if (ret == LT_L3_R_MEM_DATA_READ_SLOT_EMPTY) {
        if (actualLen) *actualLen = 0;
        return SeResult::SLOT_EMPTY;
    }

    return mapResult(ret);
}
```

## References
- `components/cdc_hal/src/Tropic01Element.cpp:547-576` - Function implementation
- `components/serial_cmd/src/SerialCmd.cpp:992-1014` - Usage in serial command
- `components/cdc_hal/include/cdc_hal/ISecureElement.h:138-143` - Interface definition
- CWE-125: Out-of-bounds read
- CWE-787: Out-of-bounds write
