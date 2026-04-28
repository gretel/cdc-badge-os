---
title: "[MEDIUM] No automated dependency update workflow configured"
severity: MEDIUM
domain: devops
lens: dependency-management
labels:
  - "audit:devops/dependency-management"
---

## Summary
The repository has no automated dependency update configuration. No `dependabot.yml` in `.github/` directory and no `renovate.json` in root. This means:
- Python dependencies in `tools/requirements.txt` and third_party docs are never checked for updates
- ESP-IDF components (managed via `dependencies.lock`) are not automatically updated
- PlatformIO/ESP32 platform dependencies are not tracked
- Git submodules (CalEPD, libtropic, Adafruit-GFX) have no update tracking

## Impact
- **Security vulnerabilities**: Dependencies with known CVEs may not be discovered until manually audited
- **Missed updates**: Minor/patch updates that could fix bugs are not discovered automatically
- **Technical debt**: Dependencies slowly drift outdated, making eventual major updates harder
- **Manual overhead**: Developers must manually check for updates across multiple package managers

## Evidence
Directory: `.github/workflows/` contains only `build.yml` and `deploy-pages.yml`
- No `dependabot.yml` in `.github/`
- No `renovate.json` in root directory
- `platformio.ini` uses `espressif32@6.12.0` with no update mechanism
- `tools/requirements.txt` has no automated update process

## Recommended Fix
Create `.github/dependabot.yml` with configuration for all dependency types:

```yaml
version: 2
updates:
  # Python dependencies (tools)
  - package-manager: pip
    directory: "/tools"
    schedule:
      interval: weekly
      day: monday
    open-pull-requests-limit: 5
    labels:
      - "dependencies"
      - "python"

  # Python dependencies (libtropic docs)
  - package-manager: pip
    directory: "/third_party/libtropic/docs"
    schedule:
      interval: weekly
    open-pull-requests-limit: 3
    labels:
      - "dependencies"
      - "docs"

  # ESP-IDF components
  - package-manager: composer
    directory: "/"
    schedule:
      interval: weekly
    open-pull-requests-limit: 3
    labels:
      - "dependencies"
      - "esp-idf"
    # Note: ESP-IDF components use manifest, may need custom script

  # Git submodules (check upstream repos)
  - package-manager: gitsubmodule
    directory: "/"
    schedule:
      interval: weekly
    open-pull-requests-limit: 3
    labels:
      - "dependencies"
      - "submodules"
```

Alternative: Use Renovate (more flexible for monorepos):
1. Create `renovate.json` in root
2. Enable Renovate GitHub App on repository
3. Configure for pip, gitsubmodules, and custom ESP-IDF component checks

## References
- [Dependabot documentation](https://docs.github.com/en/code-security/dependabot)
- [Dependabot config reference](https://docs.github.com/en/code-security/dependabot/dependabot-version-updates/configuration-options-for-the-dependabot.yml-file)
- [Renovate documentation](https://docs.renovatebot.com/)
