---
title: "[MEDIUM] No test data builders - inline test data duplication"
severity: MEDIUM
domain: testing
lens: test-anti-patterns
labels:
  - "test-structure"
---

## Summary
Test data is hardcoded inline in each test function without any builders or factories. The vCard string in `test_vcard_store.cpp` is a minimal example that doesn't test edge cases.

## Impact
- **Limited coverage**: Only one hardcoded test case, no variation
- **Hard to extend**: Adding new test cases requires duplicating the inline data structure
- **Fragile**: Changing test data format requires updating multiple places
- **No edge cases**: No tests for invalid vCards, boundary conditions, or error paths

## Evidence
```cpp
// test/test_vcard_store/test_vcard_store.cpp:11-12
const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
vcard_store_set_own(v, strlen(v), err, sizeof(err));
```

This tests only one specific valid vCard format. No tests for:
- Invalid vCards (missing BEGIN, END, VERSION)
- Oversized vCards (exceeding VCARD_MAX_LEN)
- Empty vCards
- vCards with special characters
- Multiple vCards

## Recommended Fix
1. Create test data builders or factories:
   ```cpp
   // In test_helper.h
   struct VcardBuilder {
       static std::string valid() {
           return "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
       }
       static std::string withName(const char* name) {
           char buf[256];
           snprintf(buf, sizeof(buf), "BEGIN:VCARD\nVERSION:4.0\nFN:%s\nEND:VCARD\n", name);
           return buf;
       }
       static std::string invalid() {
           return "BEGIN:VCARD\nFN:Test\n";  // Missing VERSION and END
       }
   };
   ```

2. Use builders in tests:
   ```cpp
   void test_valid_vcard() {
       char err[64];
       vcard_store_set_own(VcardBuilder::valid().c_str(), ...);
       assert(err[0] == '\0');
   }
   
   void test_missing_version() {
       char err[64];
       vcard_store_set_own(VcardBuilder::invalid().c_str(), ...);
       assert(strcmp(err, "INVALID") == 0);
   }
   ```

## References
- Test Data Builders: https://www.martinfowler.com/bliki/TestDataBuilder.html
- Builder Pattern for Tests: https://dzone.com/articles/test-data-builders-cleaner
