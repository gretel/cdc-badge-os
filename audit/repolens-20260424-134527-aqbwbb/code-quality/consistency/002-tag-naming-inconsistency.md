---
title: "[MEDIUM] Tag Naming Inconsistency: Non-uniform TAG constant declarations"
severity: MEDIUM
domain: code-style
lens: consistency
labels:
  - "audit:code-quality/consistency"
  - "naming-conventions"
---

## Summary

The TAG constant used for logging shows inconsistent spacing and naming patterns across the codebase:

**Spacing variations:**
1. `static const char* TAG` (61 files - majority)
2. `static const char *TAG` (2 files - mod_gpg)

**Naming variations:**
- Acronyms: "GPG", "HID", "TOTP", "SAO", "CCID"
- Abbreviations: "NvsEdit", "AttestKey", "ModuleReg", "UsbManager", "SleepMgr"
- Full names: "ServiceRegistry", "EventBus", "PinManager", "RgbInputView", "GroveLED"
- Mixed: "USB", "USB_HID", "TR01_STORE", "BleHID"

**Evidence:**
```cpp
// Inconsistent spacing
components/mod_gpg/src/openpgp/ccid.cpp:static const char *TAG = "CCID";
components/mod_gpg/src/openpgp/openpgp.cpp:static const char *TAG = "OpenPGP";

// Most files use this style
components/mod_totp/src/TotpModule.cpp:static const char* TAG = "TOTP";
components/cdc_core/src/ModuleRegistry.cpp:static const char* TAG = "ModuleReg";

// Inconsistent naming
components/usb_badge/usb_cdc.cpp:static const char* TAG = "USB";
components/usb_badge/usb_hid.cpp:static const char* TAG = "USB_HID";
components/mod_gpg/src/GpgStorage.cpp:static const char* TAG = "GPGStorage";
components/mod_gpg/src/GpgModule.cpp:static const char* TAG = "GPG";
```

## Impact

1. **Code review friction**: Reviewers need to check spacing conventions
2. **Search difficulty**: `grep "const char* TAG"` misses `const char *TAG`
3. **Inconsistent appearance**: Log output shows different tag lengths and styles
4. **Onboarding confusion**: New developers unsure which style to follow

## Evidence

**Files with spacing variation (char *TAG):**
- `components/mod_gpg/src/openpgp/ccid.cpp`
- `components/mod_gpg/src/openpgp/openpgp.cpp`

**Naming inconsistencies within same module:**
- mod_gpg: "CCID", "OpenPGP", "GPG", "GPGStorage" (4 different styles)
- usb_badge: "USB", "USB_HID" (2 different styles)
- mod_hid: "BleHID", "HID" (2 different styles)

**Full list of TAG values found:**
```
"TOTP", "NvsEdit", "USB_HID", "USB", "AttestKey", "ModuleReg", "TR01_STORE",
"ServiceRegistry", "UsbManager", "SlotMap", "EventBus", "PinManager", "CCID",
"OpenPGP", "GPGStorage", "GPG", "GroveLED", "RgbInputView", "QRCodeView",
"ListView", "TimeInputView", "ContextMenuView", "DateInputView", "MessageBox",
"SliderView", "PinEntryView", "T9InputView", "InfoView", "SAO", "I18n",
"ViewStack", "BleHID", "HID", "SleepMgr", "LockScreen", "PinChangeView",
"CMDREGS", "FIDO2", "Vcard", "BleUart", "KeyboardLayout", "HidReportMap"
```

## Recommended Fix

**Establish and document a convention:**

1. **Spacing**: Use `static const char* TAG` (majority style)

2. **Naming convention**:
   - Module-level: Use module name in uppercase (e.g., "GPG", "HID", "TOTP", "SAO")
   - Component-level: Use descriptive name (e.g., "ServiceRegistry", "ModuleRegistry")
   - View-level: Use class name without "View" suffix (e.g., "PinEntry", "ContextMenu")
   - Service-level: Use service name (e.g., "AttestKey", "BleUart")

3. **Apply fixes:**
   ```bash
   # Fix spacing in mod_gpg files
   sed -i 's/static const char \*TAG/static const char* TAG/' components/mod_gpg/src/openpgp/ccid.cpp
   sed -i 's/static const char \*TAG/static const char* TAG/' components/mod_gpg/src/openpgp/openpgp.cpp
   ```

4. **Document in coding guidelines:**
   Add to `CLAUDE.md` or create `CODING_STANDARDS.md`:
   ```markdown
   ## Logging Tags
   
   ```cpp
   static const char* TAG = "ModuleName";  // Use module name in uppercase
   ```
   ```

## References

- `CLAUDE.md` - Code Quality section
- `components/cdc_log/include/cdc_log.h` - Logging API
