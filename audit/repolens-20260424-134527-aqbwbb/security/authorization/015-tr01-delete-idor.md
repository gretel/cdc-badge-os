---
title: "[MEDIUM] TR01_ECC_DEL and TR01_RMEM_DEL allow deleting any slot without ownership check"
severity: MEDIUM
domain: authorization
lens: idor-secure-element
labels:
  - "medium:idor-delete"
---

## Summary
The `TR01_ECC_DEL` and `TR01_RMEM_DEL` commands allow deleting any ECC or R-Memory slot by slot number without verifying slot ownership. This enables destruction of other modules' data.

**File:** `components/serial_cmd/src/SerialCmd.cpp`  
**Lines:** 1020-1037 (ECC_DEL), 1043-1059 (RMEM_DEL), 1476-1477

## Evidence

**Command registration (lines 1476-1477):**
```cpp
reg.registerCommand({"TR01_ECC_DEL", "Delete ECC key slot", cmdTr01EccDel, "tr01", true});
reg.registerCommand({"TR01_RMEM_DEL", "Delete R-Memory slot", cmdTr01RmemDel, "tr01", true});
```

**TR01_ECC_DEL handler (lines 1020-1037):**
```cpp
static void cmdTr01EccDel(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::ECC_SLOT_COUNT, "ECC slot");
    if (!result.valid) {
        Console::printf("Usage: TR01_ECC_DEL <slot>\r\n");
        return;
    }

    auto* se = getSecureElementWithCheck();
    if (!se) return;

    uint8_t slot = static_cast<uint8_t>(result.value);
    hal::SeResult seResult = se->eccDelete(slot);
    if (seResult == hal::SeResult::OK) {
        Console::printf("OK: ECC slot %d deleted\r\n");
    } else {
        Console::printf("ERROR: Delete failed\r\n");
    }
}
```

**TR01_RMEM_DEL handler (lines 1043-1059):**
```cpp
static void cmdTr01RmemDel(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::RMEM_SLOT_COUNT, "R-Memory slot");
    if (!result.valid) {
        Console::printf("Usage: TR01_RMEM_DEL <slot>\r\n");
        return;
    }

    auto* se = getSecureElementWithCheck();
    if (!se) return;

    uint16_t slot = static_cast<uint16_t>(result.value);
    hal::SeResult seResult = se->rmemErase(slot);
    // ...
}
```

The commands use `requiresAuth = true`, but:
1. When `FEATURE_SECURE_SERIAL` is disabled, no authentication occurs
2. No slot ownership validation is performed
3. Any slot can be deleted by specifying its number

## Impact
An attacker can:
1. Delete GPG keys (ECC slots 1-3)
2. Delete CA certificate (ECC slot 4)
3. Delete FIDO2 keys (ECC slots 5-31)
4. Delete TOTP accounts (R-Memory slots 32-131)
5. Delete password entries (R-Memory slots 132-511)

This is an Insecure Direct Object Reference (IDOR) vulnerability with destructive consequences.

## Recommended Fix
Add ownership validation before deletion:

```cpp
static void cmdTr01EccDel(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::ECC_SLOT_COUNT, "ECC slot");
    if (!result.valid) {
        Console::printf("Usage: TR01_ECC_DEL <slot>\r\n");
        return;
    }

    auto* se = getSecureElementWithCheck();
    if (!se) return;

    uint8_t slot = static_cast<uint8_t>(result.value);
    
    // Verify slot ownership - get module ID from slot map
    auto& slotMap = core::TropicSlotMap::instance();
    core::TropicSlotMap::SlotRange range;
    bool found = false;
    uint8_t moduleId = 0;
    
    // Find which module owns this slot
    for (uint8_t mid = 1; mid < 255; mid++) {
        if (slotMap.getRangeByModuleId(mid, core::TropicSlotMap::SlotType::ECC, &range)) {
            if (slot >= range.start && slot <= range.end) {
                moduleId = mid;
                found = true;
                break;
            }
        }
    }
    
    if (!found) {
        Console::printf("ERROR: Slot not assigned to any module\r\n");
        return;
    }
    
    // Require module-specific authentication
    // For example, check if session is authenticated for this module
    // or require PIN verification
    
    hal::SeResult seResult = se->eccDelete(slot);
    // ...
}
```

Alternatively, remove these generic commands and use module-specific delete commands that inherently validate ownership.

## References
- CWE-639: Insecure Direct Object Reference
- CWE-287: Improper Authentication
- OWASP API Security Top 10 - IDOR
