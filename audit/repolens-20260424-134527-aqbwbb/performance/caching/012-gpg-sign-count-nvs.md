---
title: "[LOW] GPG sign-count update bypasses NVS commit optimization"
severity: LOW
domain: performance/caching
lens: embedded-firmware
labels:
  - "nvs-cache"
  - "gpg-optimization"
---

## Summary
The GPG module (`components/mod_gpg/src/openpgp/openpgp.cpp`) increments the sign-count and immediately persists it to NVS on every signature operation. The `save_state_to_nvs()` function is called after each increment, performing a full NVS open/write/commit cycle for a single value update.

**Evidence:**
- File: `components/mod_gpg/src/openpgp/openpgp.cpp`
- Line 550-570: `save_state_to_nvs()` function with full NVS cycle
- Sign count incremented during signature operations (around line 1258 area)
- NVS commit happens synchronously after each sign operation

## Impact
**Performance Cost:**
- Each signature operation triggers: NVS open → get_u32 → set_u32 → commit → close
- NVS commit is synchronous and blocks execution (~5-10ms)
- For batch signing operations (e.g., signing multiple documents), this adds up
- Typical GPG use case: 1-5 signatures per session, so impact is minimal

**Flash Wear:**
- Each NVS commit may trigger an erase cycle depending on NVS implementation
- Sign count is small (4 bytes) but stored in separate NVS key
- For high-volume signing (100s of signatures), this contributes to flash wear

## Evidence
From `components/mod_gpg/src/openpgp/openpgp.cpp`:

```cpp
/**
 * \brief Persists OpenPGP runtime state to NVS.
 */
static void save_state_to_nvs(void) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        // Always writes, even if value unchanged
        nvs_set_u32(nvs, "sig_count", sig_count);
        
        // ... writes all other fields ...
        
        nvs_commit(nvs);  // Synchronous commit
        nvs_close(nvs);
    }
}

// Called after each signature (approx line 1258)
ESP_LOGI(TAG, "Signature created, count=%lu", sig_count);
// sig_count incremented, save_state_to_nvs() called
```

## Recommended Fix
Optionally defer NVS persistence for sign-count:

1. **Add dirty flag and batch writes**:
```cpp
static bool s_signCountDirty = false;

// Increment without immediate write
void incrementSigCount(void) {
    sig_count++;
    s_signCountDirty = true;
}

// Periodic or lazy save
static void maybeSaveState(void) {
    if (!s_signCountDirty) return;
    save_state_to_nvs();
    s_signCountDirty = false;
}

// Call on app deselect or idle
void openpgp_deselect(void) {
    maybeSaveState();
    app_selected = false;
}
```

2. **Or skip NVS write if unchanged**:
```cpp
static uint32_t s_lastSavedSigCount = 0;

static void save_state_to_nvs(void) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        // Only write if changed
        if (sig_count != s_lastSavedSigCount) {
            nvs_set_u32(nvs, "sig_count", sig_count);
            s_lastSavedSigCount = sig_count;
        }
        // ... other fields ...
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}
```

3. **For GPG, consider power-loss safety**:
Since sign count is critical for FIDO2-style replay protection, immediate persistence may be intentional. Document this design decision.

## References
- ESP-IDF NVS commit cost: ~5-10ms synchronous
- GPG signature frequency: Typically low (1-10 per session)
- Sign count importance: Critical for replay protection (FIDO2 spec)
