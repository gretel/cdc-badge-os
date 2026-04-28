---
title: "[MEDIUM] Missing Coordinated Vulnerability Disclosure (CVD) process documentation"
severity: MEDIUM
domain: compliance
lens: security-disclosure
labels:
  - "missing-cvd-process"
  - "cvss-missing"
  - "timeline-missing"
---

## Summary
The repository lacks documentation of a Coordinated Vulnerability Disclosure (CVD) process. While the README references a non-existent `SECURITY.md` file, there is no documented process for:
- Severity classification (CVSS or equivalent)
- Response timeline from report to disclosure
- Patch release process
- Advisory publication mechanism (GitHub Security Advisories, CVE)

**Files Affected:**
- Repository root: No CVD process documentation
- `docs/`: No security process documentation
- `.github/`: No `SECURITY.md` for GitHub Security Advisories

## Impact
Without a documented CVD process:
1. **Inconsistent handling** - Vulnerabilities may be handled ad-hoc without a standard process
2. **Delayed patching** - No defined timeline creates uncertainty for reporters and users
3. **No severity framework** - Lack of CVSS classification makes prioritization difficult
4. **CVE coordination missing** - No process for requesting CVEs for discovered vulnerabilities
5. **CRA compliance gap** - Cyber Resilience Act requires documented vulnerability handling process
6. **Researcher friction** - Security researchers expect clear timelines and processes

## Evidence
```bash
# No CVD documentation found
$ grep -rn 'CVSS\|timeline\|patch.*release\|advisory\|CVE' --include='*.md' docs/
(no results in main docs, only in third_party/vendor files)

# No GitHub Security Advisories setup
$ ls -la .github/SECURITY.md
ls: cannot access '.github/SECURITY.md': No such file or directory

# README line 7 references non-existent SECURITY.md
> See [SECURITY.md](SECURITY.md) for hardening steps required before production use.
```

The third-party libtropic vendor files contain CVE references (e.g., CVE-2016-0695, CVE-2016-3426) but these are for the vendor's bugs, not a process for the CDC Badge OS project itself.

## Recommended Fix
Create CVD process documentation with the following components:

### 1. Add GitHub Security Advisories Support
Create `.github/SECURITY.md`:
```markdown
# Security Advisories

We use GitHub Security Advisories for private vulnerability reporting and coordination.

## Private Reporting
1. Go to [Security Advisories](https://github.com/krim404/cdc-badge-os/advisories)
2. Click "New advisory"
3. Fill in vulnerability details
4. Submit for review
```

### 2. Define Severity Classification
Document the severity framework:
```markdown
## Severity Classification

We use CVSS v3.1 for severity classification:

| CVSS Score | Severity | Response Time |
|------------|----------|---------------|
| 9.0-10.0   | Critical | 7 days        |
| 7.0-8.9    | High     | 14 days       |
| 4.0-6.9    | Medium   | 30 days       |
| 0.1-3.9    | Low      | 90 days       |
```

### 3. Document Timeline and Process
```markdown
## Vulnerability Handling Timeline

1. **Acknowledgment**: Within 48 hours of report
2. **Triage**: Within 5 business days (severity assessment)
3. **Fix Development**: Based on severity (see table above)
4. **Testing**: 3-5 business days
5. **Release**: Patch released with advisory
6. **Public Disclosure**: 30 days after patch release

## CVE Request Process
- CVEs are requested for Medium+ severity vulnerabilities
- Reporter credited in CVE entry (unless anonymity requested)
- Published via GitHub Security Advisories → CVE Services
```

### 4. Create Advisory Publication Mechanism
- Enable GitHub Security Advisories in repository settings
- Document how advisories will be published (GitHub → CVE → release notes)
- Create template for security release notes

## File Structure
```
.github/
  SECURITY.md           # GitHub Security Advisories setup
docs/
  SECURITY_PROCESS.md   # Full CVD process documentation
```

## References
- [GitHub Security Advisories](https://docs.github.com/en/code-security/security-advisories)
- [CVSS Calculator](https://www.first.org/cvss/calculator/3.1)
- [CVE Program](https://www.cve.org/)
- [Cyber Resilience Act - Vulnerability Handling](https://www.eu-cra.eu/)
- [OWASP CVD Checklist](https://owasp.org/www-community/Vulnerability_Disclosure_Policy)

---
**Related to issues #001, #002** - These issues should be resolved together as part of a complete vulnerability disclosure policy implementation.
