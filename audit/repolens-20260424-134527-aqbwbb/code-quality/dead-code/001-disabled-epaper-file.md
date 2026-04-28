---
title: "[MEDIUM] Dead disabled file: EpaperDisplay.cpp.disabled"
severity: MEDIUM
domain: dead-code
lens: dead-code
labels:
  - "audit:code-quality/dead-code"
---

## Summary
A disabled source file `EpaperDisplay.cpp.disabled` exists in the components/cdc_hal/src/ directory. This file contains a complete implementation of the EpaperDisplay class but is not compiled into the build system.

**Location:** `/input/20260423-132359-oj8ayc/cdc-badge-os/components/cdc_hal/src/EpaperDisplay.cpp.disabled`

The file contains 277 lines with a complete implementation of:
- `EpaperDisplay` class extending `IDisplay`
- Backlight control via LEDC
- Async render task
- NVS persistence for backlight settings

## Impact
- **Maintenance burden**: Dead files clutter the repository and can confuse developers
- **Version control noise**: Increases repository size and git history
- **Potential confusion**: Developers may wonder if this is an alternative implementation or a work-in-progress

## Evidence
```
File: components/cdc_hal/src/EpaperDisplay.cpp.disabled
Size: 277 lines
Key content:
  - Line 46: class EpaperDisplay : public IDisplay {
  - Line 96: bool EpaperDisplay::init() {
  - Line 272: IDisplay* getDisplayInstance() {
```

Note: There is also a hidden Mac resource file `._EpaperDisplay.cpp.disabled` alongside it.

## Recommended Fix
1. **If the implementation is needed**: Rename to `EpaperDisplay.cpp` and integrate into the CMakeLists.txt
2. **If obsolete**: Delete both files:
   ```bash
   rm components/cdc_hal/src/EpaperDisplay.cpp.disabled
   rm components/cdc_hal/src/._EpaperDisplay.cpp.disabled
   ```
3. **If keeping as reference**: Move to a `docs/` or `references/` folder with documentation on why it was disabled

## References
- Dead code detection best practices: https://docs.codeclimate.com/docs/tripling-your-codereview-speed#dead-code
- Clean Code: Chapter 1 "Introduction" - "The ratio of code to dead code is a good measure of a codebase's health"
