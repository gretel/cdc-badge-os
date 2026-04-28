---
title: "[MEDIUM] File naming: inconsistent case styles for header files"
severity: MEDIUM
domain: naming-conventions
lens: code-quality
labels:
  - "audit:code-quality/naming"
---

## Summary
Header files use different naming conventions: some use `PascalCase` (e.g., `ListView.h`), while others use `snake_case` (e.g., `vcard_store.h`, `hw_config.h`).

**Evidence:**

1. **components/cdc_views/include/cdc_views/**:
   - `ListView.h`, `SliderView.h`, `PinEntryView.h` - PascalCase

2. **components/mod_vcard/include/mod_vcard/**:
   - `VcardModule.h` - PascalCase
   - `vcard_store.h`, `ble_vcard.h` - snake_case

3. **components/cdc_hal/include/cdc_hal/**:
   - `II2cBus.h`, `IKeypad.h`, `ISecureElement.h` - PascalCase (interfaces)
   - `hw_config.h`, `libtropic_port_esp32.h` - snake_case

4. **components/cdc_core/include/cdc_core/**:
   - `EventBus.h`, `PinManager.h`, `ServiceRegistry.h` - PascalCase
   - `feature_flags.h` - snake_case

5. **components/CalEPD/include/**:
   - `epdParallel.h`, `gdep015OC1.h` - mixed camelCase/PascalCase
   - `gdew_colors.h` - snake_case

## Impact
- **Cross-platform issues**: Some filesystems are case-sensitive, others aren't
- **Findability**: Developers need to remember which convention applies
- **Build systems**: Case-sensitive includes may fail on different platforms
- **Consistency**: Makes the codebase look unprofessional

## Evidence
Side-by-side comparison:
- `ListView.h` vs `vcard_store.h` - same module type, different conventions
- `II2cBus.h` vs `hw_config.h` - both in cdc_hal, different conventions
- `feature_flags.h` vs `EventBus.h` - both in cdc_core, different conventions

## Recommended Fix
Choose ONE convention for all header files:

**Option A (Recommended for C++): Use PascalCase for all headers**
```
ListView.h, SliderView.h, PinEntryView.h
VcardModule.h, VcardStore.h, BleVcard.h
II2cBus.h, IKeypad.h, HwConfig.h
EventBus.h, PinManager.h, FeatureFlags.h
```

**Option B: Use snake_case for all headers**
```
list_view.h, slider_view.h, pin_entry_view.h
vcard_module.h, vcard_store.h, ble_vcard.h
ii2c_bus.h, i_keypad.h, hw_config.h
event_bus.h, pin_manager.h, feature_flags.h
```

**Steps:**
1. Choose one convention (PascalCase recommended for C++ files)
2. Rename all header files to match
3. Update all include statements throughout the codebase
4. Update any build system references

## References
- [C++ Core Guidelines - Naming](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-naming)
- [Google C++ Style Guide - File Names](https://google.github.io/styleguide/cppguide.html#File_Names)
