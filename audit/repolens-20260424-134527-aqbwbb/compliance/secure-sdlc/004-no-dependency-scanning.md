---
title: "[MEDIUM] No dependency vulnerability scanning in CI/CD"
severity: MEDIUM
domain: secure-sdlc
lens: compliance
labels:
  - "ci-cd"
  - "dependencies"
  - "security-testing"
---

## Summary
The project uses external dependencies (PlatformIO, ESP-IDF, libtropic, Adafruit-GFX) but has no automated dependency vulnerability scanning. Dependabot (GitHub native) or tools like Snyk can detect known vulnerabilities in dependencies and create pull requests for updates.

## Impact
- **Known Vulnerabilities**: Dependencies with CVEs may remain unpatched.
- **Supply Chain Risk**: Third-party code may contain vulnerabilities that affect the badge.
- **Compliance Gap**: CRA requires tracking of dependencies and their security status.

## Evidence
File: `.github/workflows/build.yml`
No dependency scanning step exists.

Files checked for dependencies:
- `platformio.ini`: Uses `framework-espidf`, `toolchain-xtensa-esp32s3`
- `lib_deps`: Empty (manual dependencies)
- `components/`: Third-party libraries (Adafruit-GFX, CalEPD, libtropic)

No `dependabot.yml` configuration found in `.github/`.

## Recommended Fix
**Step 1: Add Dependabot configuration**
Create `.github/dependabot.yml`:
```yaml
version: 2
updates:
  - package-ecosystem: "github-actions"
    directory: "/"
    schedule:
      interval: "weekly"
    labels:
      - "dependencies"
      - "security"

  - package-ecosystem: "pip"
    directory: "/tools"
    schedule:
      interval: "weekly"
    labels:
      - "dependencies"
      - "security"

  - package-ecosystem: "npm"
    directory: "/web-flasher"
    schedule:
      interval: "weekly"
    labels:
      - "dependencies"
      - "security"
```

**Step 2: Add dependency scanning to CI**
Add to `build.yml`:
```yaml
- name: Check for vulnerabilities
  uses: snyk/actions/setup@master
- name: Run Snyk
  run: snyk test
  env:
    SNYK_TOKEN: ${{ secrets.SNYK_TOKEN }}
```

**Step 3: Enable GitHub Dependabot**
In repository settings → Code security → Dependabot → Enable

## References
- Dependabot: https://docs.github.com/en/code-security/dependabot
- Snyk: https://snyk.io/
- CRA Article 11 - Vulnerability handling
- OWASP Supply Chain Security
