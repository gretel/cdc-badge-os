---
title: "[MEDIUM] Missing centralized error code documentation"
severity: MEDIUM
domain: Error Traceability
lens: error-traceability
labels:
  - "audit:maintainability/error-traceability"
---

## Summary
There is no centralized documentation for error codes, their meanings, and recommended remediation. Error codes are scattered across different modules without a unified reference.

## Impact
- **On-call difficulty**: Engineers cannot quickly look up error meanings
- **Consumer confusion**: API consumers cannot programmatically handle errors
- **Knowledge silos**: Error knowledge is only in code, not accessible to non-developers
- **Inconsistent handling**: Different modules may handle similar errors differently

## Evidence

### Error codes exist but are undocumented:

**FIDO2/CTAP2 error codes** (well-defined but not documented externally):
`components/mod_fido2/include/mod_fido2/ctap2.h`:
```cpp
#define CTAP1_ERR_INVALID_COMMAND       0x01
#define CTAP1_ERR_INVALID_PARAMETER     0x02
#define CTAP2_ERR_CBOR_UNEXPECTED_TYPE  0x11
#define CTAP2_ERR_PIN_BLOCKED           0x32
// ... 30+ error codes
```

**CTAPHID error codes** (protocol-level):
`components/mod_fido2/include/mod_fido2/ctaphid.h`:
```cpp
#define CTAPHID_ERR_INVALID_CMD     0x01
#define CTAPHID_ERR_INVALID_PAR     0x02
#define CTAPHID_ERR_MSG_TIMEOUT     0x05
// ... 9 error codes
```

**No error documentation files found:**
- No `ERRORS.md` or `error_codes.md` in `/docs/`
- No error catalog in `docs/` directory
- Error codes only exist in header files

**Documentation exists but lacks error info:**
- `docs/GPG.md` - No error codes or troubleshooting
- `docs/SERIAL_COMMANDS.md` - No error response documentation
- `docs/UI_FLOWS.md` - No error state documentation

## Recommended Fix

1. **Create centralized error documentation:**

   Create `docs/ERROR_CODES.md` with structure:
   ```markdown
   # Error Code Reference
   
   ## FIDO2/CTAP2 Errors
   
   | Code | Name | Meaning | Retryable | Remediation |
   |------|------|---------|-----------|-------------|
   | 0x01 | CTAP1_ERR_INVALID_COMMAND | Unknown command byte | No | Check command format |
   | 0x32 | CTAP2_ERR_PIN_BLOCKED | PIN attempts exceeded | No | Reset badge |
   
   ## Module Errors
   
   | Module | Code | Meaning | Severity |
   |--------|------|---------|----------|
   | GPG | 1001 | Slot allocation failed | Critical |
   | TOTP | 2001 | Time sync required | Warning |
   ```

2. **Add error codes to existing docs:**
   - `docs/SERIAL_COMMANDS.md` - Add error response section
   - `docs/GPG.md` - Add troubleshooting section
   - `docs/UI_FLOWS.md` - Add error state descriptions

3. **Generate documentation from code:**
   ```cpp
   // Add doxygen comments to error constants
   /**
    * \brief PIN blocked error
    * \details User has exceeded maximum PIN attempts
    * \sa CTAP2_ERR_PIN_INVALID
    */
   #define CTAP2_ERR_PIN_BLOCKED 0x32
   ```

4. **Create error lookup utility:**
   ```cpp
   // components/cdc_core/ErrorCodes.h
   const char* getErrorName(uint16_t code);
   const char* getErrorDescription(uint16_t code);
   bool isRetryable(uint16_t code);
   ```

## References
- [CTAP2 specification](https://fidoalliance.org/specs/fido-v2.0-rd-20180130/fido-client-to-authenticator-protocol-v2.0-rd-20180130.html#authenticator-errno) - Official error code definitions
- Existing error codes in `components/mod_fido2/include/mod_fido2/`
