# E2E Test Gaps Audit Summary

**Audited Repository:** `library/cdc-badge-os`
**Domain:** E2E Test Gaps
**Date:** 2026-04-25

## Overview

This audit identified **8 E2E test gap findings** in the CDC Badge OS firmware, a hardware security key with FIDO2/WebAuthn, TOTP, Password Vault, and GPG features.

## Key Findings

| # | Severity | Finding | Impact |
|---|----------|---------|--------|
| 001 | HIGH | No E2E tests for critical user flows (FIDO2, TOTP, Passwords, GPG) | Core features untested end-to-end |
| 002 | HIGH | No E2E tests for PIN authentication and lockout flows | Brute-force protection unverified |
| 003 | CRITICAL | No E2E tests for FIDO2/WebAuthn registration and authentication | Primary feature untested |
| 004 | MEDIUM | No E2E tests for multi-step wizard flows (TOTP, Passwords) | Data integrity at risk |
| 005 | MEDIUM | No E2E tests for GPG/CCID smartcard workflow | Key generation unverified |
| 006 | MEDIUM | No E2E tests for BLE vCard exchange flow | Badge-to-badge feature untested |
| 007 | MEDIUM | No E2E tests for serial command interface | Configuration interface untested |
| 008 | LOW | No E2E tests for power management and sleep flows | Battery/sleep behavior unverified |

## Current Test Coverage

**Existing tests:** 3 smoke tests in `test/` directory
- `test_vcard_store.cpp` - 21 lines (vCard parsing smoke test)
- `test_ble_vcard_symbols.cpp` - 19 lines (BLE symbol linking)
- `test_vcard_module_link.cpp` - 19 lines (module registration)

**Test framework:** Unity test framework available but not used for E2E

**CI/CD:** Build workflow (`.github/workflows/build.yml`) only builds firmware, no test execution

## Critical Gaps

### 1. Authentication Flows
- Badge PIN entry and verification
- FIDO2 PIN (Client PIN protocol)
- OpenPGP PW1/PW3 PINs
- 3-attempt lockout with 60-second timer

### 2. Core Feature Flows
- FIDO2 credential registration
- FIDO2 authentication assertion
- TOTP account creation wizard
- Password entry CRUD operations
- GPG key generation

### 3. Multi-Step Workflows
- T9 input wizard navigation
- Back-navigation state persistence
- Data validation at each step

### 4. Integration Flows
- BLE vCard exchange
- Serial command interface
- USB HID communication

## Recommended Priority

1. **CRITICAL:** FIDO2 registration/authentication (primary feature)
2. **HIGH:** PIN lockout logic (security-critical)
3. **HIGH:** Complete user flow coverage
4. **MEDIUM:** Wizard flows (TOTP, Passwords)
5. **MEDIUM:** GPG/CCID workflow
6. **MEDIUM:** BLE vCard exchange
7. **MEDIUM:** Serial commands
8. **LOW:** Power management

## Files Created

```
000-SUMMARY.md           # This summary
001-critical-user-flows.md
002-authentication-flows.md
003-fido2-critical-flow.md
004-multi-step-wizard-flows.md
005-gpg-ccid-workflow.md
006-ble-vcard-flow.md
007-serial-command-flow.md
008-power-sleep-flow.md
```

## Next Steps

1. Create `test/e2e/` directory structure
2. Add test runner configuration
3. Implement tests starting with highest priority
4. Integrate E2E tests into CI/CD workflow
5. Document test setup in `docs/TESTING.md`
