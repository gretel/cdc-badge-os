---
title: "[MEDIUM] No CODEOWNERS file for automatic code review assignment"
severity: MEDIUM
domain: secure-sdlc
lens: compliance
labels:
  - "code-review"
  - "process"
---

## Summary
The repository lacks a `CODEOWNERS` file to automatically assign code reviewers based on changed files. This means pull requests may be reviewed by people unfamiliar with specific modules (FIDO2, TROPIC01, USB HID), reducing review effectiveness.

## Impact
- **Inconsistent Reviews**: Code may be reviewed by people without domain expertise.
- **Missed Security Issues**: Security-critical areas (PIN handling, crypto operations) may not get expert review.
- **Slower PR Workflow**: Manual assignment needed for each PR.

## Evidence
File check: No `CODEOWNERS` file found in:
- `.github/CODEOWNERS`
- `docs/CODEOWNERS`
- Root directory

Repository structure shows clear module boundaries that should map to owners:
- `components/mod_fido2/` → FIDO2 expert
- `components/mod_gpg/` → GPG expert
- `components/cdc_core/` → Core architecture expert
- `components/cdc_hal/` → Hardware abstraction expert

## Recommended Fix
Create `.github/CODEOWNERS`:
```
# Core architecture
/components/cdc_core/       @maintainer-core
/components/cdc_hal/        @maintainer-core

# Security modules
/components/mod_fido2/      @maintainer-fido2
/components/mod_gpg/        @maintainer-gpg
/components/mod_password/   @maintainer-vault

# UI
/components/cdc_ui/         @maintainer-ui
/components/cdc_views/      @maintainer-ui
/components/cdc_os_ui/      @maintainer-ui

# Hardware
/components/mod_sao/        @maintainer-hardware
/components/Adafruit-GFX/   @maintainer-hardware
/components/CalEPD/         @maintainer-hardware

# Documentation
/docs/                      @maintainer-docs

# CI/CD
.github/workflows/          @maintainer-devops
```

**Additional recommendations:**
1. Ensure at least 2 owners per area for redundancy
2. Use teams instead of individual usernames for long-term maintainability
3. Review CODEOWNERS quarterly to remove departed contributors

## References
- GitHub CODEOWNERS: https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/about-code-owners
- Best practices: https://github.blog/2017-07-27-code-owners/
