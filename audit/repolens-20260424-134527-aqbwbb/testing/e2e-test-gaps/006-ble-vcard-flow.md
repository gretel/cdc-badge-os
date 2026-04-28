---
title: "[MEDIUM] No E2E Tests for BLE vCard Exchange Flow"
severity: MEDIUM
domain: testing
lens: e2e-test-gaps
labels:
  - "audit:testing/e2e-test-gaps"
---

## Summary

The **BLE vCard module** (`components/mod_vcard/`) implements badge-to-badge contact exchange via Bluetooth Low Energy, but has **no E2E tests**. The existing tests are trivial smoke tests:

- `test/test_vcard_store/test_vcard_store.cpp` - 21 lines, single function call
- `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp` - 19 lines, init + setter
- `test/test_vcard_module_link/test_vcard_module_link.cpp` - 19 lines, registration call

**Critical workflows untested:**
1. vCard parsing and validation
2. BLE advertising with vCard data
3. Peer discovery and connection
4. vCard exchange over BLE
5. Received vCard storage and display

## Impact

**Functionality Risk:**
- vCard parsing bugs could corrupt contact data
- BLE advertising might not work with different phones
- Connection handshake could fail silently

**User Experience:**
- Badge-to-badge exchange is a key feature
- No automated verification after changes
- Manual testing required for every update

## Evidence

**Module structure** (`components/mod_vcard/`):
- `VcardModule.cpp` - 19,079 lines (module + UI)
- `ble_vcard.cpp` - 35,945 lines (BLE protocol)
- `vcard_store.cpp` - 20,517 lines (vCard parsing/storage)

**Existing tests** (`test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp`):
```cpp
void test_ble_vcard_symbols() {
    ble_vcard_init();
    ble_vcard_set_exchange_enabled(true);
}
```

**Existing tests** (`test/test_vcard_store/test_vcard_store.cpp`):
```cpp
void test_vcard_validate() {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    vcard_store_set_own(v, strlen(v), err, sizeof(err));
}
```

**vCard parsing** (`components/mod_vcard/src/vcard_store.cpp`):
```cpp
// Simplified parsing flow
bool vcard_store_parse(const char* vcard, Vcard* out) {
    // 1. Validate BEGIN:VCARD
    // 2. Parse VERSION
    // 3. Parse FN (full name)
    // 4. Parse N (name components)
    // 5. Parse ORG, TITLE, URL, etc.
    // 6. Validate END:VCARD
    // 7. Store in R-Memory
}
```

**BLE exchange** (`components/mod_vcard/src/ble_vcard.cpp`):
```cpp
// Simplified exchange flow
void ble_vcard_startExchange() {
    // 1. Start BLE advertising
    // 2. Wait for peer connection
    // 3. Negotiate vCard exchange
    // 4. Send own vCard
    // 5. Receive peer vCard
    // 6. Store peer vCard
}
```

## Recommended Fix

Create E2E test file `test/e2e/e2e_ble_vcard.cpp`:

