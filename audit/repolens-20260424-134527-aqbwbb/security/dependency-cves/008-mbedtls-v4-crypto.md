---
title: "[MEDIUM] Embedded mbedTLS v4.0.0 in libtropic - Missing Independent Security Review"
severity: MEDIUM
domain: dependencies
lens: dependency-cves
labels:
  - "audit:security/dependency-cves"
---

## Summary
The project uses **mbedTLS v4.0.0** as a cryptographic abstraction layer (CAL) within libtropic, embedded in `third_party/libtropic/cal/mbedtls_v4/`. This is a **vendor-specific integration** of mbedTLS rather than a standard ESP-IDF dependency, making it harder to track security updates and CVEs specific to this integration.

**Files affected:**
- `third_party/libtropic/cal/mbedtls_v4/` - Contains mbedTLS v4.0.0 integration
- `third_party/libtropic/vendor/trezor_crypto/` - Alternative crypto library (trezor_crypto)

## Impact
1. **CVE tracking difficulty**: mbedTLS has known CVEs (e.g., CVE-2020-25698, CVE-2021-33591 for AES-GCM). The embedded version may not have all patches.
2. **Integration risk**: The CAL wrapper (`lt_mbedtls_v4_aesgcm.c`, `lt_mbedtls_v4_x25519.c`) adds an abstraction layer that could introduce bugs.
3. **Dual crypto libraries**: Both mbedTLS v4 and trezor_crypto are vendored, increasing attack surface.
4. **Limited visibility**: mbedTLS is not listed in `dependencies.lock`, making dependency auditing incomplete.

## Evidence
From `third_party/libtropic/cal/mbedtls_v4/libtropic_mbedtls_v4.h`:
```c
/**
 * @file lt_mbedtls_v4.h
 * @brief MbedTLS v4.0.0 public declarations.
 */
```

From `third_party/libtropic/cal/mbedtls_v4/lt_mbedtls_v4_aesgcm.c`:
```c
// AES-GCM context structure using mbedTLS v4.0.0
typedef struct lt_aesgcm_ctx_mbedtls_v4_t {
    mbedtls_aes_context key_ctx;
    // ...
} lt_aesgcm_ctx_mbedtls_v4_t;
```

mbedTLS v4.0.0 release date: October 2023. Current mbedTLS version: 3.6.x (as of 2024).

Known mbedTLS CVEs (check for applicability):
- CVE-2020-25698: AES-GCM timing attack
- CVE-2021-33591: ECDSA signature verification
- CVE-2022-27776: PKCS#7 padding oracle

## Recommended Fix
1. **Audit mbedTLS version**: Verify which mbedTLS v4.0.0 features are used and check against known CVEs:
   ```bash
   # Check mbedTLS changelog for v4.0.0
   https://github.com/Mbed-TLS/mbedtls/releases/tag/mbedtls-4.0.0
   ```

2. **Consider using ESP-IDF's mbedTLS**: ESP-IDF v5.5 includes mbedTLS 3.5.0 with security patches. Configure libtropic to use the system mbedTLS instead of vendored version:
   ```cmake
   # In third_party/libtropic/CMakeLists.txt
   target_link_libraries(tropic PUBLIC mbedtls)
   ```

3. **Document the integration**: Add a `crypto_dependencies.md` file listing:
   - mbedTLS v4.0.0 (AES-GCM, SHA-256, HMAC, X25519)
   - trezor_crypto (ECDSA, Ed25519)
   - Version pinning and CVE tracking plan

4. **Set up CVE monitoring**: Use tools like `pip-audit` for Python deps and `osv.dev` for mbedTLS:
   ```bash
   curl -s https://osv.dev/v1/vulns/CVE-2020-25698
   ```

5. **Consider trezor_crypto only**: For a hardware wallet application, trezor_crypto is more battle-tested for ECC operations. Evaluate if mbedTLS is needed.

## References
- mbedTLS v4.0.0 release: https://github.com/Mbed-TLS/mbedtls/releases/tag/mbedtls-4.0.0
- mbedTLS changelog: https://github.com/Mbed-TLS/mbedtls/blob/v4.0.0/CHANGES
- mbedTLS CVEs: https://github.com/Mbed-TLS/mbedtls/security/advisories
- ESP-IDF mbedTLS: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/crypto/mbedtls.html
- libtropic crypto HAL: `third_party/libtropic/cal/mbedtls_v4/`
