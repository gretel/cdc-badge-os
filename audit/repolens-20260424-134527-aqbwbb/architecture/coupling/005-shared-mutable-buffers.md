---
title: "[MEDIUM] Shared mutable static buffers create state coupling between operations"
severity: MEDIUM
domain: Architecture/Coupling
lens: architecture/coupling
labels:
  - "audit:architecture/coupling"
---

## Summary
Multiple modules use shared static character buffers that are accessed by different functions within the module. This creates implicit coupling where one operation can modify data needed by another operation, especially problematic in callback-based flows.

**Evidence locations:**
- `components/mod_password/src/PasswordModule.cpp:438` - `s_passwordToType` buffer
- `components/mod_gpg/src/gpg.cpp:35` - `s_pending_user_id` buffer
- `components/mod_gpg/src/openpgp/openpgp.cpp:22` - `s_session_pin` buffer

## Impact
- **State corruption**: Callbacks can overwrite buffer data from previous operations
- **Reentrancy issues**: Same function called twice will share buffer state
- **Thread safety**: Not safe for interrupt callbacks or async operations
- **Hard to debug**: Buffer state changes are implicit and scattered

## Evidence
```cpp
// components/mod_password/src/PasswordModule.cpp:438
/** \brief Shared output buffer used for keyboard typing callback payload. */
static char s_passwordToType[PasswordStore::PASSWORD_LEN + 1] = {};

// Line 444-455: Callback reads shared buffer
static void onTypePassword(void* userData) {
    (void)userData;
    auto* kb = core::getKeyboard();
    if (kb && kb->isConnected()) {
        if (s_passwordToType[0]) {
            kb->typeString(s_passwordToType);  // Reads shared buffer
            ui::showToastSuccess("Typed");
        }
    }
}

// Line 461-475: Function writes to shared buffer
static void showDetails(uint16_t slot) {
    PasswordEntry entry = {};
    if (!PasswordStore::instance().readEntry(slot, &entry)) {
        ui::showToastError(ui::tr(ui::StringId::FAILED));
        return;
    }

    // Store password for type callback
    strncpy(s_passwordToType, entry.password, sizeof(s_passwordToType) - 1);  // Writes shared buffer
    s_passwordToType[sizeof(s_passwordToType) - 1] = '\0';
    // ... rest of function
}
```

**Problem scenario:**
1. User opens password list
2. User selects entry A, `showDetails(A)` writes password A to `s_passwordToType`
3. Before typing, user selects entry B, `showDetails(B)` overwrites with password B
4. User types - gets password B instead of A (or vice versa depending on timing)

```cpp
// components/mod_gpg/src/gpg.cpp:35
static char s_pending_user_id[GPG_USER_ID_MAX] = {};

// Used by gpg_generate_key() and gpg_set_pending_user_id()
// If user starts two generation flows, second overwrites first
```

```cpp
// components/mod_gpg/src/openpgp/openpgp.cpp:22
static char s_session_pin[OPENPGP_PIN_MAX_LEN + 1] = {};

// Session PIN shared across all OpenPGP operations
// If multiple operations need different PINs, they conflict
```

## Recommended Fix
**Pass data through callbacks instead of shared buffers:**

1. **For password typing callback:**
```cpp
// Before:
static char s_passwordToType[PasswordStore::PASSWORD_LEN + 1] = {};

static void onTypePassword(void* userData) {
    auto* kb = core::getKeyboard();
    if (kb && kb->isConnected()) {
        if (s_passwordToType[0]) {
            kb->typeString(s_passwordToType);
        }
    }
}

static void showDetails(uint16_t slot) {
    strncpy(s_passwordToType, entry.password, sizeof(s_passwordToType) - 1);
    // ...
}

// After:
struct TypePasswordContext {
    char password[PasswordStore::PASSWORD_LEN + 1];
};

static void onTypePassword(void* userData) {
    auto* ctx = static_cast<TypePasswordContext*>(userData);
    auto* kb = core::getKeyboard();
    if (kb && kb->isConnected() && ctx->password[0]) {
        kb->typeString(ctx->password);
    }
    delete ctx;  // Clean up context
}

static void showDetails(uint16_t slot) {
    auto* ctx = new TypePasswordContext();
    strncpy(ctx->password, entry.password, sizeof(ctx->password) - 1);
    s_infoView.setYesNoCallbacks(onTypePassword, nullptr, ctx);  // Pass context
}
```

2. **For pending user ID:**
```cpp
// Before:
static char s_pending_user_id[GPG_USER_ID_MAX] = {};

static void gpg_set_pending_user_id(const char* userId) {
    strncpy(s_pending_user_id, userId, sizeof(s_pending_user_id) - 1);
}

// After:
struct GpgWizardState {
    char user_id[GPG_USER_ID_MAX];
    uint8_t curve;
    // ... other state
};

static void wizardGenerateKey() {
    GpgWizardState state = {};
    strncpy(state.user_id, userId, sizeof(state.user_id) - 1);
    state.curve = curve;
    gpg_generate_key_with_state(&state);
}
```

3. **For session PIN:**
```cpp
// Before:
static char s_session_pin[OPENPGP_PIN_MAX_LEN + 1] = {};

// After:
struct OpenPgpSession {
    char pin[OPENPGP_PIN_MAX_LEN + 1];
    uint32_t timestamp;
};

static void openpgp_verify_pin(const char* pin) {
    OpenPgpSession session = {};
    strncpy(session.pin, pin, sizeof(session.pin) - 1);
    // Use session instead of global
}
```

## References
- [Mutable State on Wikipedia](https://en.wikipedia.org/wiki/Mutable_data)
- [Callback Pattern](https://en.wikipedia.org/wiki/Callback_(computer_programming))
- [Context Object Pattern](https://refactoring.guru/design-patterns/context-object)
