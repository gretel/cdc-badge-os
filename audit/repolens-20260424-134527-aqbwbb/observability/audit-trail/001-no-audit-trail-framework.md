---
title: "[HIGH] No audit trail framework for state changes and security events"
severity: HIGH
domain: observability
lens: audit-trail
labels:
  - "audit:observability/audit-trail"
---

## Summary

The CDC Badge OS firmware lacks a dedicated audit trail framework to record state changes, security events, and administrative actions. The existing `cdc_log` library provides general logging but does not offer structured, queryable audit records with the necessary context (who/what/when/where) for forensic analysis.

**Files affected:**
- `components/cdc_log/include/cdc_log.h` - General logging only, no audit-specific structure
- `components/cdc_core/src/EventBus.cpp` - Event bus exists but no audit event types
- `components/cdc_core/include/cdc_core/EventBus.h` - EventType enum lacks audit events

## Impact

Without a dedicated audit trail:

1. **Forensic Analysis**: Security incidents (PIN brute-force attacks, credential creation, key generation) cannot be reconstructed after the fact.
2. **Compliance**: FIDO2 certification and security audits require audit logs for state transitions.
3. **Accountability**: No way to determine what actions occurred between device sessions.
4. **Debugging**: Hard to diagnose issues like unexpected credential loss or PIN lockouts.

## Evidence

### Current logging is ad-hoc and unstructured

In `PinManager.cpp:328`:
```cpp
pw1Retries_--;
saveToStorage();  // Persist retry count
LOG_W(TAG, "Wrong PW1, %d retries left", pw1Retries_);
```

This logs the retry, but:
- No timestamp with timezone/uptime context
- No correlation ID for tracking related events
- No structured format for parsing/analysis
- Log messages are volatile (not persisted to durable storage)

### Event bus lacks audit event types

In `EventBus.h:11-44`, the `EventType` enum includes:
```cpp
enum class EventType : uint8_t {
    KEY_PRESSED,
    POWER_USB_CONNECTED,
    SYSTEM_UNLOCK,
    MODULE_ERROR,
    // ...
};
```

Missing audit-relevant event types:
- `PIN_VERIFIED` / `PIN_FAILED`
- `KEY_GENERATED`
- `CREDENTIAL_CREATED` / `CREDENTIAL_DELETED`
- `MODULE_INITIALIZED` / `MODULE_RESET`
- `SETTINGS_CHANGED`

### Key state changes logged but not audited

In `GpgStorage.cpp:313`:
```cpp
LOG_I(TAG, "Saved encrypted DEC private key to R-Memory slot %d", rmem_slot);
```

In `fido2_storage.cpp:886`:
```cpp
LOG_I("FIDO2", "Created %s credential in slot %d", curve_name, slot);
```

These log the action but lack:
- Actor identification (which user/initiated the action)
- Session context (USB, BLE, keypad?)
- Before/after state for tracking changes
- Machine-readable event type for querying

## Recommended Fix

### Phase 1: Define Audit Event Structure (30 min)

Create `components/cdc_core/include/cdc_core/AuditLog.h`:

```cpp
#pragma once
#include <cstdint>

namespace cdc::core {

enum class AuditEventType : uint8_t {
    PIN_VERIFY_SUCCESS = 1,
    PIN_VERIFY_FAILURE,
    PIN_CHANGE,
    KEY_GENERATE,
    KEY_DELETE,
    CREDENTIAL_CREATE,
    CREDENTIAL_DELETE,
    MODULE_INIT,
    MODULE_START,
    MODULE_STOP,
    MODULE_RESET,
    SYSTEM_LOCK,
    SYSTEM_UNLOCK,
    USB_CONNECT,
    USB_DISCONNECT,
    CONFIG_CHANGE
};

struct AuditRecord {
    AuditEventType type;
    uint32_t timestamp_ms;
    uint8_t module_id;      // Which module triggered this
    uint8_t slot_id;        // ECC/RMEM slot affected (if applicable)
    uint8_t result;         // 0=success, 1=failure, 2=partial
    uint8_t context;        // Bitmask: USB=1, BLE=2, KEYPAD=4
    uint32_t details;       // Extended info (e.g., retry count, slot count)
};

void audit_init(void);
void audit_record(AuditEventType type, uint8_t module_id, uint8_t slot_id,
                  uint8_t result, uint8_t context, uint32_t details);
size_t audit_get_records(AuditRecord* out, size_t max_records);
void audit_clear(void);
}
```

### Phase 2: Integrate into existing state changes (30 min)

Update key files to call `audit_record()`:

1. `PinManager.cpp`: Add audit on PIN verify/change
2. `GpgStorage.cpp`: Add audit on key save/delete
3. `fido2_storage.cpp`: Add audit on credential create/delete
4. `ModuleRegistry.cpp`: Add audit on module init/start/stop

## References

- FIDO2 Specification Section 6.1 (Authenticator Data) - requires tracking state changes
- NIST SP 800-53 Rev. 5 AU-2 (Audit Events) - core audit requirements
- Common Criteria EAL2+ - audit trail requirements for security tokens
