---
title: "[LOW] Missing SECURITY.md with security considerations"
severity: LOW
domain: developer-onboarding
lens: onboarding-docs
labels:
  - "audit:documentation/onboarding-docs"
---

## Summary
The README references `SECURITY.md` but the file doesn't exist at the expected location.

**Evidence:**
- `README.md:7`: "See [SECURITY.md](SECURITY.md) for hardening steps required before production use."
- `README.md:248`: "While I'm experienced with cryptography and encryption concepts, this is my first project implemented directly on the ESP32."
- No `SECURITY.md` file in root directory

## Impact
New developers:
- Expect security guidance but find no file
- May not understand what security hardening is needed
- Don't know what "not production ready" means in practice
- May use the firmware for important accounts without understanding risks

## Evidence
1. `README.md:7`:
   ```markdown
   > **Early Alpha** - This firmware is still in active development and **not production ready**.
   > Security hardening is incomplete. Do not use for protecting critical accounts.
   > See [SECURITY.md](SECURITY.md) for hardening steps required before production use.
   ```

2. File doesn't exist:
   ```bash
   $ ls -la SECURITY.md
   ls: cannot access 'SECURITY.md': No such file or directory
   ```

3. README admits limitations but doesn't document them:
   - "Do not use it as-is for production or security-critical deployments"
   - "You may still find non-idiomatic ESP32 code, suboptimal design patterns, duplication, or refactoring debt"

## Recommended Fix
Create `SECURITY.md` at repository root with:

1. **Current Status**:
   - "Early Alpha" - development phase
   - Not production ready
   - Security hardening incomplete

2. **Known Security Considerations**:
   - DEBUG_MODE disables PIN lockouts
   - Feature flags that affect security
   - Known attack surfaces

3. **Hardening Steps** (from README):
   - Set `DEBUG_MODE=0` for production
   - Enable `FEATURE_SECURE_SERIAL=1`
   - Change default PIN immediately
   - Time synchronization requirements

4. **Security Architecture**:
   - Key storage in TROPIC01
   - PIN protection (3 attempts, 60s lockout)
   - FIDO2 ClientPIN implementation

5. **What to Test Before Production**:
   - PIN lockout behavior
   - FIDO2 credential generation
   - Secure serial authentication
   - Sleep/wake power states

6. **Reporting Security Issues**:
   - How to report vulnerabilities
   - Expected response time

**Estimated effort:** ~30-45 minutes to compile from README and code.

## References
- [FIDO2 Security Considerations](https://fidoalliance.org/specs/fido2/fido2-v2.1-ps-20210518.html#security-considerations)
