---
title: "[LOW] Secure Element Chip ID Exposed via Serial Command"
severity: LOW
domain: gdpr-dsgvo
lens: compliance/gdpr-dsgvo
labels:
  - "audit:compliance/gdpr-dsgvo"
---

## Summary
The `TR01_INFO` serial command prints the TROPIC01 secure element's unique Chip ID to the serial console. This provides a device-specific identifier that could be used to track individual devices.

**File**: `components/serial_cmd/src/SerialCmd.cpp`

## Impact
**Device Tracking**: The Chip ID is a unique identifier for each TROPIC01 secure element. If logged or captured, it could be used to track specific devices across different contexts.

**Device Fingerprinting**: Combined with other identifiers (BLE address, MAC address), the Chip ID enables more precise device fingerprinting.

**Low Severity**: The Chip ID is not directly tied to user personal data (names, emails, passwords), but it is a device identifier that could be useful for creating a device inventory or tracking.

**Comparison**: This is different from finding #8 (FIDO2 shared secret) and finding #10 (passwords) - this is a device identifier, not user data.

## Evidence

### Serial Command Implementation
```cpp
// components/serial_cmd/src/SerialCmd.cpp:921-934
uint8_t chipId[8];
uint8_t riscvVer = 0, spectVer = 0;

Console::printf("TR01 Info:\r\n");

if (se->getChipId(chipId, sizeof(chipId))) {
    Console::printf("  Chip ID: ");
    for (int i = 0; i < 8; i++) {
        Console::printf("%02X", chipId[i]);  // <-- Chip ID printed!
    }
    Console::printf("\r\n");
} else {
    Console::printf("  Chip ID: (read failed)\r\n");
}
```

### Command Registration
```cpp
// Likely in SerialCmd.cpp command registration
reg.registerCommand({"TR01_INFO", "Show TROPIC01 info", cmd_tr01_info, "tropic", false});
```

### Chip ID Retrieval
```cpp
// components/cdc_hal/src/Tropic01Element.cpp:815-840
bool Tropic01Element::getChipId(uint8_t* serialNum, uint8_t size) {
    if (!ensureSession("getChipId")) {
        return false;
    }
    
    struct lt_chip_id_t chipId;
    memset(&chipId, 0, sizeof(chipId));
    
    lt_ret_t ret = lt_get_info_chip_id(&handle_, &chipId);
    if (ret != LT_RET_OK) {
        return false;
    }
    
    // Copy chip ID to output buffer
    memcpy(serialNum, chipId.chip_id, size > chipId.chip_id_len ? chipId.chip_id_len : size);
    return true;
}
```

### Chip ID Definition
```cpp
// components/cdc_hal/include/cdc_hal/ISecureElement.h:199
virtual bool getChipId(uint8_t* serialNum, uint8_t size) = 0;
```

## Recommended Fix

### Option 1: Mask Chip ID Output (~1 hour)
Show only partial Chip ID for identification without full exposure:

```cpp
// components/serial_cmd/src/SerialCmd.cpp
if (se->getChipId(chipId, sizeof(chipId))) {
    Console::printf("  Chip ID: ");
    for (int i = 0; i < 3 && i < 8; i++) {  // Show first 3 bytes
        Console::printf("%02X", chipId[i]);
    }
    Console::printf("... (8 bytes total)\r\n");
} else {
    Console::printf("  Chip ID: (read failed)\r\n");
}
```

### Option 2: Add Verbose Flag for Full Chip ID (~1 hour)
Require explicit flag to show full Chip ID:

```cpp
static void cmd_tr01_info(const char* args) {
    bool verbose = strstr(args, "--verbose") != nullptr;
    
    // ... existing code ...
    
    if (se->getChipId(chipId, sizeof(chipId))) {
        if (verbose) {
            Console::printf("  Chip ID: ");
            for (int i = 0; i < 8; i++) {
                Console::printf("%02X", chipId[i]);
            }
            Console::printf("\r\n");
        } else {
            Console::printf("  Chip ID: [hidden, use --verbose]\r\n");
        }
    }
}
```

### Option 3: Show Chip ID Only with PIN Verification (~1 hour)
Require re-authentication to view full Chip ID:

```cpp
static void cmd_tr01_info(const char* args) {
    // Check if PIN verified session exists
    if (!isPinVerified()) {
        Console::printf("  Chip ID: [verify PIN first]\r\n");
        return;
    }
    
    // ... show full Chip ID ...
}
```

### Option 4: Remove Chip ID from Info Command (~1 hour)
Simply remove the Chip ID from the TR01_INFO output:

```cpp
static void cmd_tr01_info(const char* args) {
    uint8_t riscvVer = 0, spectVer = 0;
    
    Console::printf("TR01 Info:\r\n");
    
    // Skip Chip ID entirely
    // if (se->getChipId...) removed
    
    if (se->getFwVersion(&riscvVer, &spectVer)) {
        Console::printf("  Firmware: v%d.%d\r\n", riscvVer, spectVer);
    }
    // ... rest ...
}
```

### References
- **GDPR Art. 4(7)**: "Device identifier" can be considered personal data if linked to a user
- **Best Practice**: Device identifiers should be exposed only when necessary
- **Security**: Unique device identifiers can be used for tracking and fingerprinting
