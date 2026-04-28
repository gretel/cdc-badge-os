---
title: "[HIGH] FIDO2 sign count can lose increments on power loss"
severity: HIGH
domain: data-integrity
lens: database
labels:
  - fido2-storage
  - sign-count
  - crash-safety
  - anti-replay
---

## Summary
The FIDO2 sign count (`sign_count`) is incremented in the in-memory cache first, then written to R-Memory. If the system loses power between these operations, the sign count reverts to the previous value. This breaks FIDO2 anti-replay guarantees as the same signature can appear "valid" again.

**Files:**
- `components/mod_fido2/src/fido2_storage.cpp:914-934` (increment_sign_count function)

## Impact
FIDO2 uses `sign_count` for anti-replay detection. If sign counts can revert:
- A captured signature from time T could be replayed after power loss
- The authenticator's sign_count would appear "older" than before
- Relying parties that track sign counts could miss replay attacks

Per FIDO2 spec, sign count should monotonically increase. Current implementation allows it to decrease after power loss.

## Evidence
In `fido2_storage.cpp:914-934`:
```cpp
uint32_t fido2_storage_increment_sign_count(uint8_t slot) {
    if (!slot_logical_valid(slot) || !g_storage.creds[slot].valid) {
        return 0;
    }

    // Increment local cache FIRST
    g_storage.creds[slot].sign_count++;
    uint32_t new_count = g_storage.creds[slot].sign_count;

    // Then write to R-Memory
    fido2_stored_cred_t stored;
    if (read_rmem_credential(slot, &stored)) {
        stored.sign_count = new_count;
        if (!write_rmem_credential(slot, &stored)) {
            LOG_E("FIDO2", "CRITICAL: Failed to persist sign count for slot %d!", slot);
        }
    }

    return new_count;
}
```

The cache is incremented first (line 921), then R-Memory is updated (line 925-929). If power is lost after line 921 but before line 929 completes, the cache will be reloaded from R-Memory on next boot with the OLD value.

Also note the "CRITICAL" log at line 928 - the code acknowledges this is a critical failure but doesn't prevent the cache from being incremented anyway.

## Recommended Fix
Write to R-Memory first, then increment cache:

```cpp
uint32_t fido2_storage_increment_sign_count(uint8_t slot) {
    if (!slot_logical_valid(slot) || !g_storage.creds[slot].valid) {
        return 0;
    }

    // 1. Read current stored data
    fido2_stored_cred_t stored;
    if (!read_rmem_credential(slot, &stored)) {
        LOG_E("FIDO2", "Failed to read credential for sign count");
        return 0;
    }

    // 2. Increment and write to R-Memory FIRST
    stored.sign_count++;
    if (!write_rmem_credential(slot, &stored)) {
        LOG_E("FIDO2", "CRITICAL: Failed to persist sign count for slot %d!", slot);
        return 0;  // Return 0 to indicate failure
    }

    // 3. Update local cache AFTER successful write
    g_storage.creds[slot].sign_count = stored.sign_count;

    return g_storage.creds[slot].sign_count;
}
```

This ensures:
- If R-Memory write fails, the function returns 0 (failure)
- If power is lost after write, the new value persists
- Cache always reflects persisted state

## References
- FIDO2 Spec: Sign Count for Anti-Replay: https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#anti-replay
- CTAP2 spec requires monotonic sign counts
- See `fido2_storage.cpp:892-910` for `delete_credential` pattern (reads then writes)
