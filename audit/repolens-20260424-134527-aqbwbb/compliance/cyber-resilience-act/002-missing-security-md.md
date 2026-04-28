---
title: "[HIGH] Missing SECURITY.md and vulnerability disclosure process"
severity: HIGH
domain: cyber-resilience-act
lens: vulnerability-disclosure
labels:
  - "vulnerability-disclosure"
  - "security-process"
  - "cra-2026"
---

## Summary
The repository lacks a `SECURITY.md` file defining a vulnerability disclosure process. There is also no `.well-known/security.txt` file. The README references `SECURITY.md` in the early alpha notice but the file does not exist.

**Files affected:**
- `SECURITY.md` - Missing (referenced in README.md but not created)
- `.github/` - No `SECURITY.md` or `PULL_REQUEST_TEMPLATE.md` with security focus
- `.well-known/security.txt` - Missing

## Impact
Under CRA (Article 11 and Annex I), manufacturers must have a clear process for reporting vulnerabilities:
- **Users cannot report security issues**: No defined channel for security researchers or users to report vulnerabilities
- **Delayed patching**: Without a clear process, vulnerabilities may be reported via public issue tracker, exposing users before fixes are ready
- **Compliance gap**: CRA requires documented vulnerability handling process

**Evidence from README.md:**
```markdown
> **Early Alpha** - This firmware is in active development and **not production ready**. Security hardening is incomplete. Do not use for protecting critical accounts. See [SECURITY.md](SECURITY.md) for hardening steps required before production use.
```
The file `SECURITY.md` is referenced but does not exist.

## Recommended Fix
Create a `SECURITY.md` file with:

1. **Vulnerability reporting process**:
```markdown
# Security

## Reporting a Vulnerability

We take security seriously. If you find a security vulnerability, please report it **privately** before publishing it publicly.

### How to Report

- **Email**: security@your-domain.com (or create a GitHub security advisory)
- **GitHub**: Use [GitHub Security Advisories](https://docs.github.com/en/code-security/security-advisories)
- Include: Description, steps to reproduce, affected versions

### Response Timeline

- **Acknowledgment**: Within 7 days
- **Initial Assessment**: Within 14 days
- **Patch Target**: Critical within 30 days, High within 90 days

## Security Features

See [README.md](README.md#security-architecture) for current security implementation details.

## Production Readiness Checklist

- [ ] Change default PINs
- [ ] Set DEBUG_MODE=0
- [ ] Enable FEATURE_SECURE_SERIAL
- [ ] Verify TROPIC01 secure element initialization
```

2. **Create `.well-known/security.txt`** (optional but recommended):
```
Contact: mailto:security@your-domain.com
Expires: 2026-12-31T23:59:59.000Z
Preferred-Languages: en, de
```

## References
- [EU CRA Article 11 - Vulnerability handling](https://digital-strategy.ec.europa.eu/en/library/cyber-resilience-act)
- [GitHub Security Advisories](https://docs.github.com/en/code-security/security-advisories)
- [security.txt specification](https://securitytxt.org/)
