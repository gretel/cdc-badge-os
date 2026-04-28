---
title: "[LOW] openpgp.h uses C-style API instead of C++ namespace"
severity: LOW
domain: modularity
lens: modularity
labels:
  - "audit:maintainability/modularity"
---

## Summary
The `openpgp.h` header uses `extern "C"` to expose a C-style API with functions like `openpgp_init()`, `openpgp_process_apdu()`, etc. While this enables C compatibility, it prevents the use of C++ features like namespaces, function overloading, and type-safe templates. The module is implemented in C++ (`.cpp` files) so a C++-style API would be more appropriate.

**Evidence:**
- File: `components/mod_gpg/include/mod_gpg/openpgp/openpgp.h`
- Uses `extern "C"` wrapper (lines 19-21, 107-109)
- Functions: `openpgp_init()`, `openpgp_process_apdu()`, `openpgp_is_selected()`, etc.
- All in global namespace

## Impact
1. **Lost C++ features**: Cannot use function overloading, default parameters, or templates
2. **Namespace pollution**: All functions in global namespace
3. **Type safety**: Cannot use C++ types like `std::string`, `std::optional`, etc.
4. **Documentation**: Function origin isn't clear from namespace

## Evidence
**openpgp.h** (lines 19-21, 100-107):
```cpp
#ifdef __cplusplus
extern "C" {
#endif

// ... constants and macros ...

// Initialize OpenPGP application
bool openpgp_init(void);

// Process incoming APDU command
// Returns response length (including SW1-SW2)
int openpgp_process_apdu(const uint8_t *cmd, size_t cmd_len,
                         uint8_t *resp, size_t resp_max);

// Check if OpenPGP application is selected
bool openpgp_is_selected(void);

// Get current signature count
uint32_t openpgp_get_sig_count(void);

#ifdef __cplusplus
}
#endif
```

**Compare with modern C++ pattern (mod_gpg/GpgModule.h):**
```cpp
namespace cdc::mod_gpg {
class GpgModule : public core::IModule {
    bool init() override;
    // ...
};
}
```

## Recommended Fix
**Option A: Keep C-style API (Recommended for now)**
- Rationale: OpenPGP application needs C-style interface for APDU processing
- Keep as-is until there's a clear need for C++ features
- Document that this is a "C-compatible interface"

**Option B: Add C++ wrapper (Future work)**
1. Keep `openpgp.h` as C-compatible interface
2. Create `OpenpgpClass.h` with C++ wrapper class
3. Implement wrapper methods that call C functions

**Total estimated time for Option A: 0 minutes (document current state)**
**Total estimated time for Option B: ~60 minutes**

## References
- APDU protocol is inherently C-style (byte arrays, length parameters)
- Similar pattern in `ccid.h` - also uses `extern "C"`
- C++ Core Guidelines [C.12](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-extern-c): Use `extern "C"` for C compatibility
