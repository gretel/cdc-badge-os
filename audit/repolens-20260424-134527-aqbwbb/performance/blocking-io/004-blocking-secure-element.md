---
title: "[MEDIUM] Synchronous secure element operations blocking main thread"
severity: MEDIUM
domain: performance
lens: blocking-io
labels:
  - "performance"
  - "secure-element"
  - "tropic01"
---

## Summary

All TROPIC01 secure element operations in `components/cdc_hal/src/Tropic01Element.cpp` are synchronous and blocking. Operations like ECC key generation, signing, and R-memory reads/writes can take 10-100ms each, blocking the calling task during the entire operation.

**Affected file:**
- `components/cdc_hal/src/Tropic01Element.cpp` (all ECC and R-memory operations)

**Key blocking operations:**
- `eccGenerate()`: Key generation (~50-100ms)
- `ecdsaSign()`: ECDSA signing (~20-50ms)
- `eddsaSign()`: EdDSA signing (~10-30ms)
- `rmemRead()`, `rmemWrite()`: R-memory operations (~5-20ms each)
- `sessionStart()`: Secure session setup (~30-50ms)

**Evidence:**
```cpp
// components/cdc_hal/src/Tropic01Element.cpp:336-351
SeResult Tropic01Element::eccGenerate(uint8_t slot, EccCurve curve) {
    lock();  // Blocks entire task
    
    if (!ensureSession("eccGenerate")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    lt_ret_t ret = lt_ecc_key_generate(&handle_, static_cast<lt_ecc_slot_t>(slot), ltCurve);
    // This call blocks for 50-100ms
    
    unlock();
    return mapResult(ret);
}
```

```cpp
// components/cdc_hal/src/Tropic01Element.cpp:493-508
SeResult Tropic01Element::ecdsaSign(uint8_t slot, const uint8_t* hash, size_t hashLen,
                                     uint8_t* sig, size_t* sigLen) {
    lock();  // Blocks entire task
    
    lt_ret_t ret = lt_ecc_ecdsa_sign(&handle_, static_cast<lt_ecc_slot_t>(slot),
                                      hash, static_cast<uint32_t>(hashLen), sig);
    // This call blocks for 20-50ms
    
    unlock();
    return mapResult(ret);
}
```

## Impact

1. **UI freeze during FIDO2 operations**: When signing a FIDO2 challenge, the UI freezes for 20-50ms.

2. **Delayed keypad response**: If a secure element operation happens during keypad scanning, keypresses may be delayed.

3. **No concurrent operations**: Only one secure element operation can run at a time (mutex-locked).

4. **FIDO2 assertion latency**: Multiple signing operations during assertion (e.g., for multiple credentials) add up.

5. **GPG operations**: Key generation and signing operations block for extended periods.

## Evidence

**Blocking secure element operations:**
- `eccGenerate()`: ~50-100ms (line 336-351)
- `eccImport()`: ~30-50ms (line 361-385)
- `ecdsaSign()`: ~20-50ms (line 480-508)
- `eddsaSign()`: ~10-30ms (line 518-528)
- `rmemRead()`: ~5-15ms (line 547-576)
- `rmemWrite()`: ~10-20ms (line 585-601)
- `sessionStart()`: ~30-50ms (line 193-228)

**Called from:**
- FIDO2 module: `components/mod_fido2/src/ctap2.cpp`
- GPG module: `components/mod_gpg/src/openpgp/openpgp.cpp`
- Main UI: Settings, key generation screens

## Recommended Fix

1. **Add async operation support**:
   ```cpp
   // Define callback for async operations
   typedef void (*EccSignCallback)(SeResult result, const uint8_t* sig, size_t sigLen);
   
   bool ecdsaSignAsync(uint8_t slot, const uint8_t* hash, size_t hashLen,
                       EccSignCallback callback);
   ```

2. **Offload to worker task**:
   ```cpp
   // Create dedicated secure element task
   void se_worker_task(void* param) {
       while (true) {
           SeOp op = se_queue.pop();
           SeResult result = performOperation(op);
           op.callback(result, op.data);
       }
   }
   ```

3. **Pre-compute signatures for predictable operations**:
   - Cache FIDO2 challenge signatures when possible
   - Pre-generate key pairs during idle time

4. **Add progress feedback**:
   - Show "Signing..." indicator during long operations
   - Update UI to show progress for multi-step operations

5. **Batch operations where possible**:
   - Queue multiple small R-memory operations
   - Combine ECC operations that can run sequentially

**Estimated effort**: 2-3 hours for async operation framework

## References

- TROPIC01 datasheet: ECC operations typically 20-100ms
- [libtropic documentation](https://github.com/TropicSquare/libtropic)
- Secure element operations are inherently blocking due to hardware constraints
