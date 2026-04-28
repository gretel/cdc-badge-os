---
title: "[MEDIUM] GPG Reset via Serial Command Lacks Confirmation"
severity: MEDIUM
domain: destructive-actions
lens: serial-commands
labels:
  - audit:ux-antipatterns/destructive-actions
---

## Summary
The GPG module's serial command `GPG_RESET` executes immediate reset of all GPG key material without any confirmation step. The delete handler `cmd_gpg_reset()` in `components/mod_gpg/src/GpgModule.cpp:200-204` directly calls `gpg_reset()` with no intermediate confirmation or prompt.

While the UI-based reset flow in `GpgModule.cpp` correctly uses a confirmation dialog (`showConfirm()` at line 529-532), the serial command bypasses this protection entirely.

**Evidence:**
- File: `components/mod_gpg/src/GpgModule.cpp`
- Lines: 196-204
- Function: `cmd_gpg_reset(const char* args)`

```cpp
/**
 * \brief Serial command resetting GPG key material.
 * \param args Unused command arguments.
 */
static void cmd_gpg_reset(const char* args) {
    (void)args;
    bool ok = gpg_reset();  // Immediate reset!
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

The `gpg_reset()` function (in `components/mod_gpg/src/gpg.cpp:429-447`) performs the following destructive operations:
- Deletes SIG, DEC, and AUT keys from secure element ECC slots
- Clears all key fingerprints
- Resets metadata including user ID and creation timestamp
- Erases NVS namespace

```cpp
bool gpg_reset(void) {
    if (!gpg_storage_ready()) return false;
    se_delete_key(gpg_storage_sig_slot());
    se_delete_key(gpg_storage_dec_slot());
    se_delete_key(gpg_storage_aut_slot());
    uint8_t zero_fp[GPG_FINGERPRINT_LEN] = {};
    openpgp_set_key_fingerprint(KEY_SIG, zero_fp, 0);
    openpgp_set_key_fingerprint(KEY_DEC, zero_fp, 0);
    openpgp_set_key_fingerprint(KEY_AUT, zero_fp, 0);
    memset(&s_metadata, 0, sizeof(s_metadata));
    s_initialized = false;
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_erase_key(handle, NVS_KEY_META);
        nvs_commit(handle);
        nvs_close(handle);
    }
    return true;
}
```

## Impact
- **Data Loss Risk**: Users can accidentally wipe all GPG keys via serial console with a single command
- **No Recovery**: GPG keys are stored in secure element ECC slots; once deleted, the key pair is lost permanently (no backup exists on the badge)
- **High Impact**: This affects all three key slots (Signature, Decryption, Authentication) simultaneously
- **Inconsistent UX**: The UI flow uses `showConfirm()` with `STR_CONFIRM_RESET` ("Alle GPG Keys loeschen?"), but serial command has no such protection

## Recommended Fix
Add a confirmation step to the `cmd_gpg_reset()` serial command handler using a two-step confirmation pattern similar to `TR01_WIPE`:

1. First invocation shows a warning and requires `CONFIRM` argument
2. Second invocation with `CONFIRM` executes the actual reset

```cpp
static void cmd_gpg_reset(const char* args) {
    (void)args;
    
    // Check for confirmation argument
    if (args && strcmp(args, "CONFIRM") == 0) {
        bool ok = gpg_reset();
        cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
        return;
    }
    
    // Show confirmation prompt
    cdc::serial::Console::printf("WARNING: Reset all GPG keys?\r\n");
    cdc::serial::Console::printf("  This will delete SIG, DEC, and AUT keys.\r\n");
    cdc::serial::Console::printf("  User ID and metadata will be cleared.\r\n");
    cdc::serial::Console::printf("  Keys cannot be recovered!\r\n");
    cdc::serial::Console::printf("  To proceed, type: GPG_RESET CONFIRM\r\n");
}
```

## References
- UI confirmation pattern: `GpgModule.cpp:529-532` uses `showConfirm(mstr(STR_CONFIRM_RESET), ...)`
- Similar pattern: `TR01_WIPE` command in `serial_cmd/src/SerialCmd.cpp:1132` uses `CONFIRM` argument pattern
- GPT reset implementation: `gpg.cpp:429-447`
