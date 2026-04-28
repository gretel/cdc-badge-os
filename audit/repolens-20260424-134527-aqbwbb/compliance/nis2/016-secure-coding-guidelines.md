---
title: "[LOW] No documented secure coding guidelines for contributors"
severity: LOW
domain: security-awareness
lens: nis2
labels:
  - "audit:compliance/nis2"
---

## Summary
The repository lacks a CONTRIBUTING.md file with secure coding guidelines. NIS2 Article 20 requires "security of network and information systems" which includes development processes. Without documented secure coding guidelines, contributors may introduce vulnerabilities unknowingly.

## Impact
**NIS2 Art. 20 Development Process Gap**:
- No secure coding checklist for PR reviews
- No guidance on memory safety for ESP32
- No crypto API usage guidelines
- No logging best practices (avoiding sensitive data leaks)
- Contributors may introduce common vulnerabilities

## Evidence

1. **No CONTRIBUTING.md in main repo**:
   - File: `CONTRIBUTING.md` does not exist
   - Only exists in third_party/libtropic/
   - No secure coding guidelines for CDC Badge OS

2. **No SECURITY.md**:
   - File: `SECURITY.md` does not exist
   - README.md mentions "See SECURITY.md for hardening steps" but file doesn't exist
   - No process for reporting vulnerabilities

3. **Memory safety concerns in code**:
   - File: `components/mod_password/src/PasswordStore.cpp:165-168`
   ```cpp
   auto used = std::unique_ptr<bool[]>(new (std::nothrow) bool[cap]);
   if (!used) return false;
   ```
   - Dynamic allocation in embedded context
   - No guidance on when to use static vs dynamic allocation

4. **No code review checklist**:
   - File: `.github/pull_request_template.md` does not exist
   - No security review requirements for PRs
   - No checklist for crypto, memory, or access control

5. **Logging practices inconsistent**:
   - File: `components/cdc_log/include/cdc_log.h`
   - No guidelines on what NOT to log (PINs, secrets, etc.)
   - Easy to accidentally log sensitive data

## Recommended Fix

1. **Create CONTRIBUTING.md with secure coding guidelines**:
   ```markdown
   # Contributing to CDC Badge OS
   
   ## Secure Coding Guidelines
   
   ### Memory Management
   - Prefer static allocation for critical paths
   - Use `EXT_RAM_BSS_ATTR` for large buffers (PSRAM)
   - Always check allocation success
   - Free memory immediately after use
   
   ### Cryptography
   - Use mbedTLS or TROPIC01 APIs only
   - Never implement custom crypto
   - Use HKDF for key derivation
   - Use AES-256-GCM for encryption
   
   ### Logging
   - Use `LOG_I`, `LOG_D`, `LOG_W`, `LOG_E` from cdc_log
   - Never log PINs, passwords, or private keys
   - Use `%s` with care (ensure null-termination)
   
   ### Error Handling
   - Always check return values
   - Use `SeResult` for TROPIC01 operations
   - Log errors with context (file:line)
   ```

2. **Create SECURITY.md**:
   ```markdown
   # Security Policy
   
   ## Reporting Vulnerabilities
   
   Report security vulnerabilities via email: [security@example.com]
   
   Include:
   - Description of vulnerability
   - Steps to reproduce
   - Impact assessment
   - Suggested fix (optional)
   
   ## Hardening Checklist
   
   Before production use:
   - [ ] Set DEBUG_MODE=0
   - [ ] Change default PIN
   - [ ] Enable Secure Boot
   - [ ] Update firmware to latest release
   - [ ] Review access logs
   ```

3. **Add PR template with security checklist**:
   ```markdown
   ## Security Checklist
   
   - [ ] No sensitive data logged
   - [ ] Memory allocated safely (static preferred)
   - [ ] Crypto uses approved algorithms
   - [ ] Error handling complete
   - [ ] PIN/password verification required
   ```

4. **Document crypto API usage**:
   - Add examples for HKDF, AES-GCM, ECDSA
   - Show correct buffer sizes
   - Warn about common mistakes

5. **Add CI linting for security**:
   - Check for `ESP_LOG` usage (should use `cdc_log`)
   - Check for `malloc` in performance-critical paths
   - Check for hardcoded secrets

## References
- [NIS2 Directive Art. 20 - Development processes](https://eur-lex.europa.eu/eli/dir/2022/2555/oj)
- [OWASP Secure Coding Practices](https://owasp.org/www-project-secure-coding-practices-quick-reference-guide/)
- [NIST Secure Software Development Framework](https://www.nist.gov/software-quality-secure-software-development-framework)

</content>