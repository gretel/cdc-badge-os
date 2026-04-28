---
title: "[MEDIUM] Inconsistent boolean member variable naming in CalEPD components"
severity: MEDIUM
domain: code-quality/naming
lens: naming-conventions
labels:
  - "audit:code-quality/naming"
---

## Summary

Boolean member variables in the CalEPD display driver components follow **inconsistent naming conventions** across multiple header files. The codebase mixes:

1. **Underscore prefix style**: `_using_partial_mode`, `_initial`, `_debug_buffer`, `_tempalert`, `_mono_mode`, `_partial_mode`
2. **Snake_case without prefix**: `debug_enabled`, `colors_supported`
3. **CamelCase without prefix**: `color`

### Affected Files

| File | Inconsistent Members |
|------|---------------------|
| `components/CalEPD/include/epdParallel.h:45-46` | `_using_partial_mode` vs `debug_enabled` |
| `components/CalEPD/include/gdep015OC1.h:46-48` | `color`, `_initial`, `_debug_buffer` |
| `components/CalEPD/include/gdew075HD.h:53-54` | `_using_partial_mode`, `_initial` |
| `components/CalEPD/include/gdew0213i5f.h:57` | `_initial` |
| `components/CalEPD/include/wave12i48.h:39,57` | `colors_supported`, `_initial` |
| `components/CalEPD/include/gdem029E97.h:58` | `debug_enabled` |
| `components/CalEPD/include/gdew042t2Grays.h:50,54-55` | `_mono_mode`, `_initial`, `_partial_mode` |
| `components/CalEPD/include/parallel/ED047TC1.h:49-51` | `_tempalert`, `_initial`, `_debug_buffer` |
| `components/CalEPD/include/parallel/ED047TC1touch.h:57-59` | `color`, `_initial`, `_debug_buffer` |
| `components/CalEPD/include/parallel/ED060SC4.h:47-49` | `_tempalert`, `_initial`, `_debug_buffer` |

## Impact

**Maintainability burden**: Developers adding new boolean members must guess which convention to follow, leading to continued inconsistency.

**Readability**: Mixed conventions make it harder to quickly identify member variables vs. local variables or methods.

**Code search**: Finding all boolean members requires multiple search patterns (`_`, `enabled`, `color`, etc.).

## Evidence

From `components/CalEPD/include/epdParallel.h:45-46`:
```cpp
bool _using_partial_mode = false;  // underscore prefix
bool debug_enabled = true;         // snake_case, no prefix
```

From `components/CalEPD/include/gdep015OC1.h:46-48`:
```cpp
bool color = false;           // camelCase, no prefix
bool _initial = true;         // underscore prefix
bool _debug_buffer = false;   // underscore prefix
```

From `components/CalEPD/include/wave12i48.h:39`:
```cpp
bool colors_supported = 1;    // snake_case, no prefix
```

## Recommended Fix

Choose **one** consistent convention for boolean member variables and apply it across all CalEPD files. Recommended options:

**Option A (Recommended): Underscore suffix for members**
```cpp
bool using_partial_mode_ = false;
bool debug_enabled_ = true;
bool color_ = false;
bool initial_ = true;
```

**Option B: Underscore prefix for members**
```cpp
bool _using_partial_mode = false;
bool _debug_enabled = true;
bool _color = false;
bool _initial = true;
```

**Option C: No prefix, but consistent snake_case**
```cpp
bool using_partial_mode = false;
bool debug_enabled = true;
bool color = false;
bool initial = true;
```

Process:
1. Pick one convention (Option A is common in ESP32/PlatformIO projects)
2. Update all affected files in a single refactoring pass
3. Search for any remaining inconsistent patterns to ensure completeness

## References

- [Google C++ Style Guide - Naming](https://google.github.io/styleguide/cppguide.html#Naming)
- [ESP-IDF Coding Style](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/code-style.html)
- [Clean Code: Boolean variable naming](https://cleancode.uservoice.com/forums/178663-clean-code-articles/suggestions/4658881-booleans-should-be-named-with-is-has-can-etc)

</content>