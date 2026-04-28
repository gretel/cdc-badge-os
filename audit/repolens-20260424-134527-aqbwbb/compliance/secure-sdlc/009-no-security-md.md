---
title: "[LOW] No SECURITY.md for vulnerability reporting"
severity: LOW
domain: secure-sdlc
lens: compliance
labels:
  - "documentation"
  - "security"
---

## Summary
The repository lacks a `SECURITY.md` file with instructions for reporting vulnerabilities. The README references `SECURITY.md` (line 7) but the file doesn't exist. This means security researchers may not know how to responsibly disclose vulnerabilities.

## Impact
- **Delayed Disclosure**: Researchers may publicly disclose without giving maintainers time to fix.
- **Confusion**: No clear process for reporting bugs.
- **Professional image**: Security-conscious projects should have a clear security policy.

## Evidence
File: `README.md:7`
```markdown
> **Early Alpha** - This firmware is in active development and **not production ready**. Security hardening is incomplete. Do not use for protecting critical accounts. See [SECURITY.md](SECURITY.md) for hardening steps required before production use.
```

File check: `SECURITY.md` does not exist in root directory.

## Recommended Fix
Create `SECURITY.md`:
```markdown
# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| Latest release | Yes |
| Previous release | Yes (critical fixes only) |
| Development branch | No |

## Reporting a Vulnerability

Please report security vulnerabilities via:
- Email: security@krim.dev (preferred)
- GitHub: Create an issue with `[SECURITY]` prefix

Include:
1. Description of the vulnerability
2. Steps to reproduce
3. Impact assessment
4. Suggested fix (optional)

We aim to respond within 7 days and provide a fix within 30 days for critical issues.

## Security Hardening Steps

Before production use, ensure:
1. `DEBUG_MODE=0` in `feature_flags.h`
2. `FEATURE_SECURE_SERIAL=1` for serial PIN protection
3. Default PIN changed from `123456`
4. Latest firmware version with all security patches

## Security Architecture

See `docs/THREAT_MODEL.md` for detailed threat analysis.
See `README.md#Security-Architecture` for key storage details.
```

Add to GitHub security settings:
- Settings → Security → Security policy → Link to SECURITY.md

## References
- GitHub Security.md: https://docs.github.com/en/code-security/getting-started/adding-a-security-policy-to-your-repository
- Responsible disclosure: https://www.first.org/global/sigs/vrdx
