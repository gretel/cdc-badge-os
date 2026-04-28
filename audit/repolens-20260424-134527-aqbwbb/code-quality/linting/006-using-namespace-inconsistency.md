---
title: "[LOW] Inconsistent use of `using namespace` directives"
severity: LOW
domain: linting
lens: code-quality/linting
labels:
  - "audit:code-quality/linting"
---

## Summary
The codebase has inconsistent use of `using namespace` directives. Some files use them at file scope, others use them inside functions, and some don't use them at all. This affects code readability and consistency.

**Location:** Multiple component files

## Impact
- **Readability**: `using namespace` can make it harder to identify where types come from
- **Consistency**: Different files have different styles, making the codebase feel less cohesive
- **Potential conflicts**: File-scope `using namespace` can cause name collisions in larger files

## Evidence
Inconsistent patterns found:

**File-scope using (mod_nvsedit):**
```cpp
// components/mod_nvsedit/src/NvsEditModule.cpp
using namespace cdc::ui;
using namespace cdc::core;
```

**Function-scope using (mod_hid):**
```cpp
// components/mod_hid/src/BleHidKeyboard.cpp
    using namespace hal;
```

**Mix in same file (main.cpp):**
```cpp
// components/main/main.cpp
static const char* TAG = "BOOT";

using namespace cdc::core;  // File-scope, after variable declaration
```

The C++ Core Guidelines recommend either:
- Using fully qualified names consistently (`cdc::core::ServiceRegistry`)
- Or using `using` declarations for specific types (`using cdc::core::ServiceRegistry`)
- Avoid `using namespace` at file scope

## Recommended Fix
Choose a consistent style and apply it across the codebase:

**Option A (Recommended): Use fully qualified names**
- Remove all `using namespace` directives
- Use explicit namespace qualification

**Option B: Use specific using declarations**
```cpp
// Instead of: using namespace cdc::core;
using cdc::core::ServiceRegistry;
using cdc::core::EventBus;
```

**Option C: Keep file-scope but consistent**
- Move all `using namespace` to top of file (after includes)
- Document the convention in a style guide

For an embedded codebase with clear namespaces, **Option A** is recommended for maximum clarity.

## References
- [C++ Core Guidelines - Using](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-using)
- [Google C++ Style Guide - Namespaces](https://google.github.io/styleguide/cppguide.html#Namespaces)
