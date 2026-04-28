---
title: "[HIGH] malloc without null check"
severity: HIGH
domain: linting
lens: code-quality/linting
labels:
  - "audit:code-quality/linting"
---

## Summary
Several code locations use `malloc` and `heap_caps_malloc` without checking for null return, which can lead to null pointer dereferences when memory is exhausted.

**Location:** Multiple files in components/CalEPD and components/Adafruit-GFX

## Impact
- **Null pointer dereference**: If allocation fails, the code will crash when accessing the buffer
- **Embedded systems critical**: ESP32 has limited RAM, allocation failures are more common than on desktop
- **Hard to debug**: Crash may happen much later, far from the actual allocation

## Evidence

### File: `components/CalEPD/include/wave12i48.h`
```cpp
uint8_t* _buffer = (uint8_t*)heap_caps_malloc(WAVE12I48_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
// No null check - used later without checking if allocation succeeded
```

### File: `components/Adafruit-GFX/Adafruit_GFX.cpp`
```cpp
if ((buffer = (uint8_t *)malloc(bytes))) {  // Check exists but not consistent
    // ...
}

// Other locations:
if ((buffer = (uint8_t *)malloc(bytes))) {  // Line ~400
if ((buffer = (uint16_t *)malloc(bytes))) {  // Line ~420
```

### File: `components/Adafruit-GFX/Print.cpp`
```cpp
temp = (char*) malloc(len+1);
// No null check before use
```

### File: `components/serial_cmd/src/SerialCmd.cpp`
```cpp
char* buf = static_cast<char*>(malloc(len));
uint8_t* buf = static_cast<uint8_t*>(malloc(len));
// No null check before use
```

## Recommended Fix

### For CalEPD header files:
Add null check in constructor:

```cpp
// In constructor:
_buffer = static_cast<uint8_t*>(heap_caps_malloc(
    WAVE12I48_BUFFER_SIZE, MALLOC_CAP_SPIRAM
));

if (!_buffer) {
    LOG_E("Wave12i48", "Failed to allocate %d bytes for buffer", WAVE12I48_BUFFER_SIZE);
    // Handle error - throw, set flag, or use smaller buffer
}
```

### For Adafruit-GFX (third-party, document if not fixing):
The existing code already has some checks, but they could be improved:

```cpp
// Current:
if ((buffer = (uint8_t *)malloc(bytes))) {
    // ...
}

// Better - explicit null handling:
buffer = static_cast<uint8_t*>(malloc(bytes));
if (!buffer) {
    LOG_E("AdafruitGFX", "Failed to allocate %d bytes", bytes);
    return; // or handle appropriately
}
// Use buffer
```

### For SerialCmd:
```cpp
// Before:
char* buf = static_cast<char*>(malloc(len));

// After:
char* buf = static_cast<char*>(malloc(len));
if (!buf) {
    LOG_E("SerialCmd", "Failed to allocate %d bytes", len);
    return; // or appropriate error handling
}
```

## References
- [CWE-416: Use After Free](https://cwe.mitre.org/data/definitions/416.html)
- [CWE-476: NULL Pointer Dereference](https://cwe.mitre.org/data/definitions/476.html)
- [ESP32 Memory Allocation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/mem_alloc.html)
- [C++ Core Guidelines: R.14 - Use heap allocation with smart pointers](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rr-unique-ptr)
