---
title: "[LOW] Custom Log Macro Redundancy: CCID_LOG macros in mod_gpg"
severity: LOW
domain: code-style
lens: consistency
labels:
  - "audit:code-quality/consistency"
  - "logging"
---

## Summary

The `mod_gpg/src/openpgp/ccid.cpp` file defines custom logging macros that are simple wrappers around the standard `cdc_log` macros:

```cpp
#define CCID_LOG(tag, fmt, ...) LOG_I(tag, fmt, ##__VA_ARGS__)
#define CCID_LOG_E(tag, fmt, ...) LOG_E(tag, fmt, ##__VA_ARGS__)
#define CCID_LOG_W(tag, fmt, ...) LOG_W(tag, fmt, ##__VA_ARGS__)
```

These macros add no additional functionality beyond renaming, creating unnecessary indirection.

## Impact

1. **Cognitive overhead**: Developers need to understand CCID_LOG vs LOG_I
2. **Maintenance**: Duplication of logging macro definitions
3. **Search difficulty**: `grep "LOG_I"` misses `CCID_LOG` calls
4. **Inconsistency**: Only file in codebase with this pattern

## Evidence

**File: components/mod_gpg/src/openpgp/ccid.cpp**
```cpp
#define CCID_LOG(tag, fmt, ...) LOG_I(tag, fmt, ##__VA_ARGS__)
#define CCID_LOG_E(tag, fmt, ...) LOG_E(tag, fmt, ##__VA_ARGS__)
#define CCID_LOG_W(tag, fmt, ...) LOG_W(tag, fmt, ##__VA_ARGS__)

// Usage:
CCID_LOG(TAG, "========================================");
CCID_LOG(TAG, "ccid_process_message: msg_len=%zu resp_max=%zu", msg_len, resp_max);
CCID_LOG_E(TAG, "Failed to initialize OpenPGP");
```

## Recommended Fix

**Replace CCID_LOG macros with direct LOG_* calls:**

1. Remove macro definitions:
   ```cpp
   // Remove these lines:
   #define CCID_LOG(tag, fmt, ...) LOG_I(tag, fmt, ##__VA_ARGS__)
   #define CCID_LOG_E(tag, fmt, ...) LOG_E(tag, fmt, ##__VA_ARGS__)
   #define CCID_LOG_W(tag, fmt, ...) LOG_W(tag, fmt, ##__VA_ARGS__)
   ```

2. Replace usage (sed command):
   ```bash
   sed -i 's/CCID_LOG_E(/LOG_E(/g' components/mod_gpg/src/openpgp/ccid.cpp
   sed -i 's/CCID_LOG_W(/LOG_W(/g' components/mod_gpg/src/openpgp/ccid.cpp
   sed -i 's/CCID_LOG(/LOG_I(/g' components/mod_gpg/src/openpgp/ccid.cpp
   ```

3. Verify build and logging output

## References

- `components/mod_gpg/src/openpgp/ccid.cpp`
- `components/cdc_log/include/cdc_log.h`
