---
title: "[HIGH] GPG module public API lacks unit test coverage"
severity: HIGH
domain: mod_gpg
lens: unit-test-gaps
labels:
  - "audit:testing/unit-test-gaps"
---

## Summary
The `mod_gpg` component exposes a comprehensive public API for OpenPGP key management in `components/mod_gpg/include/mod_gpg/gpg.h` with 12 public functions, but has **zero unit test coverage**. The existing test files in `/input/20260423-132359-oj8ayc/cdc-badge-os/test/` only cover vCard module linking and basic validation, not the GPG functionality.

**Untested Public Functions** (from `gpg.h` lines 51-66):
- `gpg_init()` - Module initialization
- `gpg_is_initialized()` - Status check
- `gpg_get_status()` - Retrieve key status metadata
- `gpg_set_pending_user_id()` - Set user ID for key generation
- `gpg_has_pending_user_id()` - Check pending user ID
- `gpg_generate_key(uint8_t curve)` - Generate ECC key (P256 or ED25519)
- `gpg_reset()` - Reset key state
- `gpg_export_pubkey_pem()` - Export public key in PEM format
- `gpg_export_pubkey_raw()` - Export raw public key bytes
- `gpg_get_fingerprint()` - Get 20-byte fingerprint
- `gpg_get_fingerprint_v5()` - Get 32-byte v5 fingerprint
- `gpg_sign_hash()` - Sign a hash with the private key

Additionally, the `GpgModule` class in `GpgModule.h` has untested lifecycle methods (`init()`, `start()`, `stop()`, `getMenuItems()`, `getSlotRequest()`, `setSlotRange()`).

## Impact
**Security Risk**: The GPG module handles cryptographic key generation and signing - core security functions that must be verified. Without tests:
- Key generation edge cases may fail silently
- Fingerprint calculation bugs could cause key identification errors
- Signature generation may produce invalid results
- Memory corruption in PEM export could leak data
- No regression guard for future changes

**Maintenance Cost**: Developers cannot safely refactor GPG code without test validation, leading to technical debt accumulation.

## Evidence
**File**: `components/mod_gpg/include/mod_gpg/gpg.h` (lines 51-66)
```c
bool gpg_init(void);
bool gpg_is_initialized(void);
bool gpg_get_status(gpg_status_t *status);
bool gpg_set_pending_user_id(const char *user_id);
bool gpg_has_pending_user_id(void);
bool gpg_generate_key(uint8_t curve);
bool gpg_reset(void);
bool gpg_export_pubkey_pem(char *buf, size_t size, size_t *out_len);
bool gpg_export_pubkey_raw(uint8_t *pubkey, size_t *pubkey_len, uint8_t *curve);
bool gpg_get_fingerprint(uint8_t *fp_out);
bool gpg_get_fingerprint_v5(uint8_t *fp_out);
bool gpg_sign_hash(const uint8_t *hash, size_t hash_len,
                   uint8_t *sig_out, size_t *sig_len);
```

**File**: `components/mod_gpg/include/mod_gpg/GpgModule.h` (lines 10-18)
```cpp
bool init() override;
bool start() override;
void stop() override;
uint8_t getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) override;
core::IModule::SlotRequest getSlotRequest() const override;
void setSlotRange(const core::IModule::SlotRange& range) override;
```

**Existing Tests**: Only 3 smoke tests exist in `/test/` directory:
- `test_vcard_module_link.cpp` - Link test only
- `test_vcard_store.cpp` - Basic vCard validation
- `test_ble_vcard_symbols.cpp` - BLE symbol test

No tests reference `gpg.h` or `GpgModule.h`.

## Recommended Fix
Create unit test file `test/test_gpg_module/test_gpg.cpp` with test cases for:

1. **Initialization tests**:
   - Test `gpg_init()` returns true on success
   - Test `gpg_is_initialized()` returns correct state
   - Test multiple init calls don't cause issues

2. **Key generation tests**:
   - Test `gpg_generate_key(CDC_CURVE_P256)` succeeds
   - Test `gpg_generate_key(CDC_CURVE_ED25519)` succeeds
   - Test `gpg_get_status()` returns valid key data after generation
   - Test fingerprint is 20 bytes for P256, 32 bytes for ED25519

3. **User ID tests**:
   - Test `gpg_set_pending_user_id()` accepts valid user IDs
   - Test `gpg_has_pending_user_id()` returns correct state
   - Test user ID persistence through key generation

4. **Export tests**:
   - Test `gpg_export_pubkey_pem()` produces valid PEM format
   - Test `gpg_export_pubkey_raw()` returns correct key length
   - Test buffer overflow handling with small output buffers

5. **Signature tests**:
   - Test `gpg_sign_hash()` produces valid 64-byte signature
   - Test signature verification with known hash
   - Test with different hash sizes (20, 32, 64 bytes)

6. **Reset tests**:
   - Test `gpg_reset()` clears key data
   - Test status returns initialized state after reset

7. **Module lifecycle tests**:
   - Test `GpgModule::init()`, `start()`, `stop()` state transitions
   - Test `getMenuItems()` returns correct count
   - Test `getSlotRequest()` returns valid slot range

**Estimated effort**: ~1 hour for basic test structure and initialization/key generation tests. Additional hour for export/signature tests.

## References
- OpenPGP specification (RFC 4880): https://www.rfceditor.org/rfc/rfc4880
- ECC curves for OpenPGP: https://tools.ietf.org/html/draft-ietf-openpgp-rfc4880bis-11
- Similar test patterns: `test/test_vcard_store/test_vcard_store.cpp`
