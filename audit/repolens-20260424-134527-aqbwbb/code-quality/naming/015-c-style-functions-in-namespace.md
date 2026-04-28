---
title: "[LOW] C-style free functions in C++ namespace (vcard_store)"
severity: LOW
domain: naming-conventions
lens: code-quality
labels:
  - "audit:code-quality/naming"
---

## Summary
The `vcard_store.h` header uses C-style free functions instead of a C++ class or struct, with underscore-separated names that follow C conventions rather than C++ naming patterns.

**Evidence:**

**components/mod_vcard/include/mod_vcard/vcard_store.h** (lines 9-21):
```cpp
bool vcard_store_set_own(const char* vcard, size_t len, char* err, size_t err_len);
size_t vcard_store_get_own(char* out, size_t max_len);
size_t vcard_filter_empty_fields(char* vcard, size_t len);
bool vcard_store_has_own(void);
bool vcard_store_clear_own(void);
bool vcard_store_get_display_own(char* out, size_t max_len);
void vcard_store_init(void);
uint16_t vcard_store_count(void);
bool vcard_store_add(const char* vcard, size_t len, char* err, size_t err_len);
bool vcard_store_delete(uint16_t slot);
size_t vcard_store_get(uint16_t slot, char* out, size_t max_len);
bool vcard_store_get_display(uint16_t slot, camelCase out, size_t max_len);
uint16_t vcard_store_get_sorted(uint16_t* out_slots, uint16_t max_slots);
```

## Impact
- **Inconsistency**: Other modules use C++ classes (e.g., `PasswordStore`, `TotpStore`)
- **Naming verbosity**: `vcard_store_` prefix repeated in every function
- **Modern C++ patterns**: Doesn't leverage classes, methods, or RAII
- **Parameter naming**: Some functions use C-style `void` in parameter list

## Evidence
Comparison with similar modules:
- `PasswordStore` (mod_password): Uses C++ class with methods
- `TotpStore` (mod_totp): Uses C++ class with methods
- `vcard_store` (mod_vcard): Uses C-style functions with underscore naming

## Recommended Fix
Refactor to C++ class style for consistency with the rest of the codebase:

```cpp
class VcardStore {
public:
    static VcardStore& instance();

    bool setOwn(const char* vcard, size_t len);
    size_t getOwn(char* out, size_t max_len);
    size_t filterEmptyFields(char* vcard, size_t len);
    bool hasOwn() const;
    bool clearOwn();
    size_t getDisplayOwn(char* out, size_t max_len);
    void init();
    uint16_t count() const;
    bool add(const char* vcard, size_t len);
    bool deleteSlot(uint16_t slot);
    size_t get(uint16_t slot, char* out, size_t max_len);
    size_t getDisplay(uint16_t slot, char* out, size_t max_len);
    uint16_t getSorted(uint16_t* out_slots, uint16_t max_slots);

private:
    VcardStore() = default;
};
```

**Steps:**
1. Create `VcardStore` class in `components/mod_vcard/include/mod_vcard/VcardStore.h`
2. Move existing functions to be methods of `VcardStore`
3. Update call sites throughout the codebase
4. Keep C-style functions as static helpers if needed for internal use

## References
- [C++ Core Guidelines - Classes](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rclass-class)
- [Google C++ Style Guide - Classes](https://google.github.io/styleguide/cppguide.html#Classes_and_Data_Members)
