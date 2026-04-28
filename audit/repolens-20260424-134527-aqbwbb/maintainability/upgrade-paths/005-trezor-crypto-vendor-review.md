---
title: "[LOW] trezor-crypto vendor library update review needed"
severity: LOW
domain: dependencies
lens: upgrade-paths
labels:
  - "audit:maintainability/upgrade-paths"
---

## Summary
The project vendors `trezor-crypto` in `third_party/libtropic/vendor/trezor_crypto/` as a submodule. This library is used for cryptographic operations in FIDO2 and GPG modules. The upstream repository may have security improvements or optimizations worth reviewing.

**Evidence:**
- Location: `third_party/libtropic/vendor/trezor_crypto/`
- Used by: libtropic for cryptographic primitives (secp256k1, ed25519, SHA256, etc.)
- Upstream: [trezor/trezor-crypto](https://github.com/trezor/trezor-crypto)

## Impact
**Potential benefits:**
- Security improvements and bug fixes
- Performance optimizations for embedded devices
- New curve support (if needed)

**Risk:**
- Vendor library is deeply integrated into libtropic's build system
- Changes may require testing FIDO2 and GPG modules
- Submodule structure within libtropic needs careful handling

## Evidence
Directory structure:
```
third_party/libtropic/vendor/trezor_crypto/
├── CMakeLists.txt
├── README.md
├── secp256k1.c/h    # Bitcoin curve (FIDO2)
├── ed25519-donna/   # Ed25519 implementation
├── sha2.c/h         # SHA-256
├── blake2b.c/h      # BLAKE2b
└── ... (many crypto primitives)
```

From `vendor/trezor_crypto/README.md`:
> Heavily optimized cryptography algorithms for embedded devices.

## Recommended Fix
1. **Check upstream status**:
   ```bash
   cd third_party/libtropic/vendor/trezor_crypto
   git remote -v
   git fetch origin
   git log --oneline HEAD..origin/master | head -20
   ```

2. **Review changes**: Look for security fixes, performance improvements, or API changes.

3. **Update if beneficial**:
   ```bash
   git checkout master  # or specific version
   cd ../../..
   git add vendor/trezor_crypto
   git commit -m "chore: update trezor-crypto vendor library"
   ```

4. **Test cryptography**: Run FIDO2 registration/authentication tests and GPG key operations.

**Note:** This is a **review task** - update only if upstream has meaningful improvements.

## References
- [trezor-crypto repository](https://github.com/trezor/trezor-crypto)
- [trezor-crypto releases](https://github.com/trezor/trezor-crypto/releases)
