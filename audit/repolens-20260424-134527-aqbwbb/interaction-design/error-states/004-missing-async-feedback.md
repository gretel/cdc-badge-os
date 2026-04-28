---
title: "[MEDIUM] Missing toast/notification for async background operations"
severity: MEDIUM
domain: interaction-design/error-states
lens: async-feedback
labels:
  - "audit:interaction-design/error-states"
---

## Summary
Background operations (sync, save, update) that fail silently without user feedback. Users don't know if their data was saved or if the operation failed.

**Affected operations:**
- NVS storage writes (`components/mod_nvsedit/src/NvsEditModule.cpp`)
- TOTP account updates (`components/mod_totp/src/TotpModule.cpp`)
- GPG key operations (`components/mod_gpg/src/GpgModule.cpp`)
- Password store writes (`components/mod_password/src/PasswordModule.cpp`)

## Impact
**Data loss risk:** Users assume their changes saved successfully, but silent failures mean data isn't persisted.

**No confirmation:** Even successful operations sometimes lack feedback, leaving users uncertain.

## Evidence
```cpp
// components/mod_nvsedit/src/NvsEditModule.cpp
// Delete operation - shows error but no success confirmation
void deleteEntry() {
    bool ok = PasswordStore::instance().deleteEntry(slot);
    if (!ok) {
        showToastError("Delete failed");
        // Success case: no feedback, user doesn't know if it worked
    }
}

// components/mod_totp/src/TotpModule.cpp:870
static void wizardFinish() {
    if (strlen(s_wizard.name) == 0 || strlen(s_wizard.secret) == 0) {
        ui::showToastError(mstr(STR_INVALID_INPUT));
        // ...
        return;
    }

    bool ok = false;
    if (s_wizard.editMode) {
        ok = TotpStore::instance().updateAccount(...);  // Returns bool
    } else {
        ok = TotpStore::instance().addAccount(...);
    }

    if (ok) {
        ui::showToastSuccess(ui::tr(ui::StringId::OK));  // Generic "OK"
        // Should say "Account saved" or "Account updated"
    } else {
        ui::showToastError(ui::tr(ui::StringId::FAILED));
        // No detail about what went wrong
    }
}

// components/mod_gpg/src/GpgModule.cpp:298
static void onResetConfirm(void*) {
    if (gpg_reset()) {
        ui::showToastSuccess(ui::tr(ui::StringId::OK));  // Generic
    } else {
        ui::showToastError(ui::tr(ui::StringId::FAILED));  // Generic
    }
}

// components/mod_password/src/PasswordModule.cpp
static void onMenuDeleteConfirm(void* userData) {
    uint16_t slot = *static_cast<uint16_t*>(userData);
    bool ok = PasswordStore::instance().deleteEntry(slot);
    if (ok) {
        ui::showToastSuccess(mstr(STR_DELETED));  // Good - specific message
    } else {
        ui::showToastError(ui::tr(ui::StringId::FAILED));  // Generic
    }
}
```

**Missing success feedback in serial commands:**
```cpp
// components/mod_totp/src/TotpModule.cpp:262
static void cmd_totp_add(const char* args) {
    // ...
    bool ok = TotpStore::instance().addAccount(...);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");  // Minimal feedback
}
```

## Recommended Fix
**1. Add specific success messages for each operation:**

```cpp
// mod_totp strings
STR_ACCOUNT_SAVED = "Account saved"
STR_ACCOUNT_UPDATED = "Account updated"
STR_ACCOUNT_ADDED = "Account added"

// mod_gpg strings
STR_KEYS_GENERATED = "Keys generated"
STR_KEY_EXPORTED = "Key exported"
STR_GPG_RESET = "GPG reset complete"

// Update wizardFinish
if (ok) {
    const char* msg = s_wizard.editMode ? mstr(STR_ACCOUNT_UPDATED) : mstr(STR_ACCOUNT_ADDED);
    ui::showToastSuccess(msg);
} else {
    ui::showToastError(mstr(STR_SAVE_FAILED));
}
```

**2. Add loading/progress feedback for long operations:**

```cpp
// For operations that take > 500ms
ui::showToastTask(mstr(STR_SAVING)...);  // "Saving..." with task icon
bool ok = PasswordStore::instance().updateEntry(...);
if (ok) {
    ui::showToastSuccess(mstr(STR_SAVED), 1000);  // Auto-dismiss after 1s
} else {
    ui::showToastError(mstr(STR_SAVE_FAILED));
}
```

**3. Serial command improvements:**

```cpp
// More detailed serial output
static void cmd_totp_add(const char* args) {
    // ...
    bool ok = TotpStore::instance().addAccount(...);
    if (ok) {
        cdc::serial::Console::printf("OK: Account '%s' added\r\n", name);
    } else {
        cdc::serial::Console::printf("ERROR: Failed to add account\r\n");
    }
}
```

**4. Add NVS write confirmation:**

```cpp
// components/mod_nvsedit/src/NvsEditModule.cpp
bool saveToNVS(const char* key, const char* value) {
    nvs_handle_t nvs;
    if (nvs_open("edit", NVS_READWRITE, &nvs) != ESP_OK) {
        return false;
    }
    bool ok = (nvs_set_str(nvs, key, value) == ESP_OK);
    if (ok) {
        ok = (nvs_commit(nvs) == ESP_OK);
    }
    nvs_close(nvs);
    return ok;
}
```

**Estimated effort:** 1 hour for i18n string additions, 30 min for message updates per module

## References
- [Material Design: Feedback](https://material.io/design/communication/feedback.html)
- [Human Interface Guidelines: Progress Indicators](https://developer.apple.com/design/human-interface-guidelines/progress-indicators/)
