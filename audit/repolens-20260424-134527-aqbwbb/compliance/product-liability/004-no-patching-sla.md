---
title: "[MEDIUM] No documented vulnerability patching SLA"
severity: MEDIUM
domain: compliance/product-liability
lens: product-liability
labels:
  - patching
  - vulnerability-management
---

## Summary
The project has no documented Service Level Agreement (SLA) for vulnerability patching. Users cannot know how quickly they can expect security fixes after reporting a vulnerability.

**Evidence:**
- No `SECURITY.md` file (checked earlier)
- No mention of patching timelines in README or docs
- No reference to vulnerability response times in any documentation

## Impact
- Users cannot plan for vulnerability remediation
- Under Product Liability Directive, timely patching is expected for "state of the art" security
- Lack of commitment may delay critical patches
- Makes it harder for users to assess risk for production use

## Evidence
Search for SLA-related content:
```bash
grep -rn "SLA\|patch.*timeline\|vulnerability.*SLA\|security.*SLA\|response.*time" /input/20260423-132359-oj8ayc/cdc-badge-os --include="*.md"
# No results
```

README mentions "Early Alpha" and "not production ready" but provides no timeline or SLA for when production readiness will be achieved or how quickly vulnerabilities will be patched.

## Recommended Fix
Add a "Patching Policy" section to `SECURITY.md` (see Finding #001):

```markdown
## Patching Policy

| Severity | Response Time | Target Fix Time |
|----------|---------------|-----------------|
| Critical | 24 hours      | 7 days          |
| High     | 3 days        | 14 days         |
| Medium   | 7 days        | 30 days         |
| Low      | 14 days       | 60 days         |

Critical = Remote code execution, key extraction, PIN bypass
High = Authentication bypass, data exposure
Medium = Privilege escalation, DoS
Low = Information disclosure, minor security improvements
```

Also add to release notes:
```markdown
## Security Fixes in this Release
- CVE-XXXX-XXXXX: Description of fix
```

## References
- NIST Vulnerability Management: https://csrc.nist.gov/publications/detail/sp/800-40/rev-4/final
- OWASP Vulnerability Disclosure: https://cheatsheetseries.owasp.org/cheatsheets/Vulnerability_Disclosure_Cheat_Sheet.html