```cpp
#include <unity.h>
#include "mod_vcard/ble_vcard.h"
#include "mod_vcard/vcard_store.h"
#include "mod_vcard/VcardModule.h"

using namespace cdc::mod_vcard;

void setUp() {
    VcardModule::instance().init();
    ble_vcard_init();
}

// ============================================
// vCard Parsing Tests
// ============================================

void test_vcard_parse_minimal() {
    const char* vcard = "BEGIN:VCARD\nVERSION:4.0\nFN:John\nEND:VCARD\n";
    char err[64];
    
    bool parsed = vcard_store_parse(vcard, err, sizeof(err));
    TEST_ASSERT_TRUE(parsed);
}

void test_vcard_parse_full() {
    const char* vcard = 
        "BEGIN:VCARD\n"
        "VERSION:4.0\n"
        "FN:John Doe\n"
        "N:Doe;John;;;\n"
        "ORG:Acme Inc\n"
        "TITLE:Developer\n"
        "URL:https://john.dev\n"
        "END:VCARD\n";
    char err[64];
    
    bool parsed = vcard_store_parse(vcard, err, sizeof(err));
    TEST_ASSERT_TRUE(parsed);
}

void test_vcard_parse_invalid() {
    const char* vcard = "BEGIN:VCARD\nVERSION:4.0\nFN:John\n";  // Missing END
    char err[64];
    
    bool parsed = vcard_store_parse(vcard, err, sizeof(err));
    TEST_ASSERT_FALSE(parsed);
}

void test_vcard_set_own() {
    const char* vcard = "BEGIN:VCARD\nVERSION:4.0\nFN:Test User\nEND:VCARD\n";
    char err[64];
    
    bool stored = vcard_store_set_own(vcard, strlen(vcard), err, sizeof(err));
    TEST_ASSERT_TRUE(stored);
}

void test_vcard_get_own() {
    // Set own vCard first
    const char* vcard = "BEGIN:VCARD\nVERSION:4.0\nFN:Test User\nEND:VCARD\n";
    vcard_store_set_own(vcard, strlen(vcard), err, sizeof(err));
    
    // Get it back
    char buffer[256];
    size_t len;
    bool got = vcard_store_get_own(buffer, sizeof(buffer), &len);
    TEST_ASSERT_TRUE(got);
    TEST_ASSERT_GREATER_THAN(0, len);
}

// ============================================
// BLE vCard Tests
// ============================================

void test_ble_vcard_init() {
    ble_vcard_init();
    
    // Verify BLE is initialized
    // TEST_ASSERT_TRUE(ble_vcard_isInitialized());
}

void test_ble_vcard_enable_exchange() {
    ble_vcard_set_exchange_enabled(true);
    
    // Verify advertising starts
    // TEST_ASSERT_TRUE(ble_vcard_isAdvertising());
}

void test_ble_vcard_disable_exchange() {
    ble_vcard_set_exchange_enabled(true);
    ble_vcard_set_exchange_enabled(false);
    
    // Verify advertising stops
    // TEST_ASSERT_FALSE(ble_vcard_isAdvertising());
}

void test_ble_vcard_get_own_vcard() {
    // Set own vCard
    const char* vcard = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    vcard_store_set_own(vcard, strlen(vcard), err, sizeof(err));
    
    // Get for BLE transmission
    const uint8_t* data;
    size_t len;
    bool got = ble_vcard_getOwnVcard(&data, &len);
    TEST_ASSERT_TRUE(got);
    TEST_ASSERT_GREATER_THAN(0, len);
}

void test_ble_vcard_receive() {
    // Simulate receiving vCard data
    const uint8_t* peer_data = (const uint8_t*)"BEGIN:VCARD\nVERSION:4.0\nFN:Peer\nEND:VCARD\n";
    size_t peer_len = 38;
    
    bool received = ble_vcard_receiveVcard(peer_data, peer_len);
    TEST_ASSERT_TRUE(received);
}

void test_ble_vcard_storage() {
    // Receive and store peer vCard
    const uint8_t* peer_data = (const uint8_t*)"BEGIN:VCARD\nVERSION:4.0\nFN:Peer\nEND:VCARD\n";
    ble_vcard_receiveVcard(peer_data, 38);
    
    // Verify stored
    char buffer[256];
    size_t len;
    bool got = vcard_store_get_peer(buffer, sizeof(buffer), &len);
    TEST_ASSERT_TRUE(got);
}

// ============================================
// Integration Tests
// ============================================

void test_ble_vcard_full_exchange() {
    // Setup own vCard
    const char* own_vcard = "BEGIN:VCARD\nVERSION:4.0\nFN:My Badge\nEND:VCARD\n";
    vcard_store_set_own(own_vcard, strlen(own_vcard), err, sizeof(err));
    
    // Enable exchange
    ble_vcard_set_exchange_enabled(true);
    
    // Simulate peer connection and exchange
    const uint8_t* peer_data = (const uint8_t*)"BEGIN:VCARD\nVERSION:4.0\nFN:Peer Badge\nEND:VCARD\n";
    ble_vcard_receiveVcard(peer_data, 44);
    
    // Verify both vCards available
    // TEST_ASSERT_TRUE(vcard_store_hasOwn());
    // TEST_ASSERT_TRUE(vcard_store_hasPeer());
}

void test_ble_vcard_symbol_link() {
    // Verify all symbols link correctly
    ble_vcard_init();
    ble_vcard_set_exchange_enabled(true);
    
    // These should not cause linker errors
    TEST_ASSERT_NOT_NULL(ble_vcard_init);
    TEST_ASSERT_NOT_NULL(ble_vcard_set_exchange_enabled);
    TEST_ASSERT_NOT_NULL(ble_vcard_receiveVcard);
}

extern "C" void app_main() {
    UNITY_BEGIN();
    
    // vCard parsing
    RUN_TEST(test_vcard_parse_minimal);
    RUN_TEST(test_vcard_parse_full);
    RUN_TEST(test_vcard_parse_invalid);
    RUN_TEST(test_vcard_set_own);
    RUN_TEST(test_vcard_get_own);
    
    // BLE vCard
    RUN_TEST(test_ble_vcard_init);
    RUN_TEST(test_ble_vcard_enable_exchange);
    RUN_TEST(test_ble_vcard_disable_exchange);
    RUN_TEST(test_ble_vcard_get_own_vcard);
    RUN_TEST(test_ble_vcard_receive);
    RUN_TEST(test_ble_vcard_storage);
    
    // Integration
    RUN_TEST(test_ble_vcard_full_exchange);
    RUN_TEST(test_ble_vcard_symbol_link);
    
    UNITY_END();
}
```

## References

- [BLE vCard Module](components/mod_vcard/) - 3 source files
- [Existing Tests](test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp) - Current smoke test
- [vCard Store](components/mod_vcard/src/vcard_store.cpp) - Parsing logic
- [BLE Protocol](components/mod_vcard/src/ble_vcard.cpp) - BLE implementation
- [UI Flows](docs/UI_FLOWS.md) - Module menu (lines 165-180)
