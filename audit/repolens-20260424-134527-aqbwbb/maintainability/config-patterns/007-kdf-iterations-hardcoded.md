---
title: "[MEDIUM] Hardcoded KDF iterations without configuration"
severity: MEDIUM
domain: maintainability/config-patterns
lens: config-patterns
labels:
  - "audit:maintainability/config-patterns"
---

## Summary
KDF (Key Derivation Function) iterations for PIN hashing are hardcoded to 100000 in `PinManager.h` without being configurable. This affects security (brute-force protection) and performance (PIN verification speed) but cannot be tuned without code changes.

## Impact
1. **Security tuning**: Cannot adjust iterations for different security/performance trade-offs
2. **Hardware variants**: Different CPUs may need different iteration counts for same verification time
3. **Future-proofing**: As CPUs get faster, iterations may need to increase (hard to track)
4. **Documentation gap**: No explanation of why 100000 was chosen

## Evidence
**Hardcoded KDF configuration:**
```cpp
// components/cdc_core/include/cdc_core/PinManager.h:25-26
/** \brief Default KDF iteration count. */
static constexpr uint32_t DEFAULT_ITERATIONS = 100000;

// components/cdc_core/src/PinManager.cpp:45
iterations_ = DEFAULT_ITERATIONS;
```

**Usage in PIN verification:**
- Badge PIN (4-8 digits): Uses KDF with 100000 iterations
- OpenPGP PW1 (User PIN): Uses KDF with 100000 iterations  
- OpenPGP PW3 (Admin PIN): Uses KDF with 100000 iterations

**No configuration options:**
- No build flag to adjust iterations
- No runtime configuration
- No documentation of security implications

**Security context:**
```cpp
// components/cdc_core/include/cdc_core/PinManager.h:10-15
/**
 * PIN Manager - Manages all device PINs in TROPIC01 R-Memory Slot 0
 * 
 * [PIN Algorithm]          (1)  - KDF_ITERSALTED_S2K
 * [Iteration Count]        (4)  - Default 100000
 * [Badge/FIDO2 Hash]       (16) - LEFT(SHA256(PIN), 100000)
 * [Badge Retries]          (1)  - Remaining attempts for Badge PIN
 */
```

## Recommended Fix
1. **Add KDF configuration to centralized config**:
   ```cpp
   // components/cdc_core/include/cdc_core/Config.h
   namespace cdc::config {
   namespace kdf {
   // KDF iteration count for PIN hashing
   // Balance between security (higher = harder to brute-force) 
   // and performance (lower = faster verification)
   // 
   // Recommendation:
   // - Minimum: 50000 (acceptable for 4-digit PINs)
   // - Standard: 100000 (good balance)
   // - High security: 200000+ (slower verification)
   constexpr uint32_t DEFAULT_ITERATIONS = 100000;
   
   // Minimum acceptable iterations (for validation)
   constexpr uint32_t MIN_ITERATIONS = 50000;
   
   // KDF algorithm (OpenPGP standard)
   constexpr uint8_t ALGO = 0x03;  // KDF_ITERSALTED_S2K
   }
   }
   ```

2. **Add build flag for tuning**:
   ```cpp
   // components/cdc_core/include/cdc_core/Config.h
   #ifndef KDF_ITERATIONS
   #define KDF_ITERATIONS 100000
   #endif
   
   namespace cdc::config {
   namespace kdf {
   constexpr uint32_t DEFAULT_ITERATIONS = KDF_ITERATIONS;
   }
   }
   ```

3. **Update PinManager to use config**:
   ```cpp
   // components/cdc_core/src/PinManager.cpp
   #include "cdc_core/Config.h"
   
   // Instead of:
   // static constexpr uint32_t DEFAULT_ITERATIONS = 100000;
   
   // Use:
   static constexpr uint32_t DEFAULT_ITERATIONS = cdc::config::kdf::DEFAULT_ITERATIONS;
   ```

4. **Add validation**:
   ```cpp
   // components/cdc_core/src/Config.cpp
   bool validate() {
       if (cdc::config::kdf::DEFAULT_ITERATIONS < cdc::config::kdf::MIN_ITERATIONS) {
           LOG_W("Config", "KDF iterations < %u may be weak for brute-force protection", 
                 cdc::config::kdf::MIN_ITERATIONS);
       }
       return true;
   }
   ```

5. **Document in CONFIGURATION.md**:
   ```markdown
   ## KDF Configuration
   
   **KDF Iterations**: Number of iterations for PIN hashing
   - Default: 100000
   - Range: 50000-500000
   - Set in `platformio.ini`: `-DKDF_ITERATIONS=200000`
   
   Higher iterations = better security but slower verification.
   ```

## References
- [OpenPGP KDF Specification](https://tools.ietf.org/html/rfc4880#section-3.7.2.1)
- [NIST KDF Recommendations](https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-132.pdf)
- [Password Hashing Best Practices](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html)
