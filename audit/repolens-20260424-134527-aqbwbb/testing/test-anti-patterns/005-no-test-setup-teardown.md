---
title: "[MEDIUM] No test setup/teardown - state leaks between tests"
severity: MEDIUM
domain: testing
lens: test-anti-patterns
labels:
  - "test-structure"
---

## Summary
Tests have no setup or teardown functions. Module-level state in `vcard_store.cpp` (`g_own_vcard`, `g_cards`, `g_cards_loaded`, etc.) is not reset between tests, causing potential state leakage.

## Impact
- **State pollution**: If tests were run together, state from one test affects the next
- **Non-deterministic**: Test results may depend on execution order
- **Hard to debug**: Failures caused by previous tests are difficult to trace
- **Not idempotent**: Running the same test multiple times may give different results

## Evidence
Source file has global state that tests modify:
```cpp
// components/mod_vcard/src/vcard_store.cpp:17-20, 36-37
static char g_own_vcard[VCARD_MAX_LEN + ];
static bool g_own_loaded = false;
static bool g_own_present = false;
static vcard_meta_t g_cards[VCARD_MAX_CARDS];
static bool g_cards_loaded = false;
```

Test modifies this state but never resets it:
```cpp
// test/test_vcard_store/test_vcard_store.cpp:9-13
void test_vcard_validate() {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    vcard_store_set_own(v, strlen(v), err, sizeof(err));
    // No cleanup, state persists
}
```

## Recommended Fix
1. Add setup/teardown functions:
   ```cpp
   void test_setup() {
       // Reset global state
       memset(g_own_vcard, 0, sizeof(g_own_vcard));
       g_own_loaded = false;
       g_own_present = false;
       memset(g_cards, 0, sizeof(g_cards));
       g_cards_loaded = false;
       g_card_count = 0;
   }
   
   void test_teardown() {
       // Cleanup after test
       vcard_store_reset_all();  // If such a function exists, or reset manually
   }
   ```

2. Create a reset function in the module:
   ```cpp
   // In vcard_store.h
   void vcard_store_reset_all();  // Clear all stored data
   
   // In vcard_store.cpp
   void vcard_store_reset_all() {
       memset(g_own_vcard, 0, sizeof(g_own_vcard));
       g_own_loaded = false;
       g_own_present = false;
       memset(g_cards, 0, sizeof(g_cards));
       g_cards_loaded = false;
       g_card_count = 0;
   }
   ```

3. Use setup/teardown in test runner:
   ```cpp
   TEST_CASE("vcard store", "mod_vcard") {
       test_setup();  // Called before each test
       // ... test code ...
       test_teardown();  // Called after each test
   }
   ```

## References
- Test Fixtures: https://martinfowler.com/bliki/TestFixture.html
- ESP-IDF TEST_SETUP/TEST_TEARDOWN: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/unit-tests.html
