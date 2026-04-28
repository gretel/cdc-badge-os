---
title: "[HIGH] Missing SECURITY.md with vulnerability reporting process"
severity: HIGH
domain: compliance/product-liability
lens: product-liability
labels:
  - security-documentation
  - vulnerability-reporting
---

## Summary
The README.md references a `SECURITY.md` file that does not exist in the repository. This is a critical gap for a hardware security key firmware that distributes pre-built binaries to users.

**Evidence:**
- `README.md:7` references `See [SECURITY.md](SECURITY.md) for hardening steps required before production use.`
- No `SECURITY.md` file exists in the repository root or docs directory

## Impact
Under the Product Liability Directive (2024), software distributors must provide clear channels for reporting defects and vulnerabilities. Without a documented security contact and reporting process:
- Users cannot reliably report security defects
- The developer may not receive timely vulnerability reports
- Creates compliance gap for product liability requirements
- Delays patching of critical vulnerabilities

## Evidence
```
README.md:7: > **Early Alpha** - This firmware is in active development and **not production ready**. Security hardening is incomplete. Do not use for protecting critical accounts. See [SECURITY.md](SECURITY.md) for hardening steps required before production use.
```

File search confirmed no SECURITY.md exists:
```bash
find /input/20260423-132359-oj8ayc/cdc-badge-os -name "SECURITY.md" -o -name "SECURITY.rst" 2>/dev/null
# No results
```

## Recommended Fix
Create a `SECURITY.md` file in the repository root containing:
1. **Security Contact**: Email or GitHub security advisory form link
2. **Vulnerability Reporting Process**: How to submit a report (preferably encrypted)
3. **Expected Response Time**: e.g., "Acknowledgment within 7 days"
4. **Patch Timeline**: General commitment (e.g., "Critical patches within 14 days")
5. **Hardening Checklist**: Steps mentioned in README for production readiness

Example structure:
```markdown
# Security

## Reporting a Vulnerability

To report a security vulnerability, please open a [GitHub Security Advisory](https://github.com/krim404/cdc-badge-os/security/advisories) or email ...

## Patching Policy

- Critical vulnerabilities: Patch within 14 days
- High vulnerabilities: Patch within 30 days
- Release notes will include security fixes

## Production Hardening Checklist

1. Set DEBUG_MODE=0
2. Change default PINs
3. ...
```

## References
- EU Product Liability Directive (2024/1968) - Article 4 (defect definition)
- GitHub Security Advisories: https://docs.github.com/en/code-security/security-advisories
- OWASP Vulnerability Disclosure: https://cheatsheetseries.owasp.org/cheatsheets/Vulnerability_Disclosure_Cheat_Sheet.html
