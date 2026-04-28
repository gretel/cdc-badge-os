---
title: "[MEDIUM] No Static Application Security Testing (SAST) in CI/CD"
severity: MEDIUM
domain: cyber-resilience-act
lens: sast
labels:
  - "sast"
  - "ci-cd"
  - "cra-2026"
---

## Summary
The CI/CD pipeline lacks Static Application Security Testing (SAST). No tools like CodeQL, Semgrep, SonarQube, or similar are configured to analyze code for security vulnerabilities during builds.

**Files affected:**
- `.github/workflows/build.yml` - No SAST step
- `.github/workflows/deploy-pages.yml` - No SAST step
- Repository root - No `sonar-project.properties` or similar

## Impact
Under CRA (Article 11), manufacturers must implement security-by-design:
- **Undetected vulnerabilities**: Common security issues (buffer overflows, null pointer dereferences) may go unnoticed
- **Late discovery**: Security issues found only after deployment
- **Compliance gap**: CRA encourages security testing throughout development lifecycle

**Codebase characteristics** (C++ on ESP32):
- Large codebase with memory management (PSRAM, static allocation)
- Cryptographic operations (FIDO2, TOTP, SSH keys)
- Interrupt handlers and concurrent access
- Hardware interface code (TROPIC01, E-Paper display)

## Recommended Fix
Add CodeQL or Semgrep for SAST:

1. **Add CodeQL to build.yml** (GitHub native, good for C/C++):
```yaml
name: Build Firmware

on:
  push:
    branches: [main, master, release, develop]
  pull_request:
    branches: [main, master, release]

jobs:
  analyze:
    name: Analyze
    runs-on: ubuntu-latest
    permissions:
      actions: read
      contents: read
      security-events: write

    steps:
    - name: Checkout repository
      uses: actions/checkout@v4

    - name: Initialize CodeQL
      uses: github/codeql-action/init@v2
      with:
        languages: cpp
        queries: +security-extended

    - name: Setup Python
      uses: actions/setup-python@v5
      with:
        python-version: '3.11'

    - name: Install PlatformIO
      run: pip install platformio

    - name: Build firmware (for CodeQL)
      run: pio run

    - name: Perform CodeQL Analysis
      uses: github/codeql-action/analyze@v2
```

2. **Alternative: Add Semgrep** (faster, more configurable):
```yaml
- name: Scan with Semgrep
  uses: returntocorp/semgrep-action@v1
  with:
    config: >-
      c/cpp
      auto
```

## References
- [EU CRA Annex I - Security-by-design](https://digital-strategy.ec.europa.eu/en/library/cyber-resilience-act)
- [CodeQL for C/C++](https://codeql.github.com/docs/codeql-overview/about-codeql/)
- [Semgrep C rules](https://semgrep.dev/docs/cheat-sheet/cpp/)
