---
title: "[MEDIUM] No Automated Dependency Update Policy Configured"
severity: MEDIUM
domain: dependency-management
lens: maintainability
labels:
  - "ci-cd"
  - "dependencies"
  - "security"
---

## Summary

The repository lacks automated dependency update tooling. There is no **Dependabot**, **Renovate**, or equivalent configured to automatically create PRs for dependency updates. This means security patches and feature updates must be manually tracked and applied.

**Dependencies that need tracking:**

1. **ESP-IDF Framework** (v5.5.0) - in `dependencies.lock`
2. **Espressif components**:
   - `espressif/led_strip` (v2.5.5)
   - `espressif/qrcode` (v0.2.0)
   - `espressif/tinyusb` (v0.19.0~2)
3. **PlatformIO packages** (in `platformio.ini`):
   - `espressif32` platform (v6.12.0)
   - `framework-espidf` (~3.50500.0)
   - `toolchain-xtensa-esp32s3` (v12.2.0+20230208)
4. **Python tools** (in `tools/requirements.txt`):
   - `esptool` (>=4.7)
   - `requests` (>=2.28)
   - `bleak` (>=0.21)
   - `esp-coredump` (>=1.5)
5. **Git submodules**:
   - `components/CalEPD` (martinberlin/CalEPD)
   - `third_party/libtropic` (tropicsquare/libtropic)
   - `components/Adafruit-GFX` (martinberlin/Adafruit-GFX-Library-ESP-IDF)

**Current state:**
```bash
$ ls -la .github/
total 3
drwxr-xr-x 1 repolens repolens   40 Apr 23 11:24 .
drwxr-xr-x 1 repolens repolens  1394 Apr 23 11:24 ..
-rwxr-r-- 1 repolens repolens  163 Feb 25 07:35 ._workflows
drwxr-xr-x 1 repolens repolens  108 Apr 23 11:24 workflows

$ cat .github/dependabot.yml 2>/dev/null || echo "No dependabot.yml found"
No dependabot.yml found

$ cat .github/renovate.json 2>/dev/null || echo "No renovate.json found"
No renovate.json found
```

**Build workflow** (`.github/workflows/build.yml`) does not include dependency update checks:
- No `npm audit`, `pip-audit`, or equivalent commands
- No version pinning verification
- No transitive dependency scanning

## Impact

1. **Security Risk**: Vulnerabilities in dependencies may go unnoticed until exploited
2. **Manual Overhead**: Developers must manually check for updates across 5+ dependency sources
3. **Delayed Patches**: Critical security updates may take weeks or months to apply
4. **Inconsistent Updates**: Different developers may use different dependency versions
5. **No Audit Trail**: No automated record of dependency updates and their timing

## Recommended Fix

**Option 1: GitHub Dependabot** (Recommended for GitHub-hosted repos)

Create `.github/dependabot.yml`:

```yaml
version: 2
updates:
  # Python dependencies
  - package-manager: "pip"
    directory: "/tools"
    schedule:
      interval: "weekly"
      day: "monday"
    open-pull-requests-limit: 5
    labels:
      - "dependencies"
      - "python"

  # ESP-IDF components (via ESP-IDF Component Manager)
  - package-manager: "docker"
    directory: "/"
    schedule:
      interval: "weekly"
    labels:
      - "dependencies"
      - "esp-idf"

  # GitHub Actions
  - package-manager: "github-actions"
    directory: "/"
    schedule:
      interval: "weekly"
    labels:
      - "dependencies"
      - "ci"
```

**Option 2: Renovate** (More flexible, supports more package managers)

Create `.github/renovate.json`:

```json
{
  "$schema": "https://docs.renovatebot.com/renovate-schema.json",
  "extends": ["config:recommended"],
  "schedule": ["every monday"],
  "packageRules": [
    {
      "matchManagers": ["pip_requirements"],
      "matchPaths": ["tools/**"],
      "groupName": "Python tools"
    },
    {
      "matchManagers": ["github-actions"],
      "groupName": "GitHub Actions"
    }
  ]
}
```

**Add dependency audit to CI** (`.github/workflows/build.yml`):

```yaml
- name: Install dependency audit tools
  run: |
    pip install pip-audit

- name: Audit Python dependencies
  run: |
    pip-audit tools/requirements.txt

- name: Check for outdated ESP-IDF components
  run: |
    idf.py --version
```

## References

- [GitHub Dependabot documentation](https://docs.github.com/en/code-security/dependabot)
- [Renovate documentation](https://docs.renovatebot.com/)
- [ESP-IDF Component Manager](https://components.espressif.com/)
- [pip-audit](https://pip-audit.readthedocs.io/)
- [OWASP Dependency-Check](https://owasp.org/www-project-dependency-check/)
