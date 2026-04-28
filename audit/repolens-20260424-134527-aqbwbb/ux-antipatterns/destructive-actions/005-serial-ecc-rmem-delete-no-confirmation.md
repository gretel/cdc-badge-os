---
title: "[HIGH] Serial ECC/R-Memory Delete Commands Lack Confirmation"
severity: HIGH
domain: destructive-actions
lens: serial-commands
labels:
  - audit:ux-antipatterns/destructive-actions
---

## Summary
The serial command interface provides two low-level delete commands (`ECC_DEL` and `TR01_RMEM_DEL`) that execute immediate deletion without any confirmation step. These commands can delete individual ECC keys or R-Memory slots, potentially destroying module data structures.

**Evidence:**
- File: `components/serial_cmd/src/SerialCmd.cpp`
- Lines: 1020-1037 (ECC_DEL) and 1043-1060 (TR01_RMEM_DEL)

```cpp
// ECC_DEL - Line 1020-1037
static void cmdEccDel(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::ECC_SLOT_COUNT, "ECC slot");
    if (!result.valid) {
        Console::printf("Usage: ECC_DEL <slot>\r\n");
        return;
    }

    auto* se = getSecureElementWithCheck();
    if (!se) return;

    uint8_t slot = static_cast<uint8_t>(result.value);
    hal::SeResult seResult = se->eccDelete(slot);  // Immediate delete!
    if (seResult == hal::SeResult::OK) {
        Console::printf("OK: ECC slot %d deleted\r\n", slot);
    } else {
        Console::printf("ERROR: Delete failed\r\n");
    }
}

// TR01_RMEM_DEL - Line 1043-1060
static void cmdTr01RmemDel(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::RMEM_SLOT_COUNT, "R-Memory slot");
    if (!result.valid) {
        Console::printf("Usage: TR01_RMEM_DEL <slot>\r\n");
        return;
    }

    auto* se = getSecureElementWithCheck();
    if (!se) return;

    uint8_t slot = static_cast<uint8_t>(result.value);
    hal::SeResult seResult = se->rmemErase(slot);  // Immediate erase!
    if (seResult == hal::SeResult::OK) {
        Console::printf("OK: R-Memory slot %d erased\r\n", slot);
    } else {
        Console::printf("ERROR: Erase failed\r\n");
    }
}
```

Compare to the destructive `TR01_WIPE` command which requires explicit confirmation:
```cpp
// TR01_WIPE - Line 1132-1160
static void cmdTr01Wipe(const char* args) {
    if (!args || strcmp(args, "CONFIRM") != 0) {
        Console::printf("WARNING: This will ERASE ALL data on TROPIC01!\r\n");
        Console::printf("  - All ECC keys (slots 0-31)\r\n");
        Console::printf("  - All R-Memory data (slots 0-511)\r\n");
        Console::printf("\r\nTo proceed, type: TR01_WIPE CONFIRM\r\n");
        return;
    }
    // ... actual deletion code ...
}
```

## Impact
- **Data Loss Risk**: Individual ECC or R-Memory slots can be deleted with a single command
- **Module Corruption**: Deleting slots used by modules (GPG, FIDO2, TOTP, Password) can corrupt their data structures
- **Silent Destruction**: No warning about which module uses the slot or what data will be lost
- **Inconsistent UX**: `TR01_WIPE` (bulk delete) requires confirmation, but individual slot deletion does not

## Recommended Fix
Add confirmation steps to both `cmdEccDel()` and `cmdTr01RmemDel()` commands. Show the slot number and require explicit confirmation:

```cpp
static void cmdEccDel(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::ECC_SLOT_COUNT, "ECC slot");
    if (!result.valid) {
        Console::printf("Usage: ECC_DEL <slot> [CONFIRM]\r\n");
        return;
    }

    auto* se = getSecureElementWithCheck();
    if (!se) return;

    uint8_t slot = static_cast<uint8_t>(result.value);
    
    // Check for confirmation
    const char* confirm = args + strlen("ECC_DEL ");  // Skip command name
    confirm = skipSpaces(confirm);
    if (strcmp(confirm, "CONFIRM") != 0) {
        Console::printf("WARNING: Delete ECC slot %d?\r\n", slot);
        Console::printf("  This will erase the private key stored in this slot.\r\n");
        Console::printf("  To proceed, type: ECC_DEL %d CONFIRM\r\n", slot);
        return;
    }

    hal::SeResult seResult = se->eccDelete(slot);
    if (seResult == hal::SeResult::OK) {
        Console::printf("OK: ECC slot %d deleted\r\n", slot);
    } else {
        Console::printf("ERROR: Delete failed\r\n");
    }
}

static void cmdTr01RmemDel(const char* args) {
    auto result = parseSlotArg(args, hal::ISecureElement::RMEM_SLOT_COUNT, "R-Memory slot");
    if (!result.valid) {
        Console::printf("Usage: TR01_RMEM_DEL <slot> [CONFIRM]\r\n");
        return;
    }

    auto* se = getSecureElementWithCheck();
    if (!se) return;

    uint8_t slot = static_cast<uint8_t>(result.value);
    
    // Check for confirmation
    const char* confirm = args + strlen("TR01_RMEM_DEL ");
    confirm = skipSpaces(confirm);
    if (strcmp(confirm, "CONFIRM") != 0) {
        Console::printf("WARNING: Erase R-Memory slot %d?\r\n", slot);
        Console::printf("  This will erase all data in this slot.\r\n");
        Console::printf("  To proceed, type: TR01_RMEM_DEL %d CONFIRM\r\n", slot);
        return;
    }

    hal::SeResult seResult = se->rmemErase(slot);
    if (seResult == hal::SeResult::OK) {
        Console::printf("OK: R-Memory slot %d erased\r\n", slot);
    } else {
        Console::printf("ERROR: Erase failed\r\n");
    }
}
```

## References
- Similar confirmation pattern: `TR01_WIPE` in `serial_cmd/src/SerialCmd.cpp:1132`
- Slot allocation documentation: See `docs/SLOT_ALLOCATION.md` (if exists) or module headers
