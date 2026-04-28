---
title: "[MEDIUM] Stack overflow risk with large local arrays in CTAP2 assertion processing"
severity: MEDIUM
domain: fido2
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `components/mod_fido2/src/ctap2.cpp`, several functions declare local arrays of size `FIDO2_MAX_CREDENTIALS` (typically 32) on the stack. When multiple such arrays are used in the same function scope, they can accumulate to significant stack usage. The ESP32-S3 has limited IRAM (320KB) and each task has a default stack size (typically 4-8KB). Functions like `ctap2_get_assertion` can have multiple local arrays that together could exceed safe stack limits.

## Impact
- **Stack Overflow**: Functions like `ctap2_get_assertion` use multiple arrays:
  - `allow_list_slots[32]` (32 bytes)
  - `slots[32]` (32 bytes)
  - Plus other local variables
  - Total: ~100-200 bytes per function call, but nested calls can accumulate
- **Edge Case**: If the function is called recursively (e.g., through callbacks) or with large `FIDO2_MAX_CREDENTIALS`, stack overflow could occur.
- **Memory Pressure**: Large stack allocations reduce available stack for other operations.

## Evidence
File: `components/mod_fido2/src/ctap2.cpp`

Line 1263:
```cpp
struct GetAssertionParams {
    // ...
    uint8_t allow_list_slots[FIDO2_MAX_CREDENTIALS];  // 32 bytes
    // ...
};
```

Line 1283:
```cpp
static uint8_t ctap2_get_assertion(const uint8_t *params, uint16_t params_len,
                                    uint8_t *response, uint16_t *response_len) {
    GetAssertionParams p;  // ~500 bytes struct
    AssertionCredentials creds;  // ~40 bytes struct
    // ...
```

Line 1594:
```cpp
static void ga_find_credentials(GetAssertionParams *p, AssertionCredentials *creds) {
    uint8_t temp_slots[FIDO2_MAX_CREDENTIALS] = {0};  // 32 bytes
    // ...
```

Stack usage calculation for `ctap2_get_assertion`:
- `GetAssertionParams p`: ~500 bytes (rp_id, rp_id_hash, client_data_hash, allow_list_slots, etc.)
- `AssertionCredentials creds`: ~40 bytes (slots array, count, include_user, etc.)
- `temp_slots` in `ga_find_credentials`: 32 bytes
- Other locals: ~100 bytes
- **Total**: ~700 bytes per call

If this function is called multiple times in succession without returning (e.g., in a loop), or if it calls other functions with large stack allocations, the cumulative stack pressure could exceed limits.

## Recommended Fix
Move large arrays to heap or static/global storage with PSRAM attributes:

```cpp
// In file scope, use PSRAM for large arrays
static EXT_RAM_BSS_ATTR uint8_t g_assertion_allow_list[FIDO2_MAX_CREDENTIALS];
static EXT_RAM_BSS_ATTR uint8_t g_assertion_slots[FIDO2_MAX_CREDENTIALS];
static EXT_RAM_BSS_ATTR uint8_t g_assertion_temp_slots[FIDO2_MAX_CREDENTIALS];

static uint8_t ctap2_get_assertion(const uint8_t *params, uint16_t params_len,
                                    uint8_t *response, uint16_t *response_len) {
    GetAssertionParams p;
    AssertionCredentials creds;
    
    // Use global arrays instead of stack
    uint8_t *allow_list_slots = g_assertion_allow_list;
    uint8_t *slots = g_assertion_slots;
    uint8_t *temp_slots = g_assertion_temp_slots;
    
    // ... rest of function uses these pointers
}
```

Alternatively, use dynamic allocation with proper error handling:

```cpp
static uint8_t ctap2_get_assertion(const uint8_t *params, uint16_t params_len,
                                    uint8_t *response, uint16_t *response_len) {
    GetAssertionParams p;
    AssertionCredentials creds;
    
    uint8_t *temp_slots = (uint8_t*)malloc(FIDO2_MAX_CREDENTIALS);
    if (!temp_slots) {
        response[0] = CTAP2_ERRother;
        *response_len = 1;
        return CTAP2_ERR_OTHER;
    }
    
    // ... use temp_slots ...
    
    free(temp_slots);
    return CTAP2_OK;
}
```

## References
- [ESP32-S3 Memory](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/memory-types.html) - 320KB IRAM, PSRAM available
- [FreeRTOS Stack Size](https://www.freertos.org/FAQHelp.html) - Default task stack 4-8KB
- [Stack overflow detection](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/heap.html#stack-allocation) - Use `xTaskGetStackHighWaterMark` to monitor
