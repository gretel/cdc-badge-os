---
title: "[MEDIUM] Remove unused CalEPD display driver files"
severity: MEDIUM
domain: code-quality
lens: dead-code
labels:
  - "audit:code-quality/dead-code"
---

## Summary
The **CalEPD component** contains **62 `.cpp` files** but only **3 are actually included** in the build. The remaining **59 files are dead code** - they are display driver implementations for various e-paper displays that are never compiled into the firmware.

**Build configuration (from `components/CalEPD/CMakeLists.txt`):**
```cmake
set(srcs
    "models/goodisplay/gdey029T94.cpp"  # Only this model file
    "epd.cpp"                           # Core base class
    "epdspi.cpp"                        # SPI interface
)
```

**Dead files (59 files, ~120KB of dead code):**
- `epd4spi.cpp`, `epd7color.cpp`, `epdParallel.cpp` - Alternative interfaces
- `models/color/*` - 19 color display drivers
- `models/parallel/*` - 3 parallel interface displays
- `models/plasticlogic/*` - 5 plasticlogic displays
- `models/goodisplay/*` - 7 other Goodisplay models (except gdey029T94)
- `models/dke/*`, `models/custom/*`, `models/fix/*` - 4 more models
- `models/gdeh0154d67.cpp`, `models/gdew0213i5f.cpp`, etc. - 21 more single models

**Example dead files:**
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/CalEPD/models/color/gdeh042Z98.cpp`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/CalEPD/models/parallel/ED060SC4.cpp`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/CalEPD/models/plasticlogic/plasticlogic031.cpp`

## Impact
- **Build time**: Slightly faster builds with fewer files to parse
- **Code navigation**: Easier to find relevant code with less noise
- **Repository size**: ~120KB reduction
- **Maintainability**: Clearer focus on the actually-used Gdey029T94 display

## Evidence
```bash
# Count all CalEPD cpp files
$ find /input/20260423-132359-oj8ayc/cdc-badge-os/components/CalEPD -name "*.cpp" -type f | grep -v "._" | wc -l
62

# Files actually in CMakeLists.txt
$ grep "set(srcs" -A 5 /input/20260423-132359-oj8ayc/cdc-badge-os/components/CalEPD/CMakeLists.txt
set(srcs
    "models/goodisplay/gdey029T94.cpp"
    "epd.cpp"
    "epdspi.cpp"
)

# Only 3 files are compiled, 59 are dead
```

The project uses only the **Gdey029T94** display (296x128 B/W e-paper), as confirmed by:
- `components/cdc_hal/src/EpaperDisplay.cpp` includes `<goodisplay/gdey029T94.h>`
- `components/mod_totp/src/TotpModule.cpp` casts to `Gdey029T94*`

## Recommended Fix
**Option 1: Create a subdirectory for unused drivers** (preserves them for future use):
```bash
mkdir -p components/CalEPD/models/unused/
mv components/CalEPD/models/color/ components/CalEPD/models/unused/
mv components/CalEPD/models/parallel/ components/CalEPD/models/unused/
mv components/CalEPD/models/plasticlogic/ components/CalEPD/models/unused/
# Move other unused directories...
```

**Option 2: Delete dead code completely** (cleaner, but loses history):
```bash
# Keep only used files
find components/CalEPD/models -name "*.cpp" -type f | grep -v "goodisplay/gdey029T94.cpp" | xargs rm
find components/CalEPD -maxdepth 1 -name "*.cpp" -type f | grep -v "epd.cpp" | grep -v "epdspi.cpp" | xargs rm
```

**Option 3: Add to a note in CMakeLists.txt**:
```cmake
# Note: Only GDEY029T94 is used. Other drivers in models/ are dead code.
# Consider moving them to models/unused/ or removing them.
set(srcs
    "models/goodisplay/gdey029T94.cpp"
    "epd.cpp"
    "epdspi.cpp"
)
```

## References
- [CalEPD library GitHub](https://github.com/martinberlin/cale-idf)
- [Dead code elimination best practices](https://www.agilealliance.org/glossary/dead-code/)
