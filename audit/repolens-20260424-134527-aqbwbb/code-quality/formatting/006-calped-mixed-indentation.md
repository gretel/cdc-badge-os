---
title: "[MEDIUM] Inconsistent indentation: Tabs used in CalEPD vs spaces in main codebase"
severity: MEDIUM
domain: code-quality/formatting
lens: formatting-consistency
labels:
  - "audit:code-quality/formatting"
---

## Summary
The CalEPD component (`components/CalEPD/`) uses **tabs** for indentation while the rest of the codebase (cdc_core, cdc_hal, cdc_ui, mod_totp, etc.) uses **spaces** (4 spaces for indentation). This creates inconsistency across the codebase.

**Affected files:** 29+ files in `components/CalEPD/` with tabs for indentation

**Examples of affected files:**
- `components/CalEPD/models/gdem029E97.cpp` (37 lines with tabs)
- `components/CalEPD/models/gdew075T7Grays.cpp` (136 lines with tabs)
- `components/CalEPD/models/color/wave12i48BR.cpp` (64 lines with tabs)
- `components/CalEPD/models/goodisplay/gdeq037T31.cpp` (70 lines with tabs)
- `components/CalEPD/models/dke/depg750bn.cpp` (67 lines with tabs)
- `components/CalEPD/models/goodisplay/gdey027T91.cpp` (53 lines with tabs)
- And ~23 more files...

**Files using spaces (main codebase):**
- `components/cdc_core/src/ServiceRegistry.cpp`
- `components/cdc_hal/src/TCA9535Keypad.cpp`
- `components/mod_totp/src/TotpModule.cpp`
- `components/cdc_log/src/cdc_log.cpp`

## Impact
- **Consistency**: Mixed indentation styles make the codebase harder to navigate
- **Diff noise**: When CalEPD files are edited, the indentation style differs from the rest of the codebase
- **Formatter confusion**: Without a `.clang-format` or similar configuration, developers may inadvertently use the wrong indentation style
- **Code reviews**: Reviewers need to be aware of two different indentation conventions

## Evidence

**File: `components/CalEPD/models/gdem029E97.cpp`** (uses tabs)
```cpp
void GdeM029E97::drawPixel(int16_t x, int1_t y, uint16_t color) {
^I// Tab character for indentation
^Iif (x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT) return;
^I// More tab-indented code
}
```

**File: `components/cdc_core/src/ServiceRegistry.cpp`** (uses spaces)
```cpp
void ServiceRegistry::registerService(ServiceType type, IService* service) {
    // 4-space indentation
    services_[type] = service;
    // More space-indented code
}
```

**Tab count in CalEPD files:**
- `components/CalEPD/models/gdew075T7Grays.cpp`: 136 lines with tabs
- `components/CalEPD/models/color/wave12i48BR.cpp`: 64 lines with tabs
- `components/CalEPD/models/goodisplay/gdeq037T31.cpp`: 70 lines with tabs
- `components/CalEPD/models/dke/depg750bn.cpp`: 67 lines with tabs
- `components/CalEPD/models/goodisplay/gdey027T91.cpp`: 53 lines with tabs
- `components/CalEPD/models/custom/custom042.cpp`: 44 lines with tabs
- `components/CalEPD/models/goodisplay/gdey027T91.cpp`: 53 lines with tabs
- And many more...

## Recommended Fix

Since CalEPD is a component that may be updated or replaced, you have several options:

**Option 1 (Recommended for consistency)**: Convert CalEPD files to use 4-space indentation:
```bash
# Convert tabs to 4 spaces in CalEPD component
find components/CalEPD -type f \( -name "*.h" -o -name "*.cpp" \) | while read f; do
    sed -i 's/\t/    /g' "$f"
done
```

**Option 2**: Add a `.clang-format` file at the root that specifies 4-space indentation and run clang-format:
```yaml
# .clang-format
BasedOnStyle: Google
IndentWidth: 4
TabWidth: 4
UseTab: Never
```
Then run:
```bash
find components/CalEPD -type f \( -name "*.h" -o -name "*.cpp" \) | xargs clang-format -i
```

**Option 3**: Add an `.editorconfig` file to enforce consistent indentation:
```ini
# .editorconfig
[*.{h,cpp,c}]
indent_style = space
indent_size = 4
```

**Option 4**: If CalEPD is an external library that shouldn't be modified, document the exception and add a formatter configuration that only applies to the main codebase.

## References
- C++ Core Guidelines: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
- `.clang-format` documentation: https://clang.llvm.org/docs/ClangFormat.html
- `.editorconfig` specification: https://editorconfig.org/
- Google C++ Style Guide (4-space indentation): https://google.github.io/styleguide/cpp.html
