---
title: "[LOW] vCard peer list empty state lacks call-to-action"
severity: LOW
domain: mod_vcard
lens: empty-states
labels:
  - "empty-state"
  - "vcard"
  - "call-to-action"
---

## Summary
The vCard module's "Nearby" peer list view in `components/mod_vcard/src/VcardModule.cpp` shows "No peers found" when no BLE devices are discovered. While this provides a basic empty state message, it lacks guidance on what users should do to find nearby devices or start scanning.

**Location:** `components/mod_vcard/src/VcardModule.cpp:266-282` (rebuildPeerList function)

## Impact
Users seeing an empty peer list with just "No peers found" may:
- Not realize they need to start a scan first
- Think the feature is broken if they expect devices to appear automatically
- Not know how to initiate device discovery
- Exit the view without trying to find nearby devices

## Evidence
In `rebuildPeerList()` (lines 266-282):
```cpp
static void rebuildPeerList() {
    // Get current peers
    s_uiPeerCount = ble_vcard_get_peers(s_uiPeers, MAX_UI_PEERS);

    if (s_uiPeerCount == 0) {
        s_peerItems[0] = {mstr(STR_NO_PEERS), 0, true, nullptr};  // <-- Just shows message
        s_peerList.init(mstr(STR_NEARBY), s_peerItems, 1);
        return;
    }
    // ...
}
```

The main menu (lines 188-205) has separate "Start Scan" and "Start Advertising" buttons, but users navigating to "Nearby" might not realize they need to go back and start scanning first.

The main menu toggle items:
```cpp
s_mainMenuItems[MENU_SCAN_TOGGLE] = {
    scanning ? mstr(STR_STOP_SCAN) : mstr(STR_SCAN),
    0, false, nullptr
};
```

## Recommended Fix
Add a helpful call-to-action in the empty peer list state. Two approaches:

**Option 1: Add "Start Scan" as first item when empty**
```cpp
if (s_uiPeerCount == 0) {
    s_peerItems[0] = {mstr(STR_SCAN), 0, false, nullptr};  // Action item
    s_peerItems[1] = {mstr(STR_NO_PEERS), 0, true, nullptr};  // Info text
    s_peerList.init(mstr(STR_NEARBY), s_peerItems, 2);
    // Set callback for first item to start scan
} else {
    s_peerList.init(mstr(STR_NEARBY), s_peerItems, s_uiPeerCount);
}
```

**Option 2: Show InfoView with instructions when empty**
Modify `onPeerSelect()` to detect the empty state and show a helpful info view:
```cpp
static void onPeerSelect(uint16_t index, void* userData) {
    (void)userData;
    
    if (s_uiPeerCount == 0) {
        // Show instructions
        static char helpText[256];
        snprintf(helpText, sizeof(helpText),
            "No devices found.\n\n"
            "To exchange vCards:\n"
            "1. Go back to main menu\n"
            "2. Start scanning\n"
            "3. Make sure other devices are advertising\n\n"
            "Press N to go back.");
        ui::showInfo(mstr(STR_NEARBY), helpText);
        return;
    }
    
    if (index >= s_uiPeerCount) return;
    // ... existing code ...
}
```

## References
- vCard module: `components/mod_vcard/src/VcardModule.cpp`
- Similar pattern in TOTP: `components/mod_totp/src/TotpModule.cpp:686-720`
- Similar pattern in Password: `components/mod_password/src/PasswordModule.cpp:404-435`

</content>