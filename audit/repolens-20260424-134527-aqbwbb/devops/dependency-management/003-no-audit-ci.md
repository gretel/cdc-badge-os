---
title: "[MEDIUM] No dependency audit step in CI pipeline"
severity: MEDIUM
domain: devops
lens: dependency-management
labels:
  - "audit:devops/dependency-management"
---

## Summary
The CI build workflow (`.github/workflows/build.yml`) does not include a step to audit dependencies for known vulnerabilities. There is no `pip-audit`, `npm audit`, or ESP-IDF component audit running to detect security issues in the dependency tree.

**File:** `.github/workflows/build.yml`
**Lines:** 1-90 (entire workflow)

## Impact
- **Security risk**: Known vulnerabilities in dependencies may be silently included in production firmware
- **No early warning**: Developers aren't alerted when a new CVE affects their dependencies
- **Manual burden**: Security audits must be done manually, increasing chance of oversight
- **Release gate missing**: Vulnerable dependencies don't block builds or releases

## Evidence
Current CI build workflow steps:
1. Checkout repository
2. Setup Python 3.11
3. Install PlatformIO
4. Build firmware
5. Get version info
6. Rename artifacts
7. Upload artifacts

No audit steps present. No invocation of:
- `pip-audit` for `tools/requirements.txt`
- `esptool` version check for latest security patches
- ESP-IDF component audit (if available)

Python dependencies with floating versions that may pull vulnerable versions:
```
esptool>=4.7
requests>=2.28
bleak>=0.21
esp-coredump>=1.5
```

## Recommended Fix
Add an audit step to `.github/workflows/build.yml` after Python setup:

```yaml
      - name: Install audit tools
        run: |
          pip install pip-audit

      - name: Audit Python dependencies
        run: |
          pip-audit tools/requirements.txt
          pip-audit third_party/libtropic/docs/requirements.txt
          pip-audit third_party/libtropic/scripts/test_runner/requirements.txt
```

For ESP-IDF components, consider:
1. Adding a check for latest component versions
2. Using GitHub's Dependency Graph (if supported for CMake/ESP-IDF)
3. Manual audit script that checks ESP-IDF component registry for known issues

## References
- [pip-audit documentation](https://pip-audit.pypa.io/)
- [GitHub Dependency Review](https://docs.github.com/en/code-security/dependabot/dependency-review)
- [ESP-IDF Security Advisories](https://github.com/espressif/esp-idf/releases)

---
**Related issues:** #002 (No automated dependency update workflow)
**Estimated effort:** 1 hour
