---
title: "[MEDIUM] No OpenAPI Specification Available for Schemathesis Fuzzing"
severity: MEDIUM
domain: ToolGate
labels:
  - audit:toolgate/session-schemathesis
---

## Summary

The **CDC Badge OS** repository is an embedded firmware project (ESP32-S3) that does not expose an HTTP/REST API. Schemathesis requires an OpenAPI specification to perform fuzz testing, but no such specification exists in this codebase.

**Evidence:**
- Project type: Embedded firmware using PlatformIO + ESP-IDF framework
- Communication interfaces: USB CDC/HID (serial), BLE (planned), not HTTP
- No files matching `openapi.json`, `openapi.yaml`, `swagger.json`, or `swagger.yaml` found
- No HTTP server components detected (`esp_http_server`, `REST`, etc.)
- Serial command interface exists (`components/serial_cmd/`) but is not HTTP-based

## Impact

Schemathesis cannot be applied to this codebase in its current form. The tool is designed for HTTP REST APIs with OpenAPI specifications, not embedded firmware.

## Evidence

**Project Structure:**
```
/input/20260423-132359-oj8ayc/cdc-badge-os/
├── platformio.ini          # ESP32 build configuration
├── components/
│   ├── cdc_core/           # Core services
│   ├── usb_badge/          # USB CDC/HID
│   ├── serial_cmd/         # Serial commands (not HTTP)
│   └── ...
└── main/                   # Firmware entry point
```

**Communication Interfaces:**
- USB CDC (serial console at 115200 baud)
- USB HID (FIDO2, U2F)
- USB CCID (GPG/Smartcard)
- BLE (planned)

**Search Results:**
- `find ... -name "*openapi*"` → No results
- `find ... -name "*swagger*"` → No results
- `grep -r "esp_http"` → No results

## Recommended Fix

This is a **domain mismatch** rather than a bug. Schemathesis is not the appropriate tool for this codebase.

**Alternative testing approaches for embedded firmware:**
1. **Unit tests** - Already present in `test/` directory
2. **Property-based testing** - Use Hypothesis directly on C/C++ code
3. **Fuzzing** - Use `libFuzzer` or `AFL++` for C functions (see `third_party/libtropic/vendor/trezor_crypto/fuzzer/`)
4. **Hardware-in-the-loop testing** - Physical badge testing with scripted USB/serial inputs

**If you want to use Schemathesis:**
You would need a companion web service (e.g., a dashboard or cloud backend) that communicates with the badge. That service would need to:
1. Expose an HTTP/REST API
2. Provide an OpenAPI specification
3. Be reachable for testing

## References

- [Schemathesis documentation](https://schemathesis.readthedocs.io/) - Requires OpenAPI 3.0+
- [CDC Badge OS README](/input/20260423-132359-oj8ayc/cdc-badge-os/README.md) - Project overview
- [Embedded firmware testing best practices](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/unit-testing.html)
