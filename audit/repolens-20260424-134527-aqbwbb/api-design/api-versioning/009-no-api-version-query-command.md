---
title: "[LOW] No API Version Query Command for Serial Interface"
severity: LOW
domain: API Design
lens: api-versioning
labels:
  - "audit:api-design/api-versioning"
---

## Summary
The serial command interface has no `API_VERSION` or `VERSION` command to query the API version programmatically. `docs/SERIAL_COMMANDS.md` lists commands but none return version info.

**Evidence:**
- `docs/SERIAL_COMMANDS.md:16-27` - System commands include `PING`, `STATUS`, `MEM` but no `VERSION`
- `components/serial_cmd/src/CommandRegistry.cpp` - No version query handler
- `components/usb_badge/include/usb_badge/usb_cdc.h` - No `usb_cdc_get_version()` function

## Impact
- Clients cannot programmatically detect API version
- Scripts must guess which commands/features are available
- Hard to build compatible client libraries

## Evidence
Current system commands (from SERIAL_COMMANDS.md):
```
| Command | Description |
|---------|-------------|
| HELP | Show available commands |
| PING | Check if device is responsive |
| STATUS | Show system status |
| MEM | Show memory usage |
| ERROR_LOG | Show error log |
| REBOOT | Restart the device |
```

No version query command.

## Recommended Fix
Add version query command:

1. Add version command handler:
```cpp
// In SerialCmd.cpp
static void cmdApiVersion(const char* args) {
    (void)args;
    Console::printf("API_VERSION=1.0.0\r\n");
    Console::printf("SERIAL_CMD=v1.0.0\r\n");
    Console::printf("BLE_VCARD=v1.0.0\r\n");
    Console::printf("USB_HID=v1.0.0\r\n");
}
```

2. Register the command:
```cpp
// In module registration
{ "API_VERSION", "Get API version info", cmdApiVersion, "system", false },
```

3. Add to documentation:
```markdown
## System

| Command | Description |
|---------|-------------|
| API_VERSION | Get API version info |
```

4. Add USB version function:
```cpp
/**
 * Get USB configuration version
 * @return Version string (e.g., "1.0.0")
 */
const char* usb_cdc_get_version(void);
```

## References
- Version Query Pattern: https://restfulapi.net/versioning/
- API Discovery: https://www.ietf.org/rfc/rfc6749.txt
