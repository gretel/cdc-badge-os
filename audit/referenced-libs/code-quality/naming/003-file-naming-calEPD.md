---
title: "[MEDIUM] Inconsistent file naming conventions in CalEPD component"
severity: MEDIUM
domain: code-quality/naming
lens: naming-conventions
labels:
  - "audit:code-quality/naming"
---

## Summary

The CalEPD component uses **inconsistent file naming conventions** for display driver header files. The naming follows multiple patterns without a clear rule:

1. **Lowercase with mixed case suffix**: `gdep015OC1.h`, `gdem029E97.h`, `gdew075HD.h`, `gdew0213i5f.h`, `gdew042t2Grays.h`, `gdeh0154d67.h`
2. **Uppercase**: `ED047TC1.h`, `ED047TC1touch.h`, `ED060SC4.h`
3. **Lowercase**: `grayscales.h`

### Affected Files

| Directory | Files |
|-----------|-------|
| `components/CalEPD/include/` | `gdep015OC1.h`, `gdem029E97.h`, `gdew075HD.h`, `gdew0213i5f.h`, `gdew042t2Grays.h`, `gdew027w3.h`, `gdew027w3T.h`, `gdew042t2.h`, `gdew0583t7.h`, `gdew075T7.h`, `gdew075T7Grays.h`, `gdeh0154d67.h`, `gdeh0213b73.h` |
| `components/CalEPD/include/parallel/` | `ED047TC1.h`, `ED047TC1touch.h`, `ED060SC4.h`, `grayscales.h` |
| `components/CalEPD/include/color/` | `gdeh042Z21.h`, `gdeh042Z98.h`, `gdew0583z83.h`, `gdew075z09.h` |

## Impact

**Discoverability**: Developers searching for a specific display driver must know the exact casing convention.

**Build system complexity**: Case-sensitive file systems (Linux) vs. case-insensitive (Windows, macOS) may cause issues.

**Consistency**: The project's main components (cdc_core, cdc_ui, cdc_views) use consistent PascalCase file names matching class names (e.g., `ListView.h`, `ServiceRegistry.h`), but CalEPD (third-party) breaks this.

**Cross-references**: Include statements must match exact casing, making refactoring harder.

## Evidence

From the CalEPD include directory:
```
gdep015OC1.h      # lowercase prefix, mixed case suffix
gdem029E97.h      # lowercase prefix, mixed case suffix
gdew075HD.h       # lowercase prefix, uppercase suffix
gdew0213i5f.h     # all lowercase
ED047TC1.h        # all uppercase prefix
ED060SC4.h        # all uppercase
grayscales.h      # all lowercase
```

Comparison with project's own components:
```
components/cdc_views/include/cdc_views/ListView.h       # PascalCase
components/cdc_views/include/cdc_views/TimeInputView.h  # PascalCase
components/cdc_core/include/cdc_core/ServiceRegistry.h  # PascalCase
```

## Recommended Fix

**Option A (Recommended): Convert all to consistent PascalCase matching class names**

This requires renaming files to match their primary class:

| Current | Rename to | Class |
|---------|-----------|-------|
| `gdep015OC1.h` | `Gdep015OC1.h` | `Gdep015OC1` |
| `gdem029E97.h` | `Gdem029E97.h` | `Gdem029E97` |
| `gdew075HD.h` | `Gdew075HD.h` | `Gdew075HD` |
| `ED047TC1.h` | `Ed047TC1.h` | `Ed047TC1` |
| `ED060SC4.h` | `Ed060SC4.h` | `Ed060SC4` |

**Option B: Convert all to lowercase**

Simpler for cross-platform compatibility:
- `gdep015oc1.h`
- `gdem029e97.h`
- `ed047tc1.h`

Process:
1. Create a mapping of current to new file names
2. Rename files in batches (one directory at a time)
3. Update all `#include` statements across the codebase
4. Update any build system references (CMakeLists.txt, Kconfig)
5. Test compilation

**Scope**: This finding covers just the file naming. A related issue would cover updating all include statements.

## References

- [PlatformIO File Naming Conventions](https://docs.platformio.org/en/latest/projectconf/section_env_build.html)
- [File System Case Sensitivity](https://en.wikipedia.org/wiki/Case_sensitivity)
- [ESP32 Include Path Conventions](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html)

</content>