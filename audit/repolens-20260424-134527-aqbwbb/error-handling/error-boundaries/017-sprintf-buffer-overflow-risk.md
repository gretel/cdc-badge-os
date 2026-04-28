---
title: "[LOW] Potential buffer overflow with sprintf in third-party libraries"
severity: LOW
domain: error-handling
lens: error-boundaries
labels:
  - "error-handling"
  - "buffer-safety"
---

## Summary
Several third-party library files use `sprintf` without bounds checking, which can lead to buffer overflows when formatting strings. These are in legacy/external libraries but can still cause crashes.

**Evidence:**
- `components/mod_fido2/src/ctap2.cpp:639`:
```cpp
sprintf(hex + (i * 3), "%02X ", response[offset + i]);
```
Used in debug logging within loops - if `hex` buffer is too small, overflow occurs.

- `components/mod_fido2/src/ctap2.cpp:1162`: Same pattern in another debug function.

- `components/mod_fido2/src/ctap2.cpp:2471`:
```cpp
p += sprintf(p, "%02X ", pin_hash_enc[i + j]);
```
Pointer arithmetic with sprintf - if `p` reaches end of buffer, overflow.

- `components/Adafruit-GFX/WString.cpp:81,99,361,367,379`:
```cpp
sprintf(buf, "%d", value);
sprintf(buf, "%ld", value);
```
Used in String class constructors - buffers are sized but tight.

- `components/CalEPD/models/gdeh0213b73.cpp:229`:
```cpp
sprintf(buffer,"%x",command);
```
Small 3-byte buffer - works for hex but not robust.

## Impact
- **Buffer overflow**: `sprintf` doesn't check destination size
- **Memory corruption**: Can overwrite adjacent variables
- **Crashes**: Stack corruption leads to hard-to-debug crashes
- **Security**: Potential exploit vector if attacker controls input

## Recommended Fix
Replace `sprintf` with `snprintf` in all third-party code:

1. **FIDO2 module** (`components/mod_fido2/src/ctap2.cpp`):
```cpp
// Line 639
sprintf(hex + (i * 3), "%02X ", response[offset + i]);
// Change to:
snprintf(hex + (i * 3), 4, "%02X ", response[offset + i]);

// Line 2471
p += sprintf(p, "%02X ", pin_hash_enc[i + j]);
// Change to:
p += snprintf(p, 4, "%02X ", pin_hash_enc[i + j]);
```

2. **Adafruit GFX** (`components/Adafruit-GFX/WString.cpp`):
```cpp
// Line 81
sprintf(buf, "%d", value);
// Change to:
snprintf(buf, sizeof(buf), "%d", value);

// Line 99
sprintf(buf, "%ld", value);
// Change to:
snprintf(buf, sizeof(buf), "%ld", value);
```

3. **CalEPD** (`components/CalEPD/models/gdeh0213b73.cpp`):
```cpp
// Line 229
sprintf(buffer,"%x",command);
// Change to:
snprintf(buffer, sizeof(buffer), "%x", command);
```

## References
- [C String Safety](https://en.cppreference.com/w/cpp/string/byte/snprintf)
- [Buffer Overflow Prevention](https://owasp.org/www-community/attacks/Buffer_Overflow)
