---
title: "[MEDIUM] Missing SAST (Static Application Security Testing) in CI pipeline"
severity: MEDIUM
domain: devops
lens: ci-pipeline
labels:
  - "audit:devops/ci-pipeline"
---

## Summary
The CI pipeline lacks Static Application Security Testing (SAST) to detect common C++ vulnerabilities like buffer overflows, memory leaks, uninitialized variables, and null pointer dereferences. The codebase uses ESP-IDF with C++17, and security is paramount for a hardware security key.

**Evidence:**
- File: `.github/workflows/build.yml` - Lines 1-90
- No Clang Static Analyzer, Cppcheck, or CodeQL analysis
- No linting step (golangci-lint equivalent for C++)

## Impact
- Common C++ bugs can slip into production firmware
- Security-critical code (FIDO2, SSH, TOTP) may have undetected vulnerabilities
- Buffer overflows in secure element communication could expose keys
- Manual code review is the only safety net

## Evidence
```yaml
# build.yml flow:
- name: Build firmware
  run: pio run
# No static analysis, no linting before build
```

Additional evidence: The project uses ESP_LOG in CalEPD and mod_gpg components (legacy code) which should be flagged during static analysis to ensure consistency with cdc_log library.

## Recommended Fix
Add a static analysis step to `.github/workflows/build.yml`:

```yaml
- name: Install Cppcheck
  run: sudo apt-get install -y cppcheck

- name: Run Cppcheck
  run: |
    cppcheck --enable=all --inconclusive --std=c++17 \
      --include=components/cdc_core/include/ \
      --include=components/cdc_hal/include/ \
      src/ 2>&1 | tee cppcheck.log
```

Alternatively, enable GitHub's built-in CodeQL analysis:
1. Create `.github/codeql-config.yml` for ESP32/PlatformIO projects
2. Add CodeQL analysis job to build.yml

## References
- [Cppcheck](http://cppcheck.sourceforge.net/)
- [CodeQL for C/C++](https://codeql.github.com/docs/codeql-overview/)
- [Clang Static Analyzer](https://clang-analyzer.llvm.org/)
