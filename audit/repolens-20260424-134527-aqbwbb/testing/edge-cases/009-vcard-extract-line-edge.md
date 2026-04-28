---
title: "[LOW] vcard_extract_line() edge cases for missing/empty values"
severity: LOW
domain: mod_vcard
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary
The `vcard_extract_line()` function at `vcard_store.cpp:166-197` extracts values from vCard lines but lacks edge case tests:

1. **Empty value** - `"FN:"` with nothing after colon
2. **Value with only spaces** - `"FN:   "`
3. **Missing prefix** - Searching for `"XYZ:"` in vCard without that field
4. **Multiple matching lines** - First vs last occurrence
5. **Null input pointers** - `vcard = nullptr`, `prefix = nullptr`, `out = nullptr`
6. **Zero-length output buffer** - `out_len = 0`
7. **Multi-line values** - vCard folded lines (lines starting with space)
8. **Special characters in value** - `"FN:O'Brien"` or `"FN:Jean-Luc"`

**Evidence** (file:line):
- `vcard_store.cpp:166-197` - `vcard_extract_line()` function
- `vcard_store.cpp:206-266` - `vcard_parse_names()` uses this function

```cpp
// vcard_extract_line at vcard_store.cpp:166-197
static bool vcard_extract_line(const char* vcard, const char* prefix, char* out, size_t out_len) {
    if (!vcard || !prefix || !out || out_len == 0) return false;
    size_t prefix_len = strlen(prefix);
    const char* p = vcard;
    while (*p) {
        const char* line_start = p;
        const char* line_end = strpbrk(p, "\r\n");
        size_t line_len = line_end ? static_cast<size_t>(line_end - line_start) : strlen(line_start);

        if (line_len >= prefix_len && strncmp(line_start, prefix, prefix_len) == 0) {
            size_t copy_len = line_len - prefix_len;
            if (copy_len >= out_len) copy_len = out_len - 1;
            memcpy(out, line_start + prefix_len, copy_len);
            out[copy_len] = '\0';
            vcard_trim_cr(out);
            return true;
        }

        if (!line_end) break;
        p = line_end + 1;
        if (*p == '\n') p++;
    }
    return false;
}
```

The function handles null pointers but doesn't test edge cases like empty values or multi-line vCards.

## Impact
- **Data loss**: Folded vCard lines might not be parsed correctly
- **Silent truncation**: Long values truncated to buffer size without warning
- **Edge case bugs**: Empty values might be treated as "not found"

## Evidence
No tests exist for `vcard_extract_line()` edge cases.

## Recommended Fix
Add edge case tests:

```cpp
void test_vcard_extract_empty_value() {
    const char* vcard = "BEGIN:VCARD\nFN:\nEND:VCARD\n";
    char out[32];
    bool found = vcard_extract_line(vcard, "FN:", out, sizeof(out));
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_vcard_extract_spaces_only() {
    const char* vcard = "BEGIN:VCARD\nFN:   \nEND:VCARD\n";
    char out[32];
    bool found = vcard_extract_line(vcard, "FN:", out, sizeof(out));
    TEST_ASSERT_TRUE(found);
    // Should contain spaces or be trimmed
}

void test_vcard_extract_missing_field() {
    const char* vcard = "BEGIN:VCARD\nFN:Test\nEND:VCARD\n";
    char out[32];
    bool found = vcard_extract_line(vcard, "TEL:", out, sizeof(out));
    TEST_ASSERT_FALSE(found);
}

void test_vcard_extract_null_pointers() {
    char out[32];
    
    bool found = vcard_extract_line(nullptr, "FN:", out, sizeof(out));
    TEST_ASSERT_FALSE(found);
    
    found = vcard_extract_line("FN:Test", nullptr, out, sizeof(out));
    TEST_ASSERT_FALSE(found);
    
    found = vcard_extract_line("FN:Test", "FN:", nullptr, sizeof(out));
    TEST_ASSERT_FALSE(found);
    
    found = vcard_extract_line("FN:Test", "FN:", out, 0);
    TEST_ASSERT_FALSE(found);
}

void test_vcard_extract_long_value() {
    // Create vCard with very long name
    char vcard[512];
    snprintf(vcard, sizeof(vcard), "BEGIN:VCARD\nFN:%s\nEND:VCARD\n", 
             "VeryLongNameThatExceedsThirtyCharacters");
    char out[16]; // Small buffer
    bool found = vcard_extract_line(vcard, "FN:", out, sizeof(out));
    TEST_ASSERT_TRUE(found);
    // Should be truncated to 14 chars + null
    TEST_ASSERT_EQUAL(14, strlen(out));
}

void test_vcard_extract_special_chars() {
    const char* vcard = "BEGIN:VCARD\nFN:O'Brien\nEND:VCARD\n";
    char out[32];
    bool found = vcard_extract_line(vcard, "FN:", out, sizeof(out));
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_STRING("O'Brien", out);
}
```

## References
- vCard 4.0 line folding specification
- String extraction edge cases
