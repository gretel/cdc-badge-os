---
title: "[LOW] extern "C" Usage Inconsistency"
severity: LOW
domain: code-structure
lens: code-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
The codebase shows inconsistent use of `extern "C"` for C/C++ linkage:
1. Some headers use `extern "C"` blocks
2. Some .cpp files use `extern "C"` for function definitions
3. Inconsistent placement (headers vs. source files)

### Evidence

**extern "C" in headers:**
```cpp
// components/mod_totp/include/mod_totp/TotpModule.h
extern "C" void mod_totp_register();

// components/cdc_log/include/cdc_log.h
extern "C" {
    void log_init(void);
    void log_set_level(log_level_t level);
    ...
}
```

**extern "C" in source files:**
```cpp
// components/mod_totp/src/TotpModule.cpp
extern "C" void mod_totp_register() {
    // Implementation
}

// components/usb_badge/usb_hid.cpp
extern "C" {
    // Block of C-linkage functions
}

extern "C" bool usb_hid_init(void) { ... }
```

**extern "C" blocks:**
```cpp
// components/include/tusb_config.h
extern "C" {
    // TUSB configuration
}

// components/usb_badge/include/usb_badge/usb_cdc.h
extern "C" {
    bool usb_cdc_init(void);
    ...
}
```

**No extern "C" (inconsistent):**
- Many component headers don't use extern "C" at all

## Impact
- **Linkage issues**: Potential C++ name mangling problems
- **Confusion**: Developers unsure when to use extern "C"
- **Interop problems**: May break C/C++ interop in some cases

## Recommended Fix
1. **Standardize on extern "C" for module registration functions**:
   - Keep `extern "C" void mod_*_register();` in headers
2. **Use extern "C" blocks for C APIs**:
   - Wrap all C-compatible functions in `extern "C" { }`
3. **Document convention**: When to use extern "C" vs. regular C++

### Files to review:
- All module registration headers in `include/` directories
- USB badge component headers

## References
- C++ Core Guidelines: C/C++ interop
- ESP-IDF: C compatibility for components
