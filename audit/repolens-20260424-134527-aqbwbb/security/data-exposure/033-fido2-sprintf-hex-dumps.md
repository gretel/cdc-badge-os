---
title: "[LOW] FIDO2 uses sprintf for hex formatting without bounds checking"
severity: LOW
domain: fido2
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The FIDO2 CTAP2 implementation uses `sprintf` for hex formatting in three locations. While the buffers appear to be sized appropriately, using `sprintf` without bounds checking is a potential source of buffer overflow vulnerabilities and exposes internal data structures.

**Locations:**
- `components/mod_fido2/src/ctap2.cpp:639` - getInfo response hex dump
- `components/mod_fido2/src/ctap2.cpp:1162` - makeCredential response hex dump  
- `components/mod_fido2/src/ctap2.cpp:2471` - pinHashEnc hex dump

## Impact
- **Buffer overflow risk**: `sprintf` doesn't check destination buffer size
- **Data exposure**: Hex formatting exposes raw bytes that may include sensitive data
- **Stack corruption**: If buffer sizes are miscalculated, stack corruption could occur
- **Information leakage**: Each hex dump reveals internal protocol structure

## Evidence
File: `components/mod_fido2/src/ctap2.cpp:639`
```cpp
char hex[50] = {0};
for (size_t i = 0; i < *response_len && i < 15; i++)
    sprintf(hex + (i * 3), "%02X ", response[offset + i]);
```

File: `components/mod_fido2/src/ctap2.cpp:1162`
```cpp
char hex[50] = {0};
for (size_t i = 0; i < *response_len && i < 15; i++)
    sprintf(hex + (i * 3), "%02X ", response[offset + i]);
```

File: `components/mod_fido2/src/ctap2.cpp:2471`
```cpp
char hex[64];
char *p = hex;
for (size_t j = 0; j < row_len; j++)
    p += sprintf(p, "%02X ", pin_hash_enc[i + j]);
```

## Recommended Fix
1. Replace `sprintf` with `snprintf` for bounds checking
2. Track the position and remaining buffer size
3. Consider using a helper function for hex formatting

Example fix:
```cpp
// Replace sprintf with snprintf
char hex[50] = {0};
size_t pos = 0;
for (size_t i = 0; i < *response_len && i < 15 && pos < sizeof(hex) - 3; i++) {
    pos += snprintf(hex + pos, sizeof(hex) - pos, "%02X ", response[offset + i]);
}
```

## References
- sprintf vs snprintf: https://www.cplusplus.com/reference/cstdio/sprintf/
- Buffer overflow prevention: https://cwe.mitre.org/data/definitions/120.html
- Related to issue #21 (CalEPD printf usage)
