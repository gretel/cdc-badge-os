---
title: "[MEDIUM] WiFi setup wizard lacks format hints for IP address fields"
severity: MEDIUM
domain: information-architecture
lens: help-context
labels:
  - "wifi-setup"
  - "input-helpers"
  - "form-validation"
---

## Summary
The WiFi setup wizard (components/cdc_os_ui/src/WifiMenuUi.cpp) collects static IP configuration (IP address, gateway, netmask) without showing users the expected format or providing inline examples.

**Evidence:**
- File: `components/cdc_os_ui/src/WifiMenuUi.cpp:569-612`
- IP validation function at line 569: `onWifiStaticIpEntered()` - validates but doesn't show format
- Gateway input at line 582: `onWifiGatewayEntered()` - same issue
- Netmask input at line 595: `onWifiNetmaskEntered()` - same issue

The wizard flows through these inputs at lines 540-612:
```cpp
static void onWifiIpModeSelect(uint16_t index, void* userData) {
    // ...
    if (wizard.useDhcp) {
        wifiFinishSetup();
    } else {
        wifiShowIpInputField("IP", wizard.staticIp, sizeof(wizard.staticIp), onWifiStaticIpEntered);
    }
}

static void wifiShowIpInputField(const char* title, char* target, size_t targetSize,
                                  T9InputView::SaveCallback onComplete) {
    showT9Input(title, target, onComplete, 15);
}
```

The validation at line 570 checks for valid IP but users don't see the format before entering:
```cpp
if (!WifiHandlers::isValidIpAddress(ip)) {
    showToastError("Invalid IP", TOAST_DURATION_MEDIUM_MS);
    wifiShowIpInputField(tr(StringId::WIFI_GATEWAY), wizard.gateway, sizeof(wizard.gateway), onWifiGatewayEntered);
}
```

## Impact
Users setting up static IP configuration may:
1. Enter invalid formats (e.g., "192.168.1" instead of "192.168.1.1")
2. Not understand the difference between IP, gateway, and netmask formats
3. Get frustrated with repeated validation errors without understanding expected format
4. Need to consult external documentation

## Evidence
- WiFi wizard IP input flow: `components/cdc_os_ui/src/WifiMenuUi.cpp:540-612`
- Validation logic: `components/cdc_os_ui/src/WifiMenuUi.cpp:570-612`
- T9InputView used at `components/cdc_views/src/T9InputView.cpp:207-224` - no format hints in footer
- Footer hint at line 257: `HINT_T9_INPUT` just says "[0-9] T9 [Y] OK"

## Recommended Fix
Add format examples to the IP input fields. Modify `wifiShowIpInputField()` to show a placeholder or info view:

**Option 1: Add format hint to title**
```cpp
static void wifiShowIpInputField(const char* title, char* target, size_t targetSize,
                                  T9InputView::SaveCallback onComplete) {
    static char titleWithFormat[48];
    snprintf(titleWithFormat, sizeof(titleWithFormat), "%s (e.g., 192.168.1.100)", title);
    showT9Input(titleWithFormat, target, onComplete, 15);
}
```

**Option 2: Show info view before input**
```cpp
static void wifiShowIpInputField(const char* title, char* target, size_t targetSize,
                                  T9InputView::SaveCallback onComplete) {
    const char* formatInfo = "Format: X.X.X.X\n\n"
        "Each X is 0-255\n"
        "Example: 192.168.1.100";
    showInfo(title, formatInfo);
    showT9Input(title, target, onComplete, 15);
}
```

**Option 3: Add specific hints for each field**
```cpp
static void onWifiStaticIpEntered(const char* ip) {
    auto& wizard = WifiHandlers::instance().wizard();
    if (!WifiHandlers::isValidIpAddress(ip)) {
        showToastError("Format: 192.168.1.100", TOAST_DURATION_MEDIUM_MS);
        wifiShowIpInputField(tr(StringId::WIFI_STATIC_IP), wizard.staticIp, sizeof(wizard.staticIp), onWifiStaticIpEntered);
        return;
    }
    // ...
}
```

## References
- T9InputView implementation: `components/cdc_views/src/T9InputView.cpp`
- WiFi menu structure: `components/cdc_os_ui/src/WifiMenuUi.cpp`
- Validation function: `components/cdc_os_ui/src/WifiHandlers.cpp` (search for `isValidIpAddress`)
