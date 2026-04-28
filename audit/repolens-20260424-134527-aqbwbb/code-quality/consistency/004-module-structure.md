---
title: "[MEDIUM] Module Structure Inconsistency: Mixed file organization patterns"
severity: MEDIUM
domain: architecture
lens: consistency
labels:
  - "audit:code-quality/consistency"
  - "module-structure"
---

## Summary

Module components show inconsistent file organization patterns:

**Standard pattern (most modules):**
```
components/<module>/
├── include/<module>/
│   └── <Module>Module.h
└── src/
    └── <Module>Module.cpp
```

**Inconsistent patterns found:**

1. **CalEPD**: Flat structure at component level
   ```
   components/CalEPD/
   ├── epd.cpp
   ├── epdspi.cpp
   ├── epd7color.cpp
   ├── include/
   │   ├── epd.h
   │   └ (no module namespace folder)
   └── models/
       ├── wave12i48.cpp
       └── color/
           └── wave5i7Color.cpp
   ```

2. **usb_badge**: Flat .cpp files at component level
   ```
   components/usb_badge/
   ├── usb_cdc.cpp  (should be in src/)
   ├── usb_hid.cpp  (should be in src/)
   ├── usb_descriptors.h
   └── include/usb_badge/
       ├── usb_cdc.h
       └── usb_hid.h
   ```

3. **mod_gpg**: Mixed depth structure
   ```
   components/mod_gpg/
   ├── include/mod_gpg/
   │   ├── GpgModule.h
   │   ├── GpgStorage.h
   │   ├── ccid/
   │   │   └── (empty?)
   │   └── openpgp/
   │       ├── apdu.h
   │       ├── ccid.h
   │       └── openpgp.h
   └── src/
       ├── GpgModule.cpp
       ├── GpgStorage.cpp
       ├── gpg.cpp
       ├── ccid/
       │   └── ccid_driver.cpp
       └── openpgp/
           ├── apdu.cpp
           ├── ccid.cpp
           ├── ecdh.cpp
           └── openpgp.cpp
   ```

## Impact

1. **Discoverability**: Harder to find files when structure varies
2. **Build complexity**: CMakeLists.txt needs exceptions for non-standard layouts
3. **Onboarding friction**: New contributors confused by varying patterns
4. **Refactoring difficulty**: Moving files requires understanding each module's unique structure

## Evidence

**Standard modules:**
```
components/mod_totp/
├── include/mod_totp/
│   ├── TotpModule.h
│   └── TotpStore.h
└── src/
    ├── TotpModule.cpp
    └── TotpStore.cpp
```

**Non-standard modules:**

usb_badge (flat .cpp files):
```bash
ls components/usb_badge/*.cpp
components/usb_badge/usb_cdc.cpp
components/usb_badge/usb_hid.cpp
```

CalEPD (no src/ folder, models/ subdirectory):
```bash
ls components/CalEPD/*.cpp
components/CalEPD/epd.cpp
components/CalEPD/epdspi.cpp
ls components/CalEPD/models/
# 50+ model files
```

mod_gpg (deep nesting):
```bash
ls components/mod_gpg/include/mod_gpg/
# GpgModule.h, GpgStorage.h, ccid/, openpgp/
ls components/mod_gpg/src/
# GpgModule.cpp, GpgStorage.cpp, gpg.cpp, ccid/, openpgp/
```

## Recommended Fix

**Phase 1: Document the standard pattern**

Add to `CLAUDE.md`:
```markdown
## Module Structure

Standard module layout:
```
components/<module_name>/
├── CMakeLists.txt
├── include/<module_name>/
│   ├── <Module>Module.h
│   └── (module-specific headers in module namespace folder)
└── src/
    ├── <Module>Module.cpp
    └── (implementation files)
```

**Phase 2: Fix usb_badge (quick win)**

1. Create `components/usb_badge/src/`
2. Move `usb_cdc.cpp` and `usb_hid.cpp` to `src/`
3. Update CMakeLists.txt paths

**Phase 3: Fix CalEPD (larger task, can be split)**

1. Create `components/CalEPD/src/`
2. Move `.cpp` files from root to `src/`
3. Create `components/CalEPD/include/CalEPD/` namespace folder
4. Move headers to namespace folder
5. Update CMakeLists.txt and includes

**Phase 4: Fix mod_gpg (medium task)**

1. Flatten `include/mod_gpg/ccid/` and `include/mod_gpg/openpgp/` into `include/mod_gpg/`
2. Or document this as intentional sub-module structure

## References

- `CLAUDE.md` - Module Architecture section
- `components/mod_totp/` - Standard module example
- `main/CMakeLists.txt` - Module registration
