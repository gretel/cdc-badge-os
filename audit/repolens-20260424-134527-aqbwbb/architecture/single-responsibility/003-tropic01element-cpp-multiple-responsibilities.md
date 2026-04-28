---
title: "[MEDIUM] Tropic01Element.cpp combines secure session management, ECC operations, R-Memory, and diagnostics"
severity: MEDIUM
domain: cdc_hal
lens: single-responsibility
labels:
  - "audit:architecture/single-responsibility"
---

## Summary
`components/cdc_hal/src/Tropic01Element.cpp` (890 lines) handles multiple distinct responsibilities:
1. **Session management** - `sessionStart()`, `sessionEnd()`, `sleep()`, `ensureSession()`
2. **ECC operations** - `eccGenerate()`, `eccImport()`, `eccGetPublicKey()`, `eccDelete()`, `ecdsaSign()`, `eddsaSign()`
3. **R-Memory operations** - `rmemRead()`, `rmemWrite()`, `rmemErase()`, `rmemWriteWithHeader()`, `rmemReadWithHeader()`
4. **Diagnostics** - `getChipId()`, `getFwVersion()`
5. **Random generation** - `getRandom()`
6. **Result mapping** - `mapResult()`, `handleSessionError()`

## Impact
- **High coupling**: Changes to ECC, R-Memory, or session management all require modifying the same file
- **Complexity**: 890 lines with mixed concerns makes it hard to navigate
- **Testing difficulty**: Cannot test R-Memory operations without session setup
- **Code duplication**: Similar patterns for ECC and R-Memory operations could be abstracted

## Evidence
File: `components/cdc_hal/src/Tropic01Element.cpp`
- Lines 38-99: Class definition with all methods
- Lines 109-299: Session management and initialization
- Lines 301-532: ECC operations (generate, import, get, delete, sign)
- Lines 534-776: R-Memory operations (read, write, erase, header helpers)
- Lines 778-890: Random, diagnostics

Key pattern showing mixed concerns - ECC and R-Memory operations follow identical structure:
```cpp
// ECC operation pattern
SeResult Tropic01Element::eccGenerate(uint8_t slot, EccCurve curve) {
    lock();
    if (!ensureSession("eccGenerate")) { unlock(); return SeResult::SESSION_REQUIRED; }
    lt_ret_t ret = lt_ecc_key_generate(&handle_, ...);
    handleSessionError(ret);
    unlock();
    return mapResult(ret);
}

// R-Memory operation pattern
SeResult Tropic01Element::rmemWrite(uint16_t slot, const uint8_t* data, uint16_t len) {
    lock();
    if (!ensureSession("rmemWrite")) { unlock(); return SeResult::SESSION_REQUIRED; }
    lt_ret_t ret = lt_r_mem_data_write(&handle_, slot, data, len);
    handleSessionError(ret);
    unlock();
    return mapResult(ret);
}
```

## Recommended Fix
Split into focused modules:
1. **TropicSession** - Session management in `components/cdc_hal/src/TropicSession.cpp`
2. **TropicEcc** - ECC operations in `components/cdc_hal/src/TropicEcc.cpp`
3. **TropicRmem** - R-Memory operations in `components/cdc_hal/src/TropicRmem.cpp`
4. **TropicDiagnostics** - Chip ID and version in `components/cdc_hal/src/TropicDiagnostics.cpp`
5. **TropicTrng** - Random generation in `components/cdc_hal/src/TropicTrng.cpp`

Each module should:
- Have its own header file with specific interface
- Accept session handle via constructor
- Be testable in isolation

Example split structure:
```
components/cdc_hal/src/
  Tropic01Element.cpp  // Main orchestration, delegates to sub-modules
  TropicSession.cpp    // Session management
  TropicEcc.cpp        // ECC operations
  TropicRmem.cpp       // R-Memory operations
  TropicDiagnostics.cpp // Diagnostics
  TropicTrng.cpp       // Random generation
```

## References
- Single Responsibility Principle: https://en.wikipedia.org/wiki/Single-responsibility_principle
- High Cohesion: https://en.wikipedia.org/wiki/Cohesion_(computer_science)
