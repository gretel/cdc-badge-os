---
title: "[MEDIUM] ctap2.cpp uses global struct instead of class for CTAP2 state"
severity: MEDIUM
domain: modularity
lens: modularity
labels:
  - "audit:maintainability/modularity"
---

## Summary
The `ctap2.cpp` file (3520 lines) uses a global struct `g_ctap2` to store CTAP2 runtime state instead of encapsulating it in a `Ctap2` class. This makes the state accessible from anywhere in the compilation unit and prevents clean instantiation/multiple instances if needed. Combined with the file's large size, this creates a monolithic module that's hard to maintain and test.

**Evidence:**
- File: `components/mod_fido2/src/ctap2.cpp` (3520 lines)
- Global state: `static struct { bool initialized; bool operation_pending; ... } g_ctap2 = {};` (line ~60)
- No class wrapper for CTAP2 logic
- Functions like `ctap2_reset()`, `ctap2_send_keepalive()`, `ctap2_cancel()` operate on global state

## Impact
1. **Testability**: Cannot create isolated test instances with different state
2. **Memory efficiency**: State is always allocated even if CTAP2 is disabled
3. **Concurrency**: Global state is harder to reason about for thread safety
4. **Extensibility**: Adding features requires modifying global struct and scattered functions
5. **Readability**: 3520 lines in one file with global state is overwhelming

## Evidence
**Global state declaration (ctap2.cpp, lines 55-70):**
```cpp
/** \brief Global CTAP2 runtime state. */
static struct {
    bool initialized;
    bool operation_pending;
    bool cancelled;

    // For getNextAssertion
    uint8_t assertion_creds[FIDO2_MAX_CREDENTIALS];
    uint8_t assertion_count;
    uint8_t assertion_index;
    uint8_t assertion_rp_id_hash[32];
    uint8_t assertion_client_data_hash[32];
    bool assertion_up_done;
    bool assertion_include_user;
    bool assertion_appid_used;
} g_ctap2 = {};
```

**Functions operating on global state:**
```cpp
void ctap2_cancel(void) {
    g_ctap2.cancelled = true;  // Direct global access
    if (CTAP2_DEBUG_COMMANDS) LOG_D("CTAP2", "Operation cancelled");
}

uint8_t ctap2_make_credential(uint8_t* params, size_t params_len, uint8_t* response, size_t response_len) {
    g_ctap2.operation_pending = true;  // Direct global access
    // ... 200+ line function ...
}
```

## Recommended Fix
**Phase 1: Create Ctap2 class wrapper (45 minutes)**
1. Create `components/mod_fido2/include/mod_fido2/Ctap2.h` with class declaration (15 minutes)
   ```cpp
   namespace cdc::mod_fido2 {
   class Ctap2 {
   public:
       Ctap2();
       bool init();
       void cancel();
       uint8_t makeCredential(uint8_t* params, size_t len, uint8_t* response, size_t respLen);
       // ... other methods ...
   private:
       struct State { ... };  // Move g_ctap2 into class
       State state_;
   };
   }
   ```
2. Move global struct into `Ctap2::State` (5 minutes)
3. Convert functions to methods (15 minutes)
4. Update `ctap2.h` to declare `Ctap2` class (5 minutes)
5. Update callers to use `Ctap2::instance()` or pass instance (5 minutes)

**Phase 2: Extract sub-modules (future work, not in this issue)**
- Extract assertion logic to `AssertionHandler` class
- Extract credential management to `CredentialManager` class

**Total estimated time: ~45 minutes for Phase 1**

## References
- Similar pattern in `GroveLedModule`: Uses singleton class pattern with proper encapsulation
- CTAP2 specification: https://fidoalliance.org/specs/fido2/
- C++ Core Guidelines: [F.23](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Fa-init) - Use classes for state management
