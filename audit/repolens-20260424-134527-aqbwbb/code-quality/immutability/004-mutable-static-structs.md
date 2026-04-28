---
title: "[LOW] Mutable static structs and arrays with aggregate initialization"
severity: LOW
domain: code-quality
lens: immutability
labels:
  - "audit:code-quality/immutability"
---

## Summary
Multiple source files use static structs and arrays with aggregate initialization (`= {}`) that could benefit from const qualification if they are only assigned once during initialization. While these are module-local variables, using `const` where appropriate improves code clarity and can enable better compiler optimizations.

**Files affected:**
- `components/mod_totp/src/TotpModule.cpp:570`: `static WizardState s_wizard = {};`
- `components/mod_gpg/src/GpgModule.cpp:232`: `static WizardState s_wizard = {};`
- `components/mod_gpg/src/gpg.cpp:17-18`: `static gpg_metadata_t s_metadata = {};`, `static char s_pending_user_id[GPG_USER_ID_MAX] = {};`
- `components/mod_hid/src/BleHidKeyboard.cpp:50`: `static KeyboardReport s_currentReport = {};`
- `components/mod_password/src/PasswordModule.cpp:357,438`: `static WizardState s_wizard = {};`, `static char s_passwordToType[...] = {};`
- `components/mod_vcard/src/ble_vcard.cpp:116-189`: Multiple static structs and arrays
- `components/mod_vcard/src/VcardModule.cpp:110-125`: Multiple static structs and arrays
- `components/mod_fido2/src/Fido2Ui.cpp:98,276`: `static char s_promptRpId[...] = {};`
- `components/serial_cmd/src/SerialCmd.cpp:45`: `static char s_cmdBuffer[...] = {};`
- `components/cdc_os_ui/src/AppUi.cpp:94`: `static UiDeps s_deps = {};`

## Impact
- **Code clarity**: `const` makes intent explicit - readers know the variable is initialized once and conceptually "fixed"
- **Compiler optimizations**: `const` variables can be placed in flash memory (important for embedded)
- **Accidental mutation**: Prevents accidental writes to variables that should be read-only after initialization
- **Self-documenting code**: Reduces cognitive load by making state management explicit

## Evidence
Examples from the codebase:

From `components/mod_vcard/src/ble_vcard.cpp:116-130`:
```cpp
static vcard_peer_t s_peers[MAX_PEERS] = {};
static vcard_peer_t s_nearby_peer = {};
static char s_tx_vcard[VCARD_MAX_LEN] = {};
static char s_rx_vcard[VCARD_MAX_LEN] = {};
static char s_exchange_error[64] = {};
static uint8_t s_target_addr[6] = {};
```

From `components/mod_vcard/src/VcardModule.cpp:110-125`:
```cpp
static vcard_peer_t s_uiPeers[MAX_UI_PEERS] = {};
static ui::ListItem s_peerItems[MAX_UI_PEERS + 1] = {};
static char s_peerLabels[MAX_UI_PEERS][48] = {};
static ui::ListItem s_mainMenuItems[MENU_COUNT] = {};
```

From `components/mod_gpg/src/gpg.cpp:17-18`:
```cpp
static gpg_metadata_t s_metadata = {};
static char s_pending_user_id[GPG_USER_ID_MAX] = {};
```

Note: These variables are typically populated during module initialization and then read frequently. The question is whether they are mutated after initialization or remain constant.

## Recommended Fix
Review each variable to determine if it's mutated after initialization:

1. **If only initialized once**: Use `const`
   ```cpp
   // Before
   static vcard_peer_t s_peers[MAX_PEERS] = {};
   
   // After (if never modified after init)
   static vcard_peer_t s_peers[MAX_PEERS];  // Zero-initialized by default
   // Initialize in init function with const_cast or separate init struct
   ```

2. **If initialized once in init function**: Consider using a getter with static local
   ```cpp
   // Before
   static char s_cmdBuffer[SerialCmd::CMD_BUFFER_SIZE] = {};
   
   // After
   static char& getCmdBuffer() {
       static char buffer[SerialCmd::CMD_BUFFER_SIZE] = {};
       return buffer;
   }
   ```

3. **For structs that change**: Keep mutable but document the lifecycle
   ```cpp
   /**
    * \brief GPG metadata - initialized once in gpg_init().
    * Mutable during init, read-only after.
    */
   static gpg_metadata_t s_metadata = {};
   ```

4. **For buffers that are actively used**: Keep mutable but consider encapsulation
   ```cpp
   // Instead of global mutable buffer
   static char s_cmdBuffer[...] = {};
   
   // Consider a class with private buffer
   class CommandParser {
       char buffer_[SerialCmd::CMD_BUFFER_SIZE] = {};
   public:
       char* getBuffer() { return buffer_; }
   };
   ```

This is a LOW severity issue because:
- These are module-local statics, not truly global
- The code works correctly as-is
- The main benefit is improved code clarity and documentation of intent

## References
- C++ Core Guidelines [C.24](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c24-use-typedefs-or-using-for-constants-not-define)
- "Effective C++" by Scott Meyers, Item 21: Use `const` whenever possible
- Embedded C++ guidelines recommend `const` for read-only data to enable flash placement
