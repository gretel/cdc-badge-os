---
title: "[MEDIUM] Wi-Fi setup wizard lacks progress indication"
severity: MEDIUM
domain: UI/UX
lens: cognitive-overload
labels:
  - multi-step-flows
  - progress-indication
---

## Summary

The Wi-Fi setup wizard flow (`wifiSetup()` in `WifiMenuUi.cpp`) requires users to navigate through multiple sequential steps (SSID selection, password entry, IP mode selection, static IP/gateway/netmask entry) without any visible progress indicator showing how many steps remain or which step they are on.

**Files:**
- `components/cdc_os_ui/src/WifiMenuUi.cpp:332-620` (Wi-Fi setup wizard flow)
- `components/cdc_views/include/cdc_views/T9InputView.h:99-116` (input views used in wizard)

## Impact

**User Experience:** Users have no sense of how many steps remain in the setup process, which can lead to frustration and abandonment. The flow can require up to 6 sequential screens for static IP configuration:
1. Network scan/selection
2. Authentication mode selection
3. Password entry (for secured networks)
4. IP mode selection (DHCP/static)
5. Static IP input
6. Gateway input
7. Netmask input

**Evidence:**
From `WifiMenuUi.cpp:548-616`, the static IP flow chains multiple input views without progress indication:
```cpp
case WIFI_IP_STATIC:
    // Step 1: IP address
    wifiShowIpInputField("IP", wizard.staticIp, sizeof(wizard.staticIp), onWifiStaticIpEntered);
    // Step 2: Gateway (no indication this is step 2 of 3)
    wifiShowIpInputField(tr(StringId::WIFI_GATEWAY), wizard.gateway, sizeof(wizard.gateway), onWifiGatewayEntered);
    // Step 3: Netmask (no indication this is step 3 of 3)
    wifiShowIpInputField(tr(StringId::WIFI_NETMASK), wizard.netmask, sizeof(wizard.netmask), onWifiNetmaskEntered);
```

Each step pushes a new `T9InputView` without any "Step X of Y" indicator in the title or header.

## Recommended Fix

Add progress indication to the wizard flow:

1. Add a step counter to the wizard state structure
2. Include step numbers in view titles (e.g., "IP Address (3/4)")
3. Add a simple visual progress indicator in the header (e.g., "---o---o--o--")
4. Consider consolidating static IP fields into a single multi-field view to reduce step count

Alternative quick fix:
- Update the title string to include step position: `showT9Input("IP (2/3)", ...)`

## References

- Nielsen Norman Group: "Wizard of Oz Design for Multi-Step Forms"
- Material Design: "Steppers" component design patterns
