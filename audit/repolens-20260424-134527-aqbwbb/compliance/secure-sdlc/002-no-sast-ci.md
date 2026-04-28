---
title: "[HIGH] No SAST (Static Analysis) in CI/CD pipeline"
severity: HIGH
domain: secure-sdlc
lens: compliance
labels:
  - "ci-cd"
  - "sast"
  - "security-testing"
---

## Summary
The CI/CD pipeline in `.github/workflows/build.yml` performs only basic firmware compilation. There is no Static Application Security Testing (SAST) to detect common vulnerabilities like buffer overflows, use-after-free, format string bugs, or uninitialized variables. Popular SAST tools for C/C++/ESP32 include CodeQL, Clang-Tidy, Cppcheck, or esp-idf's built-in static analysis.

## Impact
- **Undetected Vulnerabilities**: Common C/C++ memory safety bugs may go unnoticed until runtime or, worse, production deployment.
- **CRA Compliance Gap**: The Cyber-Resilience Act requires basic security testing of digital products.
- **Maintenance Burden**: Security issues found later in development are more expensive to fix.

## Evidence
File: `.github/workflows/build.yml:1-90`
The pipeline only includes:
1. Checkout repository
2. Setup Python
3. Install PlatformIO
4. Build firmware (`pio run`)
5. Artifact upload

No security analysis steps exist. Compare to a SAST-enabled workflow which would include:
```yaml
- name: Run CodeQL analysis
  uses: github/codeql-action/analyze@v2
- name: Run Cppcheck
  run: cppcheck --enable=all src/
```

## Recommended Fix
Add SAST analysis to `build.yml`:

**Option A: CodeQL (GitHub native)**
```yaml
- name: Initialize CodeQL
  uses: github/codeql-action/init@v2
  with:
    languages: cpp
- name: Perform CodeQL Analysis
  uses: github/codeql-action/analyze@v2
```

**Option B: Cppcheck (lightweight, ESP32-friendly)**
```yaml
- name: Install Cppcheck
  run: sudo apt-get install cppcheck
- name: Run Cppcheck
  run: cppcheck --enable=warning,style --std=c++17 src/ components/
```

**Option C: Clang-Tidy (ESP-IDF compatible)**
```yaml
- name: Run Clang-Tidy
  run: |
    cd .pio/build/cdc_badge_usb/
    run-clang-tidy -p .
```

Start with Cppcheck for minimal CI overhead, then add CodeQL for comprehensive coverage.

## References
- OWASP SAST Testing Guide: https://owasp.org/www-project-web-security-testing-guide/latest/4-Web_Application_Security_Testing/02-Configuration_and_Deployment_Management_Testing/10-Test_for_Buffer_Overflows
- CRA Article 10 - Essential characteristics
- ESP-IDF Static Analysis: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/tools/static-analysis.html
