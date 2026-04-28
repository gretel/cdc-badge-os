---
title: "[MEDIUM] No security awareness training materials or guidelines for contributors"
severity: MEDIUM
domain: security-awareness
lens: nis2
labels:
  - "audit:compliance/nis2"
---

## Summary
The repository lacks security awareness training materials, secure coding guidelines for contributors, or documentation that security practices are communicated to the development team. NIS2 requires "basic security training" for personnel (Art. 20).

## Impact
**NIS2 Art. 20 Training Gap**:
- Contributors may not know secure coding practices
- No onboarding process for security requirements
- Inconsistent security implementation across modules
- No way to verify team understands security requirements

## Evidence
1. **No CONTRIBUTING.md in root**:
   - No `CONTRIBUTING.md` file found in repository root
   - File: `README.md:237-242` mentions "See [SECURITY.md](SECURITY.md)" but file doesn't exist
   - No centralized contribution guidelines

2. **No SECURITY.md in root**:
   - README references `SECURITY.md` but file doesn't exist
   - File: `README.md:7`
   ```
   > **Early Alpha** - This firmware is in active development and **not production ready**.
   > Security hardening is incomplete. Do not use for protecting critical accounts.
   > See [SECURITY.md](SECURITY.md) for hardening steps required before production use.
   ```
   - Link is broken (file not found)

3. **No secure coding guidelines**:
   - No `SECURE_CODING.md` or similar
   - No list of common pitfalls for ESP32
   - No memory safety guidelines
   - No crypto implementation guidelines

4. **No security checklist for PRs**:
   - No `PULL_REQUEST_TEMPLATE.md` with security checklist
   - No automated security checks in CI

5. **Module development guide lacks security focus**:
   - File: `docs/MODULE_DEVELOPMENT.md`
   - Focuses on architecture, not security best practices
   - No section on secure storage, PIN handling, etc.

## Recommended Fix
1. **Create CONTRIBUTING.md**:
   - Add contribution workflow
   - Include security requirements
   - Link to secure coding guidelines

2. **Create SECURITY.md**:
   - Document known security limitations
   - List hardening steps for production
   - Include security contact information
   - Document vulnerability reporting process

3. **Create Secure Coding Guidelines** (`docs/SECURE_CODING.md`):
   - Memory safety best practices for ESP32
   - Crypto implementation guidelines
   - PIN/password handling rules
   - Secure storage patterns

4. **Add PR Security Checklist**:
   - Create `.github/PULL_REQUEST_TEMPLATE.md`
   - Include security review questions
   - Add checklist for common security issues

5. **Add Security Module Documentation**:
   - Document security patterns in `docs/SECURITY_ARCHITECTURE.md`
   - Explain how to implement secure features
   - Provide code examples

## References
- [NIS2 Directive Art. 20 - Basic security training](https://eur-lex.europa.eu/eli/dir/2022/2555/oj)
- [OWASP Secure Coding Practices](https://owasp.org/www-project-secure-coding-practices-quick-reference-guide/)
- [NIST SP 800-218 - Secure Software Development Framework](https://csrc.nist.gov/publications/detail/sp/800-218/final)
