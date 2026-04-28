---
title: "[MEDIUM] FIDO2 storage state scattered across multiple C files with no unified state management"
severity: MEDIUM
domain: State Management Architecture
lens: state-architecture
labels:
  - "audit:architecture/state-architecture"
---

## Summary
The FIDO2 module's state is scattered across 5+ C source files (`fido2_storage.cpp`, `fido2.cpp`, `ctap2.cpp`, `ctaphid.cpp`, `Fido2Ui.cpp`) with each file maintaining its own `static struct` global state. There is no unified state management or clear ownership of the overall FIDO2 state machine.

### Evidence:
- `fido2_storage.cpp:52-75`: `g_storage` struct holds credential cache, counter, initialization state
- `fido2.cpp:28-32`: `g_fido2` struct holds task handle, PIN verified state
- `ctap2.cpp:24-37`: `g_ctap2` struct holds operation state, assertion iteration
- `ctap2.cpp:45-60`: `g_client_pin` struct holds ECDH key, PIN tokens, retry counters
- `Fido2Ui.cpp:35-45`: `s_listView`, `s_detailView`, `s_promptView`, `s_pinEntry` static UI state
- `Fido2Ui.cpp:46-50`: `s_listItems`, `s_labels`, `s_sortMap` static arrays

## Impact
1. **Maintenance burden**: State is hard to track across files; developers must understand 5+ files to understand FIDO2 state flow
2. **Race conditions**: No synchronization between `g_fido2.pin_verified` (fido2.cpp) and `g_client_pin` state (ctap2.cpp)
3. **Initialization coupling**: `fido2_storage_init()` populates `g_storage`, but `fido2_init()` must call it in correct order with `g_fido2` setup
4. **Testing difficulty**: Static state makes unit testing hard; requires full module init/reset between tests
5. **Memory not cleared on error**: If `fido2_init()` fails partway, `g_storage` and `g_fido2` may be partially initialized

## Recommended Fix
1. Create a unified `Fido2State` class in a new header `mod_fido2/Fido2State.h`:
   ```cpp
   class Fido2State {
   public:
       // Core state
       bool initialized;
       bool pin_verified;
       TaskHandle_t task_handle;
       
       // Storage state (migrate from g_storage)
       uint8_t cred_count;
       bool counter_loaded;
       
       // ClientPIN state (migrate from g_client_pin)
       bool client_pin_initialized;
       uint8_t pin_retries;
       
       // Methods for state transitions
       void init();
       void reset();
       bool isReady() const;
   private:
       // Private state members
   };
   
   // Singleton access
   Fido2State& getFido2State();
   ```

2. Migrate each `static struct` to members of `Fido2State`
3. Provide accessor functions for existing C-style API compatibility
4. Add state transition validation (e.g., `pin_verified` can only be set if `initialized`)

## References
- `components/mod_fido2/src/fido2_storage.cpp:52-75` - `g_storage` struct
- `components/mod_fido2/src/fido2.cpp:28-32` - `g_fido2` struct
- `components/mod_fido2/src/ctap2.cpp:24-37` - `g_ctap2` struct
- `components/mod_fido2/src/ctap2.cpp:45-60` - `g_client_pin` struct
- `components/mod_fido2/src/Fido2Ui.cpp:35-50` - Static UI state
