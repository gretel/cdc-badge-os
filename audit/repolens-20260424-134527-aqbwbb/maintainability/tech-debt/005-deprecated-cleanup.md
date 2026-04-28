---
title: "[MEDIUM] Deprecated code blocks should be cleaned up"
severity: MEDIUM
domain: maintainability
lens: tech-debt/deprecated
labels:
  - cleanup
  - deprecated
---

## Summary
Multiple deprecated code blocks and commented-out code exist in the CalEPD component that should either be removed or properly documented with reasons and dates.

## Impact
- **Code clutter**: Dead code makes the codebase harder to read
- **Confusion**: Developers may wonder if deprecated code is still needed
- **Maintenance**: Future maintainers may accidentally re-enable broken code
- **Build size**: Unused code increases firmware size

## Evidence

### Deprecated blocks found:

**1. epdspi.h - Deprecated method:**
File: `components/CalEPD/include/epdspi.h`, Line 21
```cpp
// Deprecated
```

**2. Multiple files - Commented original code:**
File: `components/CalEPD/models/color/gdeh042Z98.cpp`, Line 143
File: `components/CalEPD/models/color/gdeq042Z21.cpp`, Line 159
File: `components/CalEPD/models/color/gdeh042Z96.cpp`, Line 160
File: `components/CalEPD/models/color/gdeh042Z21.cpp`, Line 159
File: `components/CalEPD/models/gdew042t2.cpp`, Line 295
```cpp
// if (_initial) {  // --> Original deprecated
```

**3. gdew0213i5f.cpp - Deprecated methods:**
File: `components/CalEPD/models/gdew0213i5f.cpp`, Lines 295, 349
```cpp
printf("deprecated: updateWindow does not work\n");
printf("deprecated: updateToWindow does not work\n");
```

**4. gdep015OC1.cpp - Deprecated doc:**
File: `components/CalEPD/models/gdep015OC1.cpp`, Line 112
```cpp
* @deprecated It seems there is no need to do this for now
```

**5. heltec0151.cpp - Deprecated doc:**
File: `components/CalEPD/models/heltec0151.cpp`, Line 95
```cpp
* @deprecated It seems there is no need to do this for now
```

**6. epdspi.cpp - Deprecated method:**
File: `components/CalEPD/epdspi.cpp`, Line 170
```cpp
* @deprecated Not used at the moment
```

## Recommended Fix

### Step 1 - Review each deprecated block:
For each deprecated item, determine:
- Is it still needed? (keep with proper documentation)
- Can it be removed safely? (delete it)
- Should it be refactored? (create separate task)

### Step 2 - Remove confirmed dead code:
```bash
# Remove commented-out "original deprecated" blocks
# These are clearly not needed anymore
```

### Step 3 - Add proper deprecation markers:
For code that needs to stay but is deprecated:
```cpp
/**
 * \brief Old method, use newMethod() instead.
 * \deprecated Since version 1.0, replaced by newMethod().
 */
[[deprecated("Use newMethod() instead")]]
void oldMethod();
```

### Step 4 - Update documentation:
Add a DEPRECATION.md file in CalEPD component:
```markdown
# Deprecation Notes

## Removed in v2.0
- `updateWindow()` - Does not work on this display
- `updateToWindow()` - Does not work on this display

## To be removed
- `epdspi.cpp` method X - Not used, schedule for removal
```

## References
- C++17 `[[deprecated]]` attribute: https://en.cppreference.com/w/cpp/language/attributes/deprecated
- Doxygen @deprecated tag: https://www.doxygen.nl/manual/commands.html#cmddeprecated
