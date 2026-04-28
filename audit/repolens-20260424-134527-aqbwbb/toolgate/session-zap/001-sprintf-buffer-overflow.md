---
title: "[MEDIUM] Use of sprintf instead of snprintf in FIDO2 CTAP2 module"
severity: MEDIUM
domain: security
lens: toolgate/session-zap
labels:
  - "audit:toolgate/session-zap"
---

## Summary

The FIDO2 CTAP2 module uses `sprintf()` instead of `snprintf()` in three locations, which can lead to buffer overflows if the output exceeds the buffer size.

**Locations:**
1. `components/mod_fido2/src/ctap2.cpp:639` - getInfo response logging
2. `components/mod_fido2/src/ctap2.cpp:1162` - getAssertion response logging  
3. `components/mod_fido2/src/ctap2.cpp:2471` - PIN hash encryption debug logging

## Impact

Buffer overflows can cause:
- Stack corruption leading to crashes
- Potential code execution if return addresses are overwritten
- Memory corruption in an embedded context where RAM is limited

The buffers being written to are:
- `hex[50]` at line 636 (line 639: `sprintf(hex + (i * 3), "%02X ", ...)`)
- Similar patterns at lines 1162 and 2471

While the current code uses fixed-size arrays and the loop bounds appear controlled, `sprintf` provides no safety margin. If the format string or data changes, or if the loop count is modified, overflow becomes possible.

## Evidence

**File: `components/mod_fido2/src/ctap2.cpp`**

Line 636-641:
```cpp
char hex[50] = {0};
int dump_len = ((*response_len - offset) < 16) ? (*response_len - offset) : 16;
for (int i = 0; i < dump_len; i++) {
    sprintf(hex + (i * 3), "%02X ", response[offset + i]);
}
```

Line 2468-2472:
```cpp
char hex[64];
char *p = hex;
for (size_t j = 0; j < row_len; j++) {
    p += sprintf(p, "%02X ", pin_hash_enc[i + j]);
}
```

Each `sprintf` writes 3 characters (2 hex digits + space). With `hex[50]`, the maximum safe `dump_len` is 16 (16 × 3 = 48 bytes). While this is the current limit, using `sprintf` provides no protection if constraints change.

## Recommended Fix

Replace `sprintf` with `snprintf` in all three locations:

```cpp
// Line 639
sprintf(hex + (i * 3), "%02X ", response[offset + i]);
// Replace with:
snprintf(hex + (i * 3), 4, "%02X ", response[offset + i]);

// Line 1162 (same pattern as 639)
sprintf(hex + (i * 3), "%02X ", response[offset + i]);
// Replace with:
snprintf(hex + (i * 3), 4, "%02X ", response[offset + i]);

// Line 2471
p += sprintf(p, "%02X ", pin_hash_enc[i + j]);
// Replace with:
p += snprintf(p, hex + sizeof(hex) - p, "%02X ", pin_hash_enc[i + j]);
```

## References

- CWE-120: Buffer Copy without Checking Size of Input
- [C Standard Library - sprintf vs snprintf](https://www.cplusplus.com/reference/cstdio/sprintf/)
- [OWASP Buffer Overflow](https://cheatsheetseries.owasp.org/cheatsheets/BufferOverflow_Cheat_Sheet.html)
