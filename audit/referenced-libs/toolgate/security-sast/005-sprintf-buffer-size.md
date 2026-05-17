---
title: "[LOW] Potential buffer overflow in sprintf with unknown buffer size"
severity: LOW
domain: security
lens: sast
labels:
  - "buffer-overflow"
  - "sprintf"
  - "calEpd"
---

## Summary
In CalEPD component, `sprintf` is used with a buffer of unknown size. The code converts a command value to hexadecimal format, but the buffer size is not verified to be large enough.

## Impact
- **Buffer Overflow**: If the buffer is smaller than expected, adjacent memory can be corrupted
- **Stability**: Can cause crashes or display artifacts on e-paper displays
- **Hard to Debug**: Memory corruption issues are often intermittent and hard to trace

## Evidence

File: `components/CalEPD/models/gdeh0213b73.cpp:229`
```cpp
sprintf(buffer,"%x",command);
```

File: `components/CalEPD/models/fix/gdeh0213b73.cpp:185`
```cpp
sprintf(buffer,"%x",command);
```

Context (from gdeh0213b73.cpp):
```cpp
void Epd::printerf(const char* format, ...) {
    char buffer[20];  // Buffer size assumed based on similar code
    va_list args;
    va_start(args, format);
    sprintf(buffer, "%x", command);  // Line 229
    ...
}
```

## Recommended Fix

Replace `sprintf` with `snprintf`, specifying the buffer size:

```cpp
// Before:
sprintf(buffer, "%x", command);

// After:
char buffer[20];  // Ensure buffer is declared
snprintf(buffer, sizeof(buffer), "%x", command);
```

**Additional verification:**
1. Check that buffer size of 20 is sufficient for all expected command values
2. For hex output of a 16-bit command: max 4 chars + null = 5 bytes needed
3. For 32-bit command: max 8 chars + null = 9 bytes needed

## References
- CWE-120: Buffer copy without checking size of input
- CWE-131: Incorrect calculation of multi-byte string length
