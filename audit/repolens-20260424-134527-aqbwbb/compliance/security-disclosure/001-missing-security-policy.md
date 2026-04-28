---
title: "[HIGH] Missing SECURITY.md with vulnerability disclosure process"
severity: HIGH
domain: compliance
lens: security-disclosure
labels:
  - "missing-security-policy"
---

## Summary
The repository lacks a `SECURITY.md` file in the root directory and no `.well-known/security.txt` file. The README.md references `SECURITY.md` on line 7 but the file does not exist. There is no documented process for reporting security vulnerabilities, no security contact email, and no coordinated vulnerability disclosure (CVD) process.

**Files Affected:**
- Repository root: No `SECURITY.md` file
- `.well-known/`: No `security.txt` file (RFC 9116)
- `README.md:7`: References non-existent `SECURITY.md`

## Impact
Without a vulnerability disclosure policy:
1. **Security researchers** have no clear path to report vulnerabilities
2. **Users** don't know how to report bugs that might be security-related
3. **Public exposure risk** - Security issues may be reported via public GitHub issues, exposing vulnerabilities before patches are ready
4. **Cyber Resilience Act (CRA)** compliance is incomplete for products with digital elements
5. **Slower patch cycles** - No documented timeline for acknowledgment and fix release

## Evidence
```bash
# No SECURITY.md in root
$ ls -la SECURITY.md
ls: cannot access 'SECURITY.md': No such file or directory

# No .well-known directory
$ find . -path '*well-known/security.txt'
(no results)

# README.md references non-existent file on line 7
> > **Early Alpha** - This firmware is in active development and **not production ready**. Security hardening is incomplete. Do not use for protecting critical accounts. See [SECURITY.md](SECURITY.md) for hardening steps required before production use.
```

## Recommended Fix
Create a `SECURITY.md` file in the repository root with the following sections:

1. **Security Contact** - Email address for security reports (e.g., `security@domain.com` or GitHub security contact)
2. **Reporting Instructions** - Clear steps on how to report a vulnerability
3. **What to Include** - Template for vulnerability reports (description, reproduction steps, impact)
4. **Response Timeline** - Expected acknowledgment time (e.g., "within 48 hours")
5. **Scope** - What components/modules are in scope for vulnerability reporting
6. **Safe Harbor** - Statement that researchers won't be penalized for responsible disclosure
7. **Credit/Attribution** - Policy for acknowledging reporters

Example structure:
```markdown
# Security Policy

## Reporting a Vulnerability

We welcome vulnerability reports from the community. To report a security issue:

1. Email `security@domain.com` with details
2. Include description, reproduction steps, and impact assessment
3. Await acknowledgment within 48 hours

## What to Include

- Description of the vulnerability
- Steps to reproduce
- Expected vs actual behavior
- Affected versions
- Optional: Proof-of-concept code

## Response Timeline

- **Acknowledgment**: Within 48 hours
- **Status update**: Within 5 business days
- **Patch release**: As soon as feasible

## Scope

All components of CDC Badge OS firmware are in scope, including:
- FIDO2/WebAuthn module
- TOTP module
- Password vault
- GPG/CCID module
- Secure element (TROPIC01) integration

## Safe Harbor

Researchers acting in good faith to find and report vulnerabilities will not face retaliation.

## Credit

Reporters will be credited in release notes (unless anonymity is requested).
```

Additionally, consider:
- Creating `.github/SECURITY.md` for GitHub Security Advisories support
- Adding a `security.txt` file in `.well-known/` directory following RFC 9116

## References
- [GitHub Security Advisories](https://docs.github.com/en/code-security/security-advisories)
- [RFC 9116 - security.txt](https://www.rfc-editor.org/rfc/rfc9116.html)
- [Cyber Resilience Act (CRA)](https://www.eu-cra.eu/)
- [OWASP Vulnerability Disclosure](https://owasp.org/www-community/Vulnerability_Disclosure_Policy)
