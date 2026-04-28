---
title: "[LOW] No documented security update mechanism or patching SLA"
severity: LOW
domain: cyber-resilience-act
lens: update-mechanism
labels:
  - "security-updates"
  - "patching"
  - "cra-2026"
---

## Summary
The project lacks a documented security update mechanism and patching SLA. There is no information about:
- How security patches are communicated to users
- Expected timeframes for critical/high vulnerability fixes
- Versioning strategy for security releases

**Files affected:**
- `README.md` - No security update section
- `docs/` - No patching/update documentation
- No `CHANGELOG.md` with security-focused release notes

## Impact
Under CRA (Article 11), manufacturers must provide security updates:
- **User uncertainty**: Users don't know how long they can expect security support
- **Patch communication**: Security fixes may not be clearly communicated
- **Compliance gap**: CRA requires clear update process and timelines

**Current state:**
- Releases are tagged with firmware binaries
- Release notes are auto-generated from commits (`generate_release_notes: true`)
- No explicit security-focused changelog or advisory system

## Recommended Fix
Create security update documentation:

1. **Add SECURITY.md section** (or update existing):
```markdown
## Security Updates

### Patching SLA

| Severity | Target Response | Target Patch |
|----------|-----------------|--------------|
| Critical | 7 days          | 30 days      |
| High     | 14 days         | 60 days      |
| Medium   | 30 days         | 90 days      |
| Low      | 60 days         | 180 days     |

### Release Process

1. Security vulnerabilities are tracked privately
2. Patch is developed and tested
3. Release tagged as `vX.Y.Z` with security advisory
4. Users notified via GitHub Releases and documentation
```

2. **Create security advisory template** in `docs/`:
```markdown
# Security Advisory: CVE-YYYY-XXXX

**Severity**: High
**Affected Versions**: < v1.2.3
**Fixed in**: v1.2.4

## Description
Brief description of vulnerability...

## Impact
What can an attacker do?...

## Mitigation
Steps to fix...
```

3. **Update release workflow** to include security notes:
```yaml
- name: Create Release
  uses: softprops/action-gh-release@v1
  with:
    body: |
      ## Security
      - Fixed CVE-YYYY-XXXX: [description]
    generate_release_notes: true
```

## References
- [EU CRA Article 11 - Security updates](https://digital-strategy.ec.europa.eu/en/library/cyber-resilience-act)
- [Common Vulnerabilities and Exposures (CVE)](https://cve.mitre.org/)
- [GitHub Security Advisories](https://docs.github.com/en/code-security/security-advisories)
