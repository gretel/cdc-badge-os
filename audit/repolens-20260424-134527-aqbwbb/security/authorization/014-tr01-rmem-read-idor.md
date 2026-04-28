---
title: "[MEDIUM] TR01_RMEM_READ allows reading any R-Memory slot without ownership check"
severity: MEDIUM
domain: authorization
lens: idor-secure-element
labels:
  - "medium:idor-rmem"
---

## Summary
The `TR01_RMEM_READ` command allows reading any R-Memory slot by slot number without verifying that the requesting user owns that slot. This enables information disclosure of other modules' data.

**File:** `components/serial_cmd/src/SerialCmd.cpp`  
**Lines:** 992-1014

## Evidence

**Command registration (line 1475):**
```cpp
reg.registerCommand({"TR01_RMEM_READ", "Read R-Memory slot", cmdTr01RmemRead, "tr01", false});
```

**Handler implementation (lines 992-1014):**
```cpp
static void cmdTr01RmemRead(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::RMEM_SLOT_COUNT, "R-Memory slot");
    if (!result.valid) {
        Console::printf("Usage: TR01_RMEM_READ <slot>\r\n");
        return;
    }

    auto* se = getSecureElementWithCheck();
    if (!se) return;

    uint16_t slot = static_cast<uint16_t>(result.value);
    uint8_t data[256];
    uint16_t actualLen = 0;

    hal::SeResult seResult = se->rmemRead(slot, data, sizeof(data), &actualLen);
    if (seResult != hal::SeResult::OK) {
        Console::printf("ERROR: Read failed (slot may be empty)\r\n");
        return;
    }

    Console::printf("R-Memory Slot %d (%d bytes):\r\n", slot, actualLen);
    printHexDump(data, actualLen, actualLen);
}
```

The command:
1. Uses `requiresAuth = false` - no authentication required
2. Accepts any slot number (0-511)
3. Does not check slot ownership against the current module/user

## Impact
An attacker can:
1. Read TOTP account data (slots 32-131)
2. Read password vault entries (slots 132-511)
3. Read FIDO2 metadata (slots 132-158)
4. Read PIN storage (slot 0)

This is an Insecure Direct Object Reference (IDOR) vulnerability where slot numbers are predictable and no ownership validation occurs.

## Recommended Fix
Add ownership validation based on the slot map:

```cpp
static void cmdTr01RmemRead(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::RMEM_SLOT_COUNT, "R-Memory slot");
    if (!result.valid) {
        Console::printf("Usage: TR01_RMEM_READ <slot>\r\n");
        return;
    }

    auto* se = getSecureElementWithCheck();
    if (!se) return;

    uint16_t slot = static_cast<uint16_t>(result.value);
    
    // Check slot ownership - require authenticated session
    // and verify slot belongs to an allowed module
    auto& slotMap = core::TropicSlotMap::instance();
    bool found = false;
    uint8_t targetModuleId = 0;
    
    // Find which module owns this slot
    for (uint8_t moduleId = 1; moduleId < 255; moduleId++) {
        core::TropicSlotMap::SlotRange range;
        if (slotMap.getRangeByModuleId(moduleId, core::TropicSlotMap::SlotType::RMEM, &range)) {
            if (slot >= range.start && slot <= range.end) {
                targetModuleId = moduleId;
                found = true;
                break;
            }
        }
    }
    
    // Slot 0 (PINs) requires special handling
    if (slot == 0) {
        // Require PIN verification for slot 0
        auto& pm = core::PinManager::instance();
        char pin[16];
        Console::printf("Enter Badge PIN: ");
        if (!pm.verifyBadgePin(pin)) {
            Console::printf("ERROR: PIN verification failed\r\n");
            return;
        }
    } else if (!found) {
        Console::printf("ERROR: Slot not assigned to any module\r\n");
        return;
    }

    uint8_t data[256];
    uint16_t actualLen = 0;
    hal::SeResult seResult = se->rmemRead(slot, data, sizeof(data), &actualLen);
    // ... rest of handler ...
}
```

Alternatively, restrict the command to only show slot metadata (names) without raw data, and use module-specific commands for actual data access.

## References
- CWE-639: Insecure Direct Object Reference
- CWE-287: Improper Authentication
- OWASP API Security Top 10 - IDOR
