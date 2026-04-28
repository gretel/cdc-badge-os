---
title: "[INFO] Codebase has minimal REST API - primarily serial commands and binary protocols"
severity: INFO
domain: api-design
lens: rest-conventions
labels:
  - "audit:api-design/rest-conventions"
---

## Summary
The CDC Badge OS codebase is an embedded firmware project with **no traditional REST API**. The communication interfaces are:

1. **Serial Commands** (USB CDC at 115200 baud) - Text-based command interface
2. **CTAP2/FIDO2** - Binary protocol over USB HID and BLE
3. **BLE UART** - Nordic UART Service (GATT-based)
4. **GPG/CCID** - Smart card protocol (APDU-based)
5. **Web Flasher** - Single GitHub API call for version info

## Impact
- **REST conventions audit scope is limited** to:
  - Web flasher's GitHub API consumption (1 endpoint)
  - Future web interfaces (manifest.json structure)
- Most "API" conventions apply to:
  - Serial command naming (documented in `docs/SERIAL_COMMANDS.md`)
  - CTAP2 command codes (documented in `components/mod_fido2/include/mod_fido2/ctap2.h`)
  - GATT service/characteristic UUIDs (documented in `components/mod_ble_serial/include/mod_ble_serial/BleUartService.h`)

## Evidence
REST-like interface locations:

| Interface | Location | Protocol |
|-----------|----------|----------|
| Serial Commands | `components/serial_cmd/` | Text-based CLI |
| CTAP2 (FIDO2) | `components/mod_fido2/` | CBOR binary |
| BLE UART | `components/mod_ble_serial/` | GATT |
| GPG CCID | `components/mod_gpg/` | APDU |
| Web Flasher API | `web-flasher/index.html:285-298` | REST (GitHub) |

## Recommended Actions
1. **Serial Command Naming** - Consider creating a style guide for command naming consistency (currently uses UPPER_SNAKE_CASE like `TOTP_LIST`, `PASSWORD_ADD`)
2. **Future Web API** - If a web management interface is added, establish REST conventions upfront (see `docs/SERIAL_COMMANDS.md` for existing command patterns)
3. **API Documentation** - Consider OpenAPI/Swagger if HTTP API is added later

## References
- [Serial Commands Reference](docs/SERIAL_COMMANDS.md)
- [CTAP2 Specification](https://fidoalliance.org/specs/fido2/)
- [Nordic UART Service](https://devzone.nordicsemi.com/guides/short-range-guides/bt-le-nus/)
