---
title: "[LOW] Vendored trezor_crypto Library - Version Pinning and Update Strategy"
severity: LOW
domain: dependencies
lens: dependency-cves
labels:
  - "audit:security/dependency-cves"
---

## Summary
The project vendored **trezor_crypto** library inside `third_party/libtropic/vendor/trezor_crypto/` as a git submodule. This library is used for ECC operations (ECDSA, Ed25519, secp256k1). The vendored copy is at commit `e730ebfb` (from libtropic v3.0.0), but there's no explicit version tracking or update strategy for this critical cryptographic dependency.

**Files affected:**
- `third_party/libtropic/vendor/trezor_crypto/` - Vendored trezor_crypto
- `third_party/libtropic/vendor/trezor_crypto/.gitmodules` - Submodule definition

## Impact
1. **CVE tracking gap**: trezor_crypto has known vulnerabilities in the past (e.g., related to ECDSA constant-time operations). No mechanism to track new CVEs.
2. **Stale dependency**: The vendored copy may be several commits behind the upstream trezor-crypto repository.
3. **No update automation**: No Dependabot or similar tool configured for the submodule.
4. **Cryptographic criticality**: This library handles all ECC operations for FIDO2, GPG, and SSH keys.

## Evidence
From `.gitmodules`:
```ini
[submodule "components/Adafruit-GFX"]
    path = components/Adafruit-GFX
    url = https://github.com/martinberlin/Adafruit-GFX-Library-ESP-IDF
```

From `third_party/libtropic/vendor/trezor_crypto/.gitmodules`:
```ini
[submodule "tests/wycheproof"]
    path = tests/wycheproof
    url = https://github.com/google/wycheproof
```

trezor_crypto commit used:
- Parent: libtropic v3.0.0 (commit `e730ebfb585483be2347e8a96d1722806ca8d2ca`)
- trezor_crypto is a nested submodule within libtropic

Upstream trezor-crypto: https://github.com/trezor/trezor-crypto

## Recommended Fix
1. **Document the dependency**: Add to a `crypto_dependencies.md` file:
   ```markdown
   ## trezor_crypto
   - Version: [commit hash from libtropic v3.0.0]
   - Usage: ECDSA (secp256k1, nist256p1), Ed25519, BIP32
   - Upstream: https://github.com/trezor/trezor-crypto
   - Last update: [date]
   ```

2. **Add version tracking**: Create a `third_party/libtropic/vendor/trezor_crypto/VERSION` file with the commit hash and date.

3. **Set up update reminders**: Add to CI or a cron job to check for new trezor-crypto releases:
   ```bash
   # Check for new trezor-crypto commits
   curl -s https://api.github.com/repos/trezor/trezor-crypto/commits?per_page=1
   ```

4. **Review trezor-crypto security history**: Check for past CVEs or security advisories:
   - https://github.com/trezor/trezor-crypto/security/advisories
   - https://osv.dev/v1/list?ecosystem=GitHub

5. **Consider upstreaming changes**: If the project makes any modifications to trezor_crypto, contribute them upstream for better maintenance.

## References
- trezor-crypto GitHub: https://github.com/trezor/trezor-crypto
- trezor-crypto build status: https://travis-ci.org/trezor/trezor-crypto
- Wycheproof tests: https://github.com/google/wycheproof
- ECDSA security best practices: https://crypto.stackexchange.com/questions/tagged/ecdsa
