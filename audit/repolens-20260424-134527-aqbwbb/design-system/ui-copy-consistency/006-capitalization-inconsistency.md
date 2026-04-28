---
title: "[LOW] Capitalization pattern inconsistency"
severity: LOW
domain: design-system/ui-copy-consistency
lens: ui-copy-consistency
labels:
  - "microcopy"
  - "capitalization"
  - "i18n"
---

## Summary
The application lacks a consistent capitalization pattern across UI elements:

1. **Title Case vs sentence case mixed:**
   - Menu items: "Main Menu", "Hardware Info", "Change PIN" (Title Case)
   - Status messages: "Saved", "Deleted", "Failed" (Title Case - single word)
   - Error messages: "Wrong PIN", "Locked out", "Too many attempts" (mixed)
   - Footer hints: "[N] Back", "[Y] Select" (Title Case)

2. **Inconsistent within same category:**
   - "Hardware Info" (Title Case) vs "Hardware Info 2" (Title Case)
   - "Daylight Saving" vs "Sommerzeit" (German doesn't use same capitalization)
   - "Scanning..." vs "Syncing time..." (lowercase "time")

3. **Acronyms inconsistent:**
   - "WiFi" vs "WLAN" (German) - consistent
   - "BLE" vs "Bluetooth" - mixed
   - "NTP" - all caps
   - "PIN" - all caps
   - "ID" - all caps
   - "IP" - all caps

4. **Mixed capitalization in compound strings:**
   - "Bluetooth ON" / "Bluetooth OFF" (ON/OFF all caps)
   - "IP Mode" / "DHCP (Auto)" - mixed

## Impact
- Visual inconsistency makes the interface feel less polished
- Users may perceive the application as less professional
- Makes it harder to scan UI elements quickly
- Complicates translation when source capitalization is inconsistent

## Evidence
**File: `components/cdc_ui/src/I18n.cpp` - various lines**
```cpp
REG(HARDWARE_INFO,  "Hardware Info",    "Hardware-Info");
REG(WIFI_FAILED,    "Connection failed", "Verbindung fehlgeschlagen");
REG(LOCKED_OUT,     "Locked out",       "Gesperrt");
REG(TOO_MANY_ATTEMPTS, "Too many attempts", "Zu viele Versuche");
REG(BLUETOOTH_ON,   "Bluetooth ON",     "Bluetooth EIN");
REG(BLUETOOTH_OFF,  "Bluetooth OFF",    "Bluetooth AUS");
REG(WIFI_IP_MODE,   "IP Mode",          "IP Modus");
REG(WIFI_DHCP,      "DHCP (Auto)",      "DHCP (Auto)");
```

**File: `components/cdc_views/src/ConfirmView.cpp` line 186**
```cpp
gfx->print("Y=Ja  N=Nein");  // Mixed: Y/N uppercase, Ja/Nein title case
```

## Recommended Fix
1. **Establish a capitalization convention:**
   - **Menu items and headings:** Title Case (e.g., "Hardware Info", "Change PIN")
   - **Status messages and errors:** Sentence case (e.g., "Connection failed", "Locked out")
   - **Action buttons:** Title Case or verb phrase (e.g., "Save", "Change PIN")
   - **Footer hints:** Title Case for actions (e.g., "[Y] Select [N] Back")

2. **Standardize acronyms:**
   - Always use all-caps for acronyms: "WiFi", "BLE", "NTP", "PIN", "IP", "ID", "UUID"
   - Document this convention

3. **Fix specific inconsistencies:**
   - "Bluetooth ON" → "Bluetooth Enabled" or "Bluetooth: On"
   - "Connection failed" → "Connection Failed" (if Title Case) OR keep as is (if sentence case)
   - "Locked out" → "Locked Out" (if Title Case) OR keep as is (if sentence case)

4. **Document the convention:**
   - Create a style guide section on capitalization
   - Include examples of correct usage

## References
- Capitalization Pattern Inconsistency guidelines
- UI Copy: Title Case vs Sentence Case
