---
title: "[MEDIUM] Mutable static variables in module state management"
severity: MEDIUM
domain: code-quality
lens: immutability
labels:
  - "audit:code-quality/immutability"
---

## Summary
Multiple module source files use mutable static variables for module state that could benefit from const enforcement or better encapsulation. The following files contain static variables that are reassigned during runtime but could be reviewed for const applicability:

**Files affected:**
- `components/mod_totp/src/TotpModule.cpp` (lines 25-50): `s_strIdBase`, `s_commandsRegistered`, `s_viewsInitialized`, `s_accountCount`, `s_capacity`
- `components/mod_gpg/src/GpgModule.cpp` (lines 30-50): `s_strIdBase`, `s_commandsRegistered`, `s_viewsInitialized`
- `components/mod_gpg/src/openpgp/openpgp.cpp` (lines 120-160): `s_openpgp_aid`, `app_selected`, `pw1_verified`, `pw3_verified`, `sig_count`, `s_session_pin`
- `components/cdc_log/src/cdc_log.cpp` (lines 17-38): `s_log_level`, `s_initialized`, `s_error_log_head`, `s_error_log_count`
- `components/usb_badge/usb_hid.cpp` (lines 28-65): `s_hid_initialized`, `s_defs`, `s_def_count`, `s_hid_map`, `s_hid_count`, `s_config_descriptor_len`
- `components/mod_nvsedit/src/NvsEditModule.cpp` (lines 39-55): `s_namespaces`, `s_namespaceCount`, `s_keys`, `s_keyTypes`, `s_keyCount`

## Impact
- **Maintainability**: Mutable static state makes it harder to track where and when values change, increasing cognitive load for developers
- **Thread safety**: Static mutable variables accessed from multiple contexts (tasks, ISRs) without synchronization can lead to race conditions
- **Testing**: Mutable global state makes unit testing more difficult as state persists between test runs
- **Bug risk**: Accidental mutation of variables that should be constant can introduce subtle bugs

## Evidence
Example from `components/mod_totp/src/TotpModule.cpp:25`:
```cpp
static uint16_t s_strIdBase = 0;  // Mutable, could be const after initialization
static bool s_commandsRegistered = false;  // Once set to true, never changes
static bool s_viewsInitialized = false;  // Once set to true, never changes
```

Example from `components/mod_gpg/src/openpgp/openpgp.cpp:120`:
```cpp
static uint8_t s_openpgp_aid[16] = { ... };  // Initialized once, never changes - should be const
static bool app_selected = false;  // Runtime state
static bool pw1_verified = false;  // Runtime state
```

Example from `components/cdc_log/src/cdc_log.cpp:17`:
```cpp
static log_level_t s_log_level = CDC_LOG_LEVEL_DEBUG;  // Can change at runtime
static bool s_initialized = false;  // Once true, never changes - could use const after init
```

## Recommended Fix
Review each static variable and apply const where appropriate:

1. **For variables that are set once and never change**: Use `constexpr` or `const`
   ```cpp
   // Before
   static uint16_t s_strIdBase = 0;
   
   // After (if only assigned once during init)
   static uint16_t s_strIdBase;  // Assign once in init function
   // Or use a getter with static const local
   static uint16_t getStrIdBase() {
       static uint16_t base = 0;
       return base;
   }
   ```

2. **For arrays that are initialized once**: Use `const`
   ```cpp
   // Before
   static uint8_t s_openpgp_aid[16] = { ... };
   
   // After
   static constexpr uint8_t s_openpgp_aid[16] = { ... };
   // Or if it needs to be modified during init:
   static uint8_t s_openpgp_aid[16];  // Initialize once in init function
   ```

3. **For flags that transition once (false -> true)**: Consider using a more explicit state machine or const-after-init pattern

4. **Group related state**: Consider wrapping related static variables in a struct with controlled access methods

This is a Medium severity issue because while these are module-local statics (not truly global), they still represent mutable state that could be better managed for improved code clarity and maintainability.

## References
- C++ Core Guidelines [C.24](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c24-use-typedefs-or-using-for-constants-not-define): Use `const` or `constexpr` for constants
- C++ Core Guidelines [C.47](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c47-define-and-initialize-objects-in-the-same-expression): Define and initialize objects in the same expression
- "Effective Modern C++" by Scott Meyers, Item 2: Prefer `constexpr` to `const`
