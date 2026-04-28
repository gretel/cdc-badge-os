---
title: "[MEDIUM] WiFi setup wizard forces sequential input without batch preview"
severity: MEDIUM
domain: UI/UX
lens: cognitive-overload
labels:
  - "form-fatigue"
  - "wizard-flow"
---

## Summary
The WiFi setup wizard in `components/cdc_os_ui/src/WifiMenuUi.cpp` forces users through a sequential multi-step input flow (SSID -> Authentication -> Password -> IP Mode -> Static IP/Gateway/Netmask) without showing a summary/preview before final confirmation. Each step immediately pushes the next view, and there is no "Review" screen before `wifiFinishSetup()` persists and connects.

**File:** `components/cdc_os_ui/src/WifiMenuUi.cpp`
**Lines:** 440-620 (wizard flow)

## Impact
- **Cognitive Load:** Users must remember all entered values across 5-7 sequential steps without a chance to review
- **Error Prone:** If a user makes a mistake in step 1 (SSID), they won't realize until step 6 (IP configuration), requiring a full restart
- **No Context:** Users cannot see the "big picture" of their configuration before committing
- **Frustration:** Static IP setup requires 3 consecutive text inputs (IP, Gateway, Netmask) with no visual grouping

## Evidence
The wizard flow in `onWifiScanSelect()` and related functions:

```cpp
// Line 440-460: Scan selection -> Auth menu
static void onWifiScanSelect(uint16_t index, void* userData) {
    // ...
    if (wizard.security == hal::WifiSecurity::OPEN) {
        wizard.password[0] = '\0';
        wifiShowIpModeMenu();  // Direct jump, no summary
    } else {
        wifiShowPasswordInput();
    }
}

// Line 508-520: Password -> IP mode (no review)
static void onWifiPasswordEntered(const char* password) {
    auto& wizard = WifiHandlers::instance().wizard();
    strncpy(wizard.password, password, sizeof(wizard.password) - 1);
    wifiShowIpModeMenu();  // Direct jump
}

// Line 535-560: IP mode -> 3 sequential inputs
static void onWifiIpModeSelect(uint16_t index, void* userData) {
    wizard.useDhcp = (index == WIFI_IP_DHCP);
    if (wizard.useDhcp) {
        wifiFinishSetup();  // Immediate finish
    } else {
        wifiShowIpInputField("IP", wizard.staticIp, sizeof(wizard.staticIp), onWifiStaticIpEntered);
    }
}

// Line 570-610: 3 sequential text inputs with no batch preview
static void onWifiStaticIpEntered(const char* ip) {
    // Validates then immediately shows next field
    wifiShowIpInputField(tr(StringId::WIFI_GATEWAY), wizard.gateway, sizeof(wizard.gateway), onWifiGatewayEntered);
}
```

## Recommended Fix
Add a "Review & Confirm" step at the end of the wizard that:
1. Shows all entered values in a scrollable `InfoView` or `ListView`
2. Provides "Edit" options for each field (or "Back" to modify)
3. Requires explicit "Confirm" before calling `wifiFinishSetup()`

Implementation approach:
- Add `WIFI_IDX_REVIEW` to the wizard state machine
- Create `wifiShowReviewScreen()` that builds a summary from `wizard` struct
- Use existing `InfoView` with "Y=Confirm, N=Back" callbacks
- Only persist config after user confirms

Alternative (simpler): Add a single summary toast before connecting:
```cpp
static void wifiFinishSetup() {
    // Show summary first
    char summary[128];
    snprintf(summary, sizeof(summary), "SSID: %s\nIP: %s\nConnect?",
             wizard.ssid, wizard.useDhcp ? "DHCP" : wizard.staticIp);
    showConfirm(summary, applyWifiConfig, nullptr, ConfirmView::Icon::INFO);
}
```

## References
- Nielsen Norman Group: [Progressive Disclosure](https://www.nngroup.com/articles/progressive-disclosure/)
- Cognitive Load Theory: Reduce extraneous load by grouping related information
- Form Design Best Practices: Show summaries for multi-step flows with 4+ fields
