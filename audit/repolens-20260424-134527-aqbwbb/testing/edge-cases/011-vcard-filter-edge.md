---
title: "[LOW] vcard_filter_empty_fields() edge cases for buffer overflow"
severity: LOW
domain: mod_vcard
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary
The `vcard_filter_empty_fields()` function at `vcard_store.cpp:129-163` filters vCard fields but has potential edge cases:

1. **Input at exactly VCARD_MAX_LEN** - No room for filtering
2. **Input larger than VCARD_MAX_LEN** - Should be caught by caller but not checked
3. **All fields filtered** - Result is empty string
4. **Very long single line** - Line exceeds `result` buffer
5. **Output buffer overflow check** - `out_pos + line_len + 1 < sizeof(result)` might be off-by-one
6. **Null input** - `vcard = nullptr`
7. **Zero length input** - `len = 0`

**Evidence** (file:line):
- `vcard_store.cpp:129-163` - `vcard_filter_empty_fields()` function
- `vcard_store.h:6` - `#define VCARD_MAX_LEN 768`

```cpp
// vcard_filter_empty_fields at vcard_store.cpp:129-163
size_t vcard_filter_empty_fields(char* vcard, size_t len) {
    char result[VCARD_MAX_LEN + 1];
    size_t out_pos = 0;

    const char* p = vcard;
    const char* end = vcard + len;

    while (p < end) {
        // ... process lines ...
        
        if (keep && out_pos + line_len + 1 < sizeof(result)) {
            memcpy(result + out_pos, line_start, line_len);
            out_pos += line_len;
            result[out_pos++] = '\n';
        }
        // ...
    }

    result[out_pos] = '\0';
    memcpy(vcard, result, out_pos + 1);
    return out_pos;
}
```

The check at line 152 `out_pos + line_len + 1 < sizeof(result)` uses strict less than, which means at exactly `sizeof(result) - 1` it would still fit. But this is complex and error-prone.

## Impact
- **Silent truncation**: Large vCards might be silently truncated
- **Buffer overflow**: If check is wrong, could overflow `result` buffer
- **Data loss**: All fields filtered results in empty vCard

## Evidence
No tests exist for `vcard_filter_empty_fields()` edge cases. Called from `vcard_store_set_own()` and `vcard_store_add()`.

## Recommended Fix
Add edge case tests:

```cpp
void test_vcard_filter_at_max_length() {
    // Create vCard at exactly VCARD_MAX_LEN
    char exact[VCARD_MAX_LEN + 1];
    memset(exact, 'A', VCARD_MAX_LEN);
    exact[VCARD_MAX_LEN] = '\0';
    
    size_t len = vcard_filter_empty_fields(exact, VCARD_MAX_LEN);
    // Should handle gracefully, might truncate
    TEST_ASSERT_TRUE(len <= VCARD_MAX_LEN);
}

void test_vcard_filter_all_empty() {
    // vCard with only empty fields
    const char* vcard = "BEGIN:VCARD\nVERSION:4.0\nN:;;\nFN:\nEND:VCARD\n";
    char buf[256];
    strncpy(buf, vcard, sizeof(buf) - 1);
    
    size_t len = vcard_filter_empty_fields(buf, strlen(vcard));
    // Should keep structural lines (BEGIN, VERSION, END)
    TEST_ASSERT_TRUE(len > 0);
}

void test_vcard_filter_very_long_line() {
    // vCard with extremely long single line
    char vcard[1024];
    snprintf(vcard, sizeof(vcard), "BEGIN:VCARD\nFN:%s\nEND:VCARD\n", 
             "VeryLongNameThatExceedsNormalLengthByFarAndShouldBeTested");
    
    size_t len = vcard_filter_empty_fields(vcard, strlen(vcard));
    TEST_ASSERT_TRUE(len <= VCARD_MAX_LEN);
}

void test_vcard_filter_null_input() {
    size_t len = vcard_filter_empty_fields(nullptr, 10);
    // Should handle gracefully or crash - need to define behavior
}

void test_vcard_filter_zero_length() {
    char vcard[] = "BEGIN:VCARD\nEND:VCARD\n";
    size_t len = vcard_filter_empty_fields(vcard, 0);
    TEST_ASSERT_EQUAL(0, len);
}

void test_vcard_filter_preserves_structure() {
    const char* vcard = "BEGIN:VCARD\nVERSION:4.0\nN:Smith;John;;\nFN:John Smith\nNOTE:Test\nEND:VCARD\n";
    char buf[256];
    strncpy(buf, vcard, sizeof(buf) - 1);
    
    size_t len = vcard_filter_empty_fields(buf, strlen(vcard));
    
    // Should contain BEGIN, VERSION, END
    TEST_ASSERT_TRUE(strstr(buf, "BEGIN:VCARD") != nullptr);
    TEST_ASSERT_TRUE(strstr(buf, "END:VCARD") != nullptr);
}
```

## References
- Buffer overflow edge cases
- String filtering algorithms
