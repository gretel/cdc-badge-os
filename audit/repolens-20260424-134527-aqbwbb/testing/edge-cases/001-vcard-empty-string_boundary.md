---
title: "[MEDIUM] vCard validation lacks comprehensive empty string and boundary value tests"
severity: MEDIUM
domain: mod_vcard
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary
The `vcard_store` component in `components/mod_vcard/src/vcard_store.cpp` validates vCard input but the test at `test/test_vcard_store/test_vcard_store.cpp` only tests a single "happy path" case. Missing edge case coverage for:

1. **Empty string input** - `vcard_validate()` checks `len == 0` but tests don't verify behavior with `len = 0` explicitly
2. **Maximum length boundary** - `VCARD_MAX_LEN` (768 bytes) boundary not tested at exactly 768, 769 bytes
3. **Whitespace-only vCard** - vCard with only whitespace between structural elements
4. **Null terminator in middle** - The check `memchr(vcard, '\0', len)` exists but edge cases around embedded NUL bytes at boundaries not tested

**Evidence** (file:line):
- `vcard_store.cpp:288-313` - `vcard_validate()` function handles empty/null but tests don't cover edge cases
- `test_vcard_store.cpp:9-13` - Only one test with typical vCard, no edge cases

```cpp
// Current validation at vcard_store.cpp:288-290
if (!vcard || len == 0) {
    set_err(err, err_len, "Empty vCard");
    return false;
}
```

## Impact
- **Data corruption risk**: Edge cases like embedded NUL bytes could cause partial parsing
- **Silent failures**: Whitespace-only vCards might pass validation but produce empty metadata
- **Boundary overflow**: Without testing at exactly VCARD_MAX_LEN, off-by-one errors could slip through

## Evidence
Current test only covers happy path:
```cpp
// test_vcard_store.cpp:9-13
void test_vcard_validate() {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    vcard_store_set_own(v, strlen(v), err, sizeof(err));
}
```

Missing test cases:
- `vcard_store_set_own("", 0, err, sizeof(err))` - empty string
- `vcard_store_set_own(vcard, VCARD_MAX_LEN + 1, ...)` - exactly over limit
- `vcard_store_set_own("BEGIN:VCARD\n   \nEND:VCARD", ...)` - whitespace-only
- `vcard_store_add("BEGIN:VCARD\x00VERSION:4.0...", ...)` - embedded NUL

## Recommended Fix
Add edge case tests to `test/test_vcard_store/test_vcard_store.cpp`:

```cpp
void test_vcard_empty_input() {
    char err[64];
    // Test zero length
    bool result = vcard_store_set_own("Test", 0, err, sizeof(err));
    TEST_ASSERT_FALSE(result);
    
    // Test empty string
    result = vcard_store_set_own("", strlen(""), err, sizeof(err));
    TEST_ASSERT_FALSE(result);
}

void test_vcard_boundary_length() {
    char err[64];
    char exact_max[VCARD_MAX_LEN + 1];
    // Fill with valid vCard structure at exactly max length
    // Test at boundary
}

void test_vcard_whitespace_only() {
    char err[64];
    const char* whitespace_vcard = "BEGIN:VCARD\n   \n  \nEND:VCARD\n";
    bool result = vcard_store_set_own(whitespace_vcard, strlen(whitespace_vcard), err, sizeof(err));
    // Should either fail or handle gracefully
}

void test_vcard_embedded_null() {
    char err[64];
    const char* null_vcard = "BEGIN:VCARD\nVERSION:4.0\x00\nEND:VCARD\n";
    size_t len = 25; // Includes NUL in middle
    bool result = vcard_store_set_own(null_vcard, len, err, sizeof(err));
    TEST_ASSERT_FALSE(result);
}
```

## References
- C++ Edge Cases: Empty strings, boundary values
- vCard 4.0 RFC 6350 specification
- OWASP Input Validation cheat sheet
