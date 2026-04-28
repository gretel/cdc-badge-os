---
title: "[MEDIUM] GPG_GENERATE command lacks idempotency check for existing keys"
severity: MEDIUM
domain: api-design/api-idempotency
lens: api-idempotency
labels:
  - audit:api-design/api-idempotency
---

## Summary
The `GPG_GENERATE` serial command (and `gpg_generate_key()` function) generates new GPG keys without checking if keys already exist. When called multiple times, it overwrites existing keys in the same secure element slots, losing the previous key material without explicit user intent.

**Location:** `components/mod_gpg/src/gpg.cpp:354-423` (gpg_generate_key), `components/mod_gpg/src/GpgModule.cpp:150-179` (cmd_gpg_generate)

## Impact
- **Accidental key loss:** Re-running `GPG_GENERATE` overwrites existing keys silently
- **No confirmation step:** Unlike `GPG_RESET` which is typically followed by `GPG_GENERATE`, calling `GPG_GENERATE` alone can destroy existing keys
- **Inconsistent with reset pattern:** `GPG_RESET` clears keys explicitly, but `GPG_GENERATE` assumes reset was done first
- **Slot exhaustion risk:** Each overwrite uses the same slots (SIG, DEC, AUT), but if interrupted mid-process, could leave keys in inconsistent state

## Evidence
```cpp
// components/mod_gpg/src/gpg.cpp:354-423
bool gpg_generate_key(uint8_t curve) {
    if (!gpg_storage_ready()) return false;
    if (!gpg_has_pending_user_id() && !s_initialized) return false;  // Only checks if initialized, not if keys exist

    uint8_t sig_slot = gpg_storage_sig_slot();
    uint8_t dec_slot = gpg_storage_dec_slot();
    uint8_t aut_slot = gpg_storage_aut_slot();

    if (!se_generate_key(sig_slot, curve)) return false;  // Overwrites existing key
    if (!se_generate_key(aut_slot, curve)) return false;  // Overwrites existing key
    if (!se_generate_key(dec_slot, CDC_CURVE_P256)) return false;  // Overwrites existing key

    // ... generates new metadata and saves ...
    s_initialized = true;
    s_pending_user_id[0] = '\0';
    return true;
}
```

Serial command handler (no duplicate check):
```cpp
// components/mod_gpg/src/GpgModule.cpp:150-179
static void cmd_gpg_generate(const char* args) {
    char curveBuf[8] = {};
    char userId[GPG_USER_ID_MAX] = {};

    // Parses curve and user_id
    // Sets pending user ID
    gpg_set_pending_user_id(userId);
    bool ok = gpg_generate_key(curve);  // Overwrites existing keys without warning
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

The check `if (!gpg_has_pending_user_id() && !s_initialized)` only validates that:
1. A user ID is pending, OR
2. Keys were already initialized (but doesn't prevent overwrite)

It does NOT check if keys already exist in the secure element slots.

## Recommended Fix
Add a check for existing keys before generating new ones:

1. **Check if keys exist** in the secure element before overwriting
2. **Return error** or prompt for confirmation if keys already exist
3. **Follow GPG_RESET** pattern where explicit reset is required first

Example implementation:
```cpp
bool gpg_generate_key(uint8_t curve) {
    if (!gpg_storage_ready()) return false;
    if (!gpg_has_pending_user_id() && !s_initialized) return false;

    uint8_t sig_slot = gpg_storage_sig_slot();
    uint8_t dec_slot = gpg_storage_dec_slot();
    uint8_t aut_slot = gpg_storage_aut_slot();

    // Check if keys already exist
    uint8_t tmp_key[64];
    uint8_t tmp_curve;
    bool keys_exist = false;

    if (se_get_pubkey(sig_slot, tmp_key, sizeof(tmp_key), &tmp_curve)) {
        keys_exist = true;
    }

    if (keys_exist) {
        LOG_W("GPG", "Keys already exist, use GPG_RESET first");
        return false;  // Or return true to indicate idempotent success
    }

    // ... rest of existing key generation code ...
}
```

Alternatively, for a more permissive approach (replace if exists):
```cpp
bool gpg_generate_key(uint8_t curve) {
    if (!gpg_storage_ready()) return false;
    if (!gpg_has_pending_user_id() && !s_initialized) return false;

    uint8_t sig_slot = gpg_storage_sig_slot();
    uint8_t dec_slot = gpg_storage_dec_slot();
    uint8_t aut_slot = gpg_storage_aut_slot();

    // Delete existing keys first if they exist
    se_delete_key(sig_slot);
    se_delete_key(aut_slot);
    se_delete_key(dec_slot);

    // ... generate new keys ...
}
```

For serial command, add explicit confirmation:
```cpp
static void cmd_gpg_generate(const char* args) {
    // ... parse args ...

    // Check if keys exist
    if (gpg_keys_exist()) {
        cdc::serial::Console::printf("WARNING: Keys already exist!\r\n");
        cdc::serial::Console::printf("Use 'GPG_RESET' first to clear, then run again.\r\n");
        return;
    }

    gpg_set_pending_user_id(userId);
    bool ok = gpg_generate_key(curve);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

## References
- GPG_RESET command already exists for explicit key clearing
- Similar pattern: FIDO2 credentials check for existing RP+User before creation
- OpenPGP card spec: Generate keys should fail if keys already present unless explicitly reset
