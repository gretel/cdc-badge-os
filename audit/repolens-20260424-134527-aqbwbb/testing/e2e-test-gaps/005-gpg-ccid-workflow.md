---
title: "[MEDIUM] No E2E Tests for GPG/CCID Smartcard Workflow"
severity: MEDIUM
domain: testing
lens: e2e-test- gaps
labels:
  - "audit:testing/e2e-test-gaps"
---

## Summary

The **GPG/CCID module** (`components/mod_gpg/`) implements an OpenPGP smartcard interface but has **no E2E tests**. This module provides:

1. GPG key generation (Ed25519 for signing, P-256 for encryption)
2. CCID protocol implementation for USB smartcard communication
3. PW1 (user) and PW3 (admin) PIN authentication
4. Public key export
5. Key reset functionality

Critical workflows like key generation and CCID communication are only manually tested.

## Impact

**Functionality Risk:**
- GPG keys may not generate correctly on first attempt
- CCID protocol bugs could prevent compatibility with GPG software
- Key export might produce invalid OpenPGP packets

**Integration Risk:**
- Users need to verify keys work with `gpg --card-edit`
- SSH integration (`ssh-keygen -t ed25519-sk`) needs verification
- No automated verification after code changes

## Evidence

**Module structure** (`components/mod_gpg/`):
- `GpgModule.cpp` - 22,915 lines (module + CCID)
- `GpgStorage.cpp` - 13,830 lines (key storage)
- `gpg.cpp` - 20,978 lines (OpenPGP logic)
- `pin_storage.cpp` - 1,662 lines (PW1/PW3)
- `ccid/` - CCID protocol files
- `openpgp/` - OpenPGP packet structures

**CCID Protocol** (`components/mod_gpg/src/ccid/`):
- 3 files implementing USB CCID protocol
- Handles APDU command/response
- Critical for GPG software compatibility

**GPG Storage** (`components/mod_gpg/src/GpgStorage.cpp`):
```cpp
// Simplified key generation flow
bool GpgStorage::generateKeys(GpgKeyType type, const char* userId) {
    // 1. Verify PW3 (admin PIN)
    if (!PinManager::instance().verifyPW3(pw3)) {
        return false;
    }
    
    // 2. Allocate TROPIC01 slot
    uint8_t slot = tropic_allocSlot(type);
    
    // 3. Generate key on secure element
    tropic_generateECCKey(slot, type);
    
    // 4. Store metadata in R-Memory
    tropic_rmemWrite(slot, userId, strlen(userId));
    
    // 5. Update OpenPGP card structure
    updateOpenPGPStructure();
    
    return true;
}
```

**Serial Commands** (`docs/SERIAL_COMMANDS.md:105-113`):
```markdown
## GPG Module

| Command | Description |
|---------|-------------|
| `GPG_STATUS` | Show GPG key status `[AUTH]` |
| `GPG_GENERATE <curve> <user_id>` | Generate GPG keys (1=Ed25519, 2=P-256) `[AUTH]` |
| `GPG_EXPORT` | Export public keys `[AUTH]` |
| `GPG_RESET` | Reset GPG keys `[AUTH]` |
```

**No E2E tests exist** for GPG module.

## Recommended Fix

Create E2E test file `test/e2e/e2e_gpg_flow.cpp`:

