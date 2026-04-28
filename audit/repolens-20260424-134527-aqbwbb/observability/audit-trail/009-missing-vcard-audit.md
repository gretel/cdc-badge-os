---
title: "[MEDIUM] Missing audit trail for vCard state changes"
severity: MEDIUM
domain: observability
lens: audit-trail
labels:
  - "audit:observability/audit-trail"
---

## Summary
The vCard module (`components/mod_vcard/src/vcard_store.cpp`) performs state-changing operations (add, update, delete) on vCard data without any audit logging. Key operations that modify persistent state include:

- `vcard_store_set_own()` - Updates the local vCard (line 337)
- `vcard_store_clear_own()` - Deletes the local vCard (line 415)
- `vcard_store_add()` - Adds a new peer vCard (line 515)
- `vcard_store_delete()` - Deletes a peer vCard (line 579)

Each operation logs basic success/failure via `LOG_I` or `LOG_W`, but there is no structured audit record capturing:
- **What** operation was performed (add/update/delete)
- **When** it happened (timestamp with timezone)
- **Which** vCard was affected (slot number, peer name, hash)
- **Context** (serial command, BLE exchange, UI action)

## Impact
Without audit trails for vCard operations:
1. **Forensic analysis** - Cannot reconstruct which vCards were exchanged or when
2. **Compliance** - No record of contact data modifications for privacy audits
3. **Debugging** - Hard to trace issues with vCard synchronization or duplicate detection
4. **Security** - Cannot detect unauthorized vCard modifications or mass deletions

## Evidence
File: `components/mod_vcard/src/vcard_store.cpp`

**vCard addition (no audit):**
```cpp
// Line 570-588
bool vcard_store_add(const char* vcard, size_t len, char* err, size_t err_len) {
    // ... validation ...
    esp_err_t ret = nvs_set_str(nvs, key, tmp);
    // ... commit ...
    g_cards[free_slot].used = true;
    g_cards[free_slot].hash = hash;
    g_card_count++;
    return true;  // No audit log!
}
```

**vCard deletion (no audit):**
```cpp
// Line 579-592
bool vcard_store_delete(uint16_t slot) {
    // ... erase ...
    g_cards[slot].used = false;
    g_card_count--;
    return true;  // No audit log!
}
```

**Own vCard update (minimal logging only):**
```cpp
// Line 337-374
bool vcard_store_set_own(const char* vcard, size_t len, char* err, size_t err_len) {
    // ... validation and filtering ...
    LOG_I(TAG, "Own vCard stored (%d bytes)", (int)len);  // No context, no timestamp
    return true;
}
```

## Recommended Fix
Add audit logging to vCard state changes. Since this is an embedded system without a full audit framework, implement minimal structured logging:

1. **Create audit helper function** in `vcard_store.cpp`:
```cpp
static void audit_vcard_change(const char* action, uint16_t slot, const char* details) {
    uint32_t now = esp_timer_get_time() / 1000;  // ms since boot
    LOG_I(TAG, "AUDIT: %s | slot=%u | time=%lu | %s", action, slot, now, details);
}
```

2. **Add audit calls** to existing functions:
```cpp
// In vcard_store_add():
audit_vcard_change("VCARD_ADD", free_slot, hash_str);

// In vcard_store_delete():
audit_vcard_change("VCARD_DEL", slot, g_cards[slot].display);

// In vcard_store_set_own():
audit_vcard_change("VCARD_SET_OWN", 0, "updated");
```

3. **For vCard exchange via BLE** (`ble_vcard.cpp`), add audit at exchange completion:
```cpp
// In onExchangeComplete callback:
audit_vcard_change("VCARD_EXCHANGE", slot, peer_name);
```

## References
- NIST SP 800-92: Guide to Computer Security Log Management
- FIDO2 spec requires audit of credential operations (applicable by analogy)
- Related to existing findings: `001-no-audit-trail-framework.md`, `006-insufficient-log-structure.md`
