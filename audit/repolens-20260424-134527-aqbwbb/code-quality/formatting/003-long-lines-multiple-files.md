---
title: "[MEDIUM] Lines exceeding 120 characters in multiple source files"
severity: MEDIUM
domain: code-quality/formatting
lens: formatting-consistency
labels:
  - "audit:code-quality/formatting"
---

## Summary
Multiple source files contain lines exceeding 120 characters. While the project doesn't appear to have a strict line length limit configured, lines over 120 characters can reduce readability on standard terminal widths and create wider diffs.

**Affected files and lines:**

1. **components/mod_password/src/PasswordModule.cpp** (2 lines, 125 chars each):
   - Line 264: `cdc::serial::Console::printf("Usage: PASSWORD_ADD <title> <username|- > <password> <url|- > [totpSlot] [notes]\r\n");`
   - Line 271: Same printf statement

2. **components/usb_badge/usb_cdc.cpp** (3 lines, 124 chars each):
   - Line 16: `#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0) && defined(CONFIG_SOC_USB_OTG_SUPPORTED) && CONFIG_SOC_USB_OTG_SUPPORTED`
   - Line 40: Same preprocessor directive
   - Line 119: Same preprocessor directive

3. **components/cdc_os_ui/src/AppUi.cpp** (1 line, 121 chars):
   - Line 345: `s_toolsItems[2] = {tr(StringId::BLUETOOTH), static_cast<uint8_t>(ble && ble->isEnabled() ? '*' : 0), false, nullptr};`

## Impact
- **Readability**: Lines over 120 characters require horizontal scrolling on standard editor windows
- **Diff readability**: Longer lines create wider diffs that are harder to review
- **Consistency**: Most of the codebase stays within 120 characters

## Evidence

**File: `components/mod_password/src/PasswordModule.cpp:264`**
```cpp
cdc::serial::Console::printf("Usage: PASSWORD_ADD <title> <username|- > <password> <url|- > [totpSlot] [notes]\r\n");
```

**File: `components/usb_badge/usb_cdc.cpp:16`**
```cpp
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0) && defined(CONFIG_SOC_USB_OTG_SUPPORTED) && CONFIG_SOC_USB_OTG_SUPPORTED
```

**File: `components/cdc_os_ui/src/AppUi.cpp:345`**
```cpp
s_toolsItems[2] = {tr(StringId::BLUETOOTH), static_cast<uint8_t>(ble && ble->isEnabled() ? '*' : 0), false, nullptr};
```

## Recommended Fix

### For PasswordModule.cpp (lines 264, 271)
Extract the format string to a named constant:
```cpp
static constexpr const char* PASSWORD_ADD_USAGE = "Usage: PASSWORD_ADD <title> <username|- > <password> <url|- > [totpSlot] [notes]\\r\\n";
// Then use:
cdc::serial::Console::printf(PASSWORD_ADD_USAGE);
```

### For usb_cdc.cpp (lines 16, 40, 119)
Use a macro or split the preprocessor condition:
```cpp
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
#if defined(CONFIG_SOC_USB_OTG_SUPPORTED) && CONFIG_SOC_USB_OTG_SUPPORTED
// ... code ...
#endif
#endif
```

### For AppUi.cpp (line 345)
Split across multiple lines:
```cpp
auto* ble = hal::getBluetoothControllerInstance();
s_toolsItems[2] = {tr(StringId::BLUETOOTH), 
                   static_cast<uint8_t>(ble && ble->isEnabled() ? '*' : 0), 
                   false, nullptr};
```

## References
- Common C/C++ line length conventions: 100-120 characters
- ESP-IDF style guide: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/style-guide.html