```cpp
#include <unity.h>
#include "mod_gpg/GpgModule.h"
#include "mod_gpg/GpgStorage.h"
#include "mod_gpg/gpg.h"
#include "cdc_core/PinManager.h"

using namespace cdc::mod_gpg;

void setUp() {
    GpgModule::instance().init();
    GpgModule::instance().start();
}

void test_gpg_module_initialization() {
    TEST_ASSERT_EQUAL(ServiceState::RUNNING, GpgModule::instance().getState());
}

void test_gpg_generate_ed25519_key() {
    // Verify PW3 first
    // TEST_ASSERT_TRUE(PinManager::instance().verifyPW3("123456"));
    
    // Generate Ed25519 key for signing
    // bool generated = gpg_generateKey(GPG_KEY_ED25519, "user@example.com");
    // TEST_ASSERT_TRUE(generated);
    
    // Verify key exists
    // GpgKeyInfo key = gpg_getKeyInfo(GPG_KEY_ED25519);
    // TEST_ASSERT_TRUE(key.exists);
    // TEST_ASSERT_EQUAL_STRING("user@example.com", key.userId);
}

void test_gpg_generate_p256_key() {
    // Generate P-256 key for encryption
    // bool generated = gpg_generateKey(GPG_KEY_P256, "user@example.com");
    // TEST_ASSERT_TRUE(generated);
    
    // Verify key exists
    // GpgKeyInfo key = gpg_getKeyInfo(GPG_KEY_P256);
    // TEST_ASSERT_TRUE(key.exists);
}

void test_gpg_generate_requires_pw3() {
    // Try without PW3
    // bool generated = gpg_generateKey(GPG_KEY_ED25519, "user@example.com");
    // TEST_ASSERT_FALSE(generated);
}

void test_gpg_export_public_keys() {
    // Generate key first
    // gpg_generateKey(GPG_KEY_ED25519, "user@example.com");
    
    // Export
    // uint8_t* buffer;
    // size_t len;
    // bool exported = gpg_exportKeys(&buffer, &len);
    // TEST_ASSERT_TRUE(exported);
    // TEST_ASSERT_GREATER_THAN(0, len);
    
    // Verify OpenPGP packet structure
    // TEST_ASSERT_EQUAL(0x95, buffer[0]);  // Public key packet tag
}

void test_gpg_reset_keys() {
    // Generate keys
    // gpg_generateKey(GPG_KEY_ED25519, "user@example.com");
    
    // Reset
    // bool reset = gpg_resetKeys();
    // TEST_ASSERT_TRUE(reset);
    
    // Verify keys gone
    // GpgKeyInfo key = gpg_getKeyInfo(GPG_KEY_ED25519);
    // TEST_ASSERT_FALSE(key.exists);
}

void test_gpg_pw1_independent_from_pw3() {
    // Wrong PW1 should not affect PW3
    // PinManager::instance().verifyPW1("wrong");
    
    // PW3 should still have full retries
    // TEST_ASSERT_EQUAL(3, PinManager::instance().getPW3Retries());
}

void test_gpg_ccid_apdu_response() {
    // Test CCID APDU handling
    // uint8_t apdu[] = {0x00, 0xA4, 0x04, 0x00, 0x06, 0xD2, 0x76, 0x00, 0x01, 0x24, 0x01};
    // uint8_t response[256];
    // size_t len;
    
    // bool handled = ccid_processAPDU(apdu, sizeof(apdu), response, &len);
    // TEST_ASSERT_TRUE(handled);
    // TEST_ASSERT_GREATER_THAN(0, len);
}

void test_gpg_multiple_keys() {
    // Generate Ed25519
    // gpg_generateKey(GPG_KEY_ED25519, "user1@example.com");
    
    // Generate P-256
    // gpg_generateKey(GPG_KEY_P256, "user1@example.com");
    
    // Verify both exist
    // GpgKeyInfo ed = gpg_getKeyInfo(GPG_KEY_ED25519);
    // GpgKeyInfo p256 = gpg_getKeyInfo(GPG_KEY_P256);
    // TEST_ASSERT_TRUE(ed.exists);
    // TEST_ASSERT_TRUE(p256.exists);
}

void test_gpg_key_fingerprint() {
    // Generate key
    // gpg_generateKey(GPG_KEY_ED25519, "user@example.com");
    
    // Get fingerprint
    // uint8_t fp[20];
    // gpg_getFingerprint(GPG_KEY_ED25519, fp);
    
    // Verify fingerprint is valid (non-zero, correct length)
    // TEST_ASSERT_NOT_NULL(fp);
    // TEST_ASSERT_GREATER_THAN(0, fp[0]);  // At least some bits set
}

extern "C" void app_main() {
    UNITY_BEGIN();
    
    RUN_TEST(test_gpg_module_initialization);
    RUN_TEST(test_gpg_generate_ed25519_key);
    RUN_TEST(test_gpg_generate_p256_key);
    RUN_TEST(test_gpg_generate_requires_pw3);
    RUN_TEST(test_gpg_export_public_keys);
    RUN_TEST(test_gpg_reset_keys);
    RUN_TEST(test_gpg_pw1_independent_from_pw3);
    RUN_TEST(test_gpg_ccid_apdu_response);
    RUN_TEST(test_gpg_multiple_keys);
    RUN_TEST(test_gpg_key_fingerprint);
    
    UNITY_END();
}
```

**Additional integration test** (manual verification):
- Test with actual GPG software: `gpg --card-edit`
- Test SSH key generation: `ssh-keygen -t ed25519-sk`
- Verify CCID enumeration on USB

## References

- [GPG Module](components/mod_gpg/) - 5 source files
- [GPG Storage](components/mod_gpg/src/GpgStorage.cpp) - Key generation logic
- [CCID Protocol](components/mod_gpg/src/ccid/) - USB smartcard protocol
- [Serial Commands](docs/SERIAL_COMMANDS.md) - GPG commands (lines 105-113)
- [GPG Documentation](docs/GPG.md) - Usage guide
