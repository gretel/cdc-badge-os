---
title: "[LOW] Missing SAST security analysis in CI pipeline"
severity: LOW
domain: security
lens: sast
labels:
  - "ci-cd"
  - "sast"
  - "security-analysis"
---

## Summary
The CI pipeline (`.github/workflows/build.yml`) builds the firmware but does not include any static application security testing (SAST) to detect common vulnerabilities like buffer overflows, format strings, or insecure function usage.

## Impact
- **Late Discovery**: Security issues are found manually or during code review, not automatically
- **Regression Risk**: New vulnerabilities can be introduced without detection
- **Best Practices**: Modern CI/CD pipelines should include automated security analysis

## Evidence

Current CI pipeline (`.github/workflows/build.yml`):
```yaml
name: Build Firmware

on:
  push:
    branches: [main, master, release, develop, feature/*]
  pull_request:
    branches: [main, master, release]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout repository
        uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Setup Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.11'

      - name: Install PlatformIO
        run: |
          python -m pip install --upgrade pip
          pip install platformio

      - name: Build firmware
        run: pio run

      - name: Get version info
        ...
```

No security analysis step is present.

## Recommended Fix

Add a SAST analysis step to the CI pipeline. For C/C++ projects, recommended tools:

**Option 1: cppcheck (lightweight, fast)**
```yaml
- name: Install cppcheck
  run: sudo apt-get install cppcheck

- name: Run cppcheck
  run: |
    cppcheck --enable=warning,style,portability --error-exitcode=1 \
      --include=components/cdc_core/include/cdc_core/feature_flags.h \
      main/ components/
```

**Option 2: Clang Static Analyzer**
```yaml
- name: Run Clang Static Analyzer
  run: |
    pip install scan-build
    scan-build pio run
```

**Option 3: Semgrep (multi-language, customizable)**
```yaml
- name: Install Semgrep
  run: pip install semgrep

- name: Run Semgrep
  run: semgrep scan --config auto --output=semgrep.json
```

**Recommended approach:**
1. Start with cppcheck for C/C++ (fast, embedded-friendly)
2. Add to CI as a "check" step (not blocking initially)
3. Gradually fix warnings and make it blocking

## References
- OWASP: "Static Code Analysis"
- CWE: "List of Common Weaknesses"
- GitHub Actions: "Workflows for C/C++ projects"
