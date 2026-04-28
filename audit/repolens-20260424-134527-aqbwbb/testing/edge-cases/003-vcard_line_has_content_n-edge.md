---
title: "[LOW] vcard_line_has_content() edge case with N: field boundary conditions"
severity: LOW
domain: mod_vcard
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary
The `vcard_line_has_content()` function at `vcard_store.cpp:82-121` has complex logic for determining if a line has meaningful content, particularly for the `N:` (Name) field. Edge cases not tested:

1. **N: with only semicolons** - `"N:;;;"` vs `"N:;;\t \t"`
2. **N: with leading/trailing semicolons** - `";;Smith"` or `"Smith;;"`
3. **IMPP: with multiple colons** - `"IMPP:telegram:user@example.com"`
4. **Empty field with whitespace** - `"NOTE:   "` (3 spaces)
5. **Line with only prefix** - `"N:"` (nothing after colon)
6. **Case variations** - `"n:"` lowercase vs `"N:"` uppercase

**Evidence** (file:line):
- `vcard_store.cpp:82-121` - `vcard_line_has_content()` function
- `vcard_store.cpp:96-102` - Special handling for N: field

```cpp
// Special N: handling at vcard_store.cpp:96-102
if (len > 2 && (line[0] == 'N' && (line[1] == ':' || line[1] == ';'))) {
    bool has_real_content = false;
    for (size_t i = 0; i < val_len; i++) {
        if (val[i] != ';' && val[i] != ' ' && val[i] != '\t') {
            has_real_content = true;
            break;
        }
    }
    if (!has_real_content) return false;
}
```

The function checks for content after semicolons but doesn't test boundary conditions.

## Impact
- **Data quality**: vCards with only semicolons in N: field could be incorrectly filtered
- **Inconsistent behavior**: Edge cases might cause vCards to be accepted/rejected unpredictably

## Evidence
No tests exist for `vcard_line_has_content()` edge cases. The function is called from `vcard_filter_empty_fields()` at line 148.

## Recommended Fix
Add edge case tests:

```cpp
void test_vcard_n_field_edge_cases() {
    // Test N: with only semicolons
    bool result = vcard_line_has_content("N:;;;", 5);
    TEST_ASSERT_FALSE(result);
    
    // Test N: with semicolons and whitespace
    result = vcard_line_has_content("N:;; \t ", 8);
    TEST_ASSERT_FALSE(result);
    
    // Test N: with actual content
    result = vcard_line_has_content("N:;Smith", 8);
    TEST_ASSERT_TRUE(result);
    
    // Test N: empty
    result = vcard_line_has_content("N:", 2);
    TEST_ASSERT_FALSE(result);
    
    // Test IMPP: with multiple colons
    result = vcard_line_has_content("IMPP:telegram:user", 18);
    TEST_ASSERT_TRUE(result);
    
    // Test IMPP: empty value
    result = vcard_line_has_content("IMPP:telegram:", 14);
    TEST_ASSERT_FALSE(result);
}
```

## References
- vCard 4.0 RFC 6350 - N field specification
- Edge case testing for string parsing functions
