---
title: "[MEDIUM] Git Submodules with Manual Patching Increase Maintenance Burden"
severity: MEDIUM
domain: dependency-management
lens: maintainability
labels:
  - "submodules"
  - "patches"
  - "build-system"
---

## Summary

The project uses **3 git submodules** that require **manual patching** to work correctly. This creates a complex build process that must apply patches automatically and track which versions are compatible.

**Submodules (from `.gitmodules`):**
```ini
[submodule "components/CalEPD"]
    path = components/CalEPD
    url = https://github.com/martinberlin/CalEPD.git
    ignore = dirty
[submodule "third_party/libtropic"]
    path = third_party/libtropic
    url = https://github.com/tropicsquare/libtropic.git
    ignore = dirty
[submodule "components/Adafruit-GFX"]
    path = components/Adafruit-GFX
    url = https://github.com/martinberlin/Adafruit-GFX-Library-ESP-IDF
```

**Manual patches applied (in `CMakeLists.txt`):**
1. **CalEPD** - `patches/calepd_spi_miso.patch`: Adds MISO support for TROPIC01 on shared SPI bus
2. **libtropic** - `patches/libtropic_espidf_gcm_workaround.patch`: Workaround for ESP-IDF hardware GCM

**Patch application logic** (CMakeLists.txt, lines 3-55):
```cmake
# Patch CalEPD for CDC Badge (MISO support + minimal build)
set(CALEPD_DIR "${CMAKE_SOURCE_DIR}/components/CalEPD")
set(CALEPD_PATCH_MARKER "${CALEPD_DIR}/.patched")

if(NOT EXISTS ${CALEPD_PATCH_MARKER})
    # Overwrite CMakeLists.txt with minimal version
    file(WRITE "${CALEPD_DIR}/CMakeLists.txt" "...")

    # Apply MISO patch
    set(PATCH_MISO "${CMAKE_SOURCE_DIR}/patches/calepd_spi_miso.patch")
    if(EXISTS ${PATCH_MISO})
        execute_process(
            COMMAND patch -p1 -N -d ${CALEPD_DIR} -i ${PATCH_MISO}
            RESULT_VARIABLE PATCH_RESULT
        )
    endif()
    file(WRITE ${CALEPD_PATCH_MARKER} "Patched for CDC Badge")
endif()

# Patch libtropic for ESP-IDF Hardware GCM workaround
set(LIBTROPIC_DIR "${CMAKE_SOURCE_DIR}/third_party/libtropic")
set(LIBTROPIC_PATCH_MARKER "${LIBTROPIC_DIR}/.patched")

if(NOT EXISTS ${LIBTROPIC_PATCH_MARKER})
    set(PATCH_GCM "${CMAKE_SOURCE_DIR}/patches/libtropic_espidf_gcm_workaround.patch")
    if(EXISTS ${PATCH_GCM})
        execute_process(
            COMMAND patch -p1 -N -d ${LIBTROPIC_DIR} -i ${PATCH_GCM}
            RESULT_VARIABLE PATCH_RESULT
        )
    endif()
    file(WRITE ${LIBTROPIC_PATCH_MARKER} "Patched for ESP-IDF GCM")
endif()
```

**Additional complexity:**
- `third_party/libtropic` is **excluded** from ESP-IDF Component Manager (line 59: `set(EXCLUDE_COMPONENTS libtropic)`)
- Uses custom wrapper component `components/libtropic_sdk/` for building
- Patch marker files (`.patched`) are listed in `.gitignore`

## Impact

1. **Upgrade Difficulty**: Updating submodules requires re-testing all patches
2. **Version Lock-in**: Cannot easily upgrade to newer submodule versions without patch rebase
3. **Build Complexity**: Developers must understand patch application order
4. **Fragile Builds**: Patches may fail silently if upstream changes
5. **Documentation Gap**: No documented mapping of submodule versions to compatible patch sets
6. **CI/CD Overhead**: Build workflow must handle patch application (currently works via CMake)

## Recommended Fix

**Option 1: Fork and maintain upstream dependencies** (Recommended)

Create dedicated forks:
```
github.com/cdc-badge-os/CalEPD (with MISO patch applied)
github.com/cdc-badge-os/libtropic (with GCM workaround applied)
```

Update `.gitmodules`:
```ini
[submodule "components/CalEPD"]
    path = components/CalEPD
    url = https://github.com/cdc-badge-os/CalEPD.git
[submodule "third_party/libtropic"]
    path = third_party/libtropic
    url = https://github.com/cdc-badge-os/libtropic.git
[submodule "components/Adafruit-GFX"]
    path = components/Adafruit-GFX
    url = https://github.com/cdc-badge-os/Adafruit-GFX-Library-ESP-IDF
```

**Benefits:**
- Clean submodule updates
- Patches become part of fork history
- Easy to track upstream changes
- One-time effort to maintain forks

**Option 2: Document version compatibility matrix**

Create `DEPENDENCIES.md`:
```markdown
# Dependency Versions

| Submodule | Version | Commit | Patches | Last Tested |
|-----------|---------|--------|---------|-------------|
| CalEPD | master | abc123 | MISO | 2026-01-27 |
| libtropic | v3.0.0 | def456 | GCM | 2026-01-27 |
| Adafruit-GFX | v1.7.7 | ghi789 | None | 2026-01-27 |
```

**Option 3: Convert to ESP-IDF Component Manager**

If upstream supports it, replace submodules with component manager dependencies:
```cmake
idf_component_register(
    REQUIRES
        "espressif/led_strip"
        "espressif/tinyusb"
        # Custom components
        "cal_epd"
        "libtropic"
)
```

## References

- [Git submodules documentation](https://git-scm.com/book/en/v2/Git-Tools-Submodules)
- [ESP-IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/versions.html#component-manager)
- [Managing Patches in Git](https://git-scm.com/docs/git-apply)
