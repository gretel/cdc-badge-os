---
title: "[LOW] sprintf used instead of snprintf in debug logging"
severity: LOW
domain: code-quality
lens: session-nuclei
labels:
  - audit:toolgate/session-nuclei
---

## Summary

In `components/mod_fido2/src/ctap2.cpp`, `sprintf` is used in several debug logging blocks instead of `snprintf`. While these are only active in `DEBUG_MODE`, using `snprintf` is more consistent and safer.

**Locations**:
1. Line 639: `sprintf(hex + (i * 3), "%02X ", response[offset + i]);`
2. Line 1162: `sprintf(hex + (i * 3), "%02X ", response[offset + i]);`
3. Line 2471: `p += sprintf(p, "%02X ", pin_hash_enc[i + j]);`

## Impact

- **Buffer overflow risk**: `sprintf` does not check buffer bounds. Although the buffers are sized appropriately in the current code, using `sprintf` is considered less safe.
- **Consistency**: The codebase uses `snprintf` in other places (e.g., `Fido2Ui.cpp`, `u2f.cpp`), so `sprintf` is an outlier.
- **Maintainability**: If buffer sizes change in the future, `sprintf` calls are more likely to cause issues.

## Evidence

**File**: `components/mod_fido2/src/ctap2.cpp` (lines 636-643)

```cpp
#if DEBUG_MODE
    LOG_I("CTAP2", "getInfo response len=%u", *response_len);
    for (uint16_t offset = 0; offset < *response_len; offset += 16) {
        char hex[50] = {0};
        int dump_len = ((*response_len - offset) < 16) ? (*response_len - offset) : 16;
        for (int i = 0; i < dump_len; i++) {
            sprintf(hex + (i * 3), "%02X ", response[offset + i]);
        }
        LOG_D("CTAP2", "%03u: %s", offset, hex);
    }
#endif
```

**File**: `components/mod_fido2/src/ctap2.cpp` (lines 2468-2476)

```cpp
#if DEBUG_MODE
    // Log received pinHashEnc for debugging
    LOG_I("PIN", "Received pinHashEnc (%zu bytes):", pin_hash_enc_len);
    for (size_t i = 0; i < pin_hash_enc_len; i += 16) {
        size_t row_len = (pin_hash_enc_len - i < 16) ? (pin_hash_enc_len - i) : 16;
        char hex[64];
        char *p = hex;
        for (size_t j = 0; j < row_len; j++) {
            p += sprintf(p, "%02X ", pin_hash_enc[i + j]);
        }
        LOG_I("PIN", "  %s", hex);
    }
#endif
```

For comparison, `snprintf` is used correctly in `Fido2Ui.cpp`:

```cpp
snprintf(s_labels[i], sizeof(s_labels[i]), ...);
```

## Recommended Fix

Replace `sprintf` with `snprintf` for consistency and safety:

**For lines 639 and 1162:**
```cpp
sprintf(hex + (i * 3), "%02X ", response[offset + i]);
```
Change to:
```cpp
snprintf(hex + (i * 3), 4, "%02X ", response[offset + i]);  // 3 chars + null
```

**For line 2471:**
```cpp
p += sprintf(p, "%02X ", pin_hash_enc[i + j]);
```
Change to:
```cpp
p += snprintf(p, 4, "%02X ", pin_hash_enc[i + j]);  // 3 chars + null
```

## References

- C Standard Library: `sprintf` vs `snprintf`
- CWE-134: Use of inadequately formatted string
- ESP32 C Standard Library documentation
