---
title: "[MEDIUM] No OpenAPI/Swagger specification found for API documentation and testing"
severity: MEDIUM
domain: api
lens: dast-api
labels:
  - "audit:toolgate/dast-api"
  - "setup"
---

## Summary
The repository `/input/20260423-132359-oj8ayc/cdc-badge-os` is an embedded firmware project (ESP32-S3) with no HTTP APIs or OpenAPI/Swagger specifications to fuzz.

## Impact
- **DAST API fuzzer cannot run**: Schemathesis requires an OpenAPI specification to generate test cases
- **No API contract testing**: Without a spec, API endpoints (if any exist) cannot be validated for schema conformance, status codes, or content types
- **Documentation gap**: API consumers lack machine-readable documentation

## Evidence
- **Repository type**: C/C++ embedded firmware (PlatformIO/ESP-IDF)
- **Files searched**: `openapi.json`, `openapi.yaml`, `swagger.json`, `swagger.yaml` - none found
- **HTTP references**: Only found in comments pointing to external resources (datasheets, GitHub issues)
- **API interfaces present**:
  - Serial commands via USB CDC (`components/serial_cmd/`)
  - USB HID for FIDO2 (`components/usb_badge/`)
  - BLE connectivity (`components/mod_ble_serial/`)
- **No HTTP server**: No embedded HTTP server found in source code

## Recommended Fix
1. **If this is correct** (firmware project without HTTP APIs):
   - Document that the DAST API fuzzer lens is not applicable to this repository
   - Mark as "out of scope" for API fuzzing

2. **If HTTP APIs should exist** (e.g., web-flasher backend, BLE-to-HTTP bridge):
   - Add OpenAPI specification file at `docs/api/openapi.json` or `api/openapi.yaml`
   - Define all endpoints with request/response schemas
   - Ensure spec is kept in sync with implementation

3. **Alternative**: If APIs are exposed via web-flasher or other services:
   - Provide hosted environment URL with `--hosted` flag
   - Point to service URL where OpenAPI spec can be discovered

## References
- [OpenAPI Specification](https://spec.openapis.org/oas/latest.html)
- [Schemathesis Documentation](https://schemathesis.readthedocs.io/)
- API-first design best practices
