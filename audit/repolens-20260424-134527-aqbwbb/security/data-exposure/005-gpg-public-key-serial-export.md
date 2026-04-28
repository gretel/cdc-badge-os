---
title: "[LOW] GPG public key exported to serial console without control"
severity: LOW
domain: serial-cmd
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The `GPG_EXPORT` serial command in `components/mod_gpg/src/GpgModule.cpp:193` and `showExport()` function at line 509 exports the full PEM-encoded public key to the serial console. While public keys are not secret, this exposes the key material to anyone with serial access without any confirmation or control.

## Impact
- **Key Material Exposure**: Public keys are sent to serial output, visible to anyone with USB-CDC access
- **QR Code Generation**: The key is also pushed to a QR code view (line 510), which could be photographed
- **Log Persistence**: Serial output is often captured in logs, making key material easily searchable

## Evidence
File: `components/mod_gpg/src/GpgModule.cpp:189-193`

```cpp
static void cmd_gpg_export(const char* args) {
    (void)args;
    char pem_buf[2048];
    size_t out_len = 0;
    if (!gpg_export_pubkey_pem(pem_buf, sizeof(pem_buf), &out_len)) {
        cdc::serial::Console::printf("ERROR\r\n");
        return;
    }
    cdc::serial::Console::printf("%s\r\n", pem_buf);  // Full PEM key output
}
```

Line 509 (UI export):
```cpp
cdc::serial::Console::printf("%s\r\n", pem_buf);  // Also logs to serial
s_qrView.init(mstr(STR_EXPORT_TITLE), nullptr, pem_buf);  // QR code
```

## Recommended Fix
1. **Add confirmation** before exporting to serial (e.g., "Export to serial? (Y/N)")
2. **Consider adding a flag** to control where the key is exported (serial vs. just QR)
3. **Add a timestamp** to the output for audit trail

Example fix:
```cpp
// Only export to serial if explicitly requested
if (args && args[0] == 'V') {  // VERBOSE flag
    cdc::serial::Console::printf("%s\r\n", pem_buf);
}
```

## References
- OpenPGP card specification
- General best practice: Public keys are not secret but should still have controlled export
