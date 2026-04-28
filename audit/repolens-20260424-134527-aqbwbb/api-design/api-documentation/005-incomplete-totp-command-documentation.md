---
title: "[LOW] TOTP_ADD command parameters not fully documented"
severity: LOW
domain: api-design/api-documentation
lens: serial-command-api
labels:
  - "audit:api-design/api-documentation"
---

## Summary
The `docs/SERIAL_COMMANDS.md` documents the TOTP_ADD command but does not specify:
1. The exact format for the `secret` parameter (Base32 encoded)
2. Valid values for `digits` parameter (6, 7, or 8)
3. Valid values for `period` parameter (typically 30 seconds)
4. What happens with invalid parameter values

## Impact
- Users may try to paste raw secrets instead of Base32-encoded ones
- Invalid digits/period values may fail silently or produce unexpected results
- Difficult to integrate with existing TOTP providers without knowing exact format

## Evidence
**Current documentation (docs/SERIAL_COMMANDS.md:91-99):**
```markdown
**TOTP_ADD Parameters:**
- `name` - Account name (required)
- `secret` - Base32 encoded secret (required)
- `issuer` - Issuer name (optional)
- `digits` - Code length: 6, 7, or 8 (default: 6)
- `period` - Time period in seconds (default: 30)
```

**Implementation (components/mod_totp/src/TotpModule.cpp):**
The code validates digits and period but the validation rules are not documented:
- digits: Must be 6, 7, or 8
- period: Typically 30 seconds (standard), but other values may work

**Examples show correct usage but don't explain format:**
```bash
echo "TOTP_ADD GitHub JBSWY3DPEHPK3PXP" > /dev/ttyACM0
echo "TOTP_ADD AWS HXDMVJECJJWSRB3HWIZR4IFUGFTMXBOZ Amazon 6 30" > /dev/ttyACM0
```

## Recommended Fix
Enhance the TOTP_ADD documentation:

```markdown
**TOTP_ADD Parameters:**
- `name` - Account name (required, no spaces or use quotes)
- `secret` - Base32 encoded secret (required)
  - Get from TOTP provider (Google Authenticator, etc.)
  - Example: `JBSWY3DPEHPK3PXP`
  - Must be valid Base32 (A-Z, 2-7, optionally with `=` padding)
- `issuer` - Issuer name (optional, e.g., "Google", "GitHub")
- `digits` - Code length: 6, 7, or 8 (default: 6)
  - Most services use 6 digits
  - Check your service's requirements
- `period` - Time period in seconds (default: 30)
  - Standard TOTP uses 30 seconds
  - Some services use 60 seconds

**Getting the secret:**
1. Scan QR code with computer instead of phone
2. Extract secret from QR URL (otpauth://totp/...&secret=...)
3. Or manually enter secret into TOTP app to get Base32 value
```

## References
- docs/SERIAL_COMMANDS.md:78-99 (TOTP documentation)
- components/mod_totp/src/TotpModule.cpp (implementation)
