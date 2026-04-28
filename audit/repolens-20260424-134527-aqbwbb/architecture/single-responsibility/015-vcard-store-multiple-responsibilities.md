---
title: "[MEDIUM] vcard_store.cpp mixes storage, parsing, and formatting logic"
severity: MEDIUM
domain: mod_vcard
lens: single-responsibility
labels:
  - "audit:architecture/single-responsibility"
  - "component:mod_vcard"
---

## Summary
`components/mod_vcard/src/vcard_store.cpp` (699 lines) combines three distinct responsibilities:
1. **NV storage management** - Slot metadata, card loading/saving to NVS
2. **vCard parsing** - Line-by-line parsing, field extraction, duplicate detection
3. **vCard formatting** - String building, field concatenation, format validation

Key code evidence:
- Lines 1-50: NVS namespace setup, global storage state
- Lines 50-100: `fnv1a_hash()`, line trimming, content checking
- Lines 100-200: vCard line parsing, field extraction
- Lines 200-350: vCard formatting and string building
- Lines 350-500: Slot metadata management with NVS persistence
- Lines 500-650: Card search, hash-based duplicate detection

## Impact
**Maintainability**: Changing vCard format requires modifying parsing, storage, and formatting code together.

**Reusability**: Cannot use just the parser (e.g., for validation) without pulling in NVS code.

**Testability**: Testing parsing logic requires NVS initialization or complex mocking.

**Extensibility**: Adding new vCard fields requires changes across all three concerns.

## Evidence
File: `components/mod_vcard/src/vcard_store.cpp`

Lines 40-55 (Hash function):
```cpp
static uint32_t fnv1a_hash(const char* data, size_t len) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        hash ^= static_cast<uint8_t>(data[i]);
        hash *= 16777619u;
    }
    return hash;
}
```

Lines 65-100 (Line parsing):
```cpp
static void vcard_trim_cr(char* line) {
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
        line[len - 1] = '\0';
        len--;
    }
}

static bool vcard_line_has_content(const char* line, size_t len) {
    const char* colon = static_cast<const char*>(memchr(line, ':', len));
    // Field extraction logic
}
```

Lines 200-300 (Formatting):
```cpp
size_t vcard_store_get_own(char* out, size_t max_len) {
    // String building for vCard output
    snprintf(out, max_len, "BEGIN:VCARD\n");
    snprintf(out + len, max_len - len, "FN:%s\n", name);
    // More field formatting
}
```

Lines 350-450 (NV storage):
```cpp
void vcard_store_load_all() {
    nvs_open(VCARD_NAMESPACE, NVS_READONLY, &nvs);
    // Load slot metadata
    for (uint16_t i = 0; i < VCARD_MAX_CARDS; i++) {
        vcard_key_for_slot(key, sizeof(key), i);
        nvs_get_str(nvs, key, buffer, &len);
    }
}
```

## Recommended Fix
**Split into focused components** (each 1 hour task):

1. **Create `VcardParser` class**: Pure parsing logic:
   - `parseLine()`, `extractField()`
   - `validateVcard()`, `computeHash()`
   - No NVS dependencies

2. **Create `VcardFormatter` class**: Pure formatting logic:
   - `formatVcard()`, `formatField()`
   - Output buffer management
   - No NVS dependencies

3. **Create `VcardStore` class**: NVS storage only:
   - `loadCard()`, `saveCard()`
   - `loadAll()`, `deleteCard()`
   - Uses `VcardParser` and `VcardFormatter`

4. **Refactor `vcard_store.cpp`**: Keep only C API:
   - `vcard_store_get_own()`, `vcard_store_set_own()`
   - `vcard_store_load_all()`
   - Delegates to `VcardStore`

**Files to create**:
- `components/mod_vcard/include/mod_vcard/VcardParser.h`
- `components/mod_vcard/include/mod_vcard/VcardFormatter.h`
- `components/mod_vcard/include/mod_vcard/VcardStore.h`

**Migration steps**:
1. Create parser class, move parsing logic
2. Create formatter class, move formatting logic
3. Create store class, move NVS operations
4. Update C API to delegate to store

## References
- SRP: https://en.wikipedia.org/wiki/Single-responsibility_principle
- vCard format: https://tools.ietf.org/html/rfc6350
- Embedded string parsing: https://www.embedded.com/design/prototyping-and-development/4033491/Parsing-strings-in-embedded-systems
