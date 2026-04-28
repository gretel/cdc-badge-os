---
title: "[MEDIUM] Inconsistent status label terminology"
severity: MEDIUM
domain: design-system/ui-copy-consistency
lens: ui-copy-consistency
labels:
  - "microcopy"
  - "terminology"
  - "status-labels"
---

## Summary
The application uses inconsistent terminology for status labels across different features:

1. **On/Off status:**
   - `StringId::ON` / `StringId::OFF` (line 231-232, I18n.cpp)
   - Used for: Bluetooth status ("Bluetooth ON" / "Bluetooth OFF")

2. **Enabled/Disabled pattern:**
   - No dedicated StringId for "Enabled"/"Disabled"
   - Implicit in menu items with asterisk indicator

3. **Connected/Disconnected pattern:**
   - `StringId::WIFI_CONNECTED` / `StringId::WIFI_DISCONNECTED` (lines 286, 288)
   - `StringId::BLE_NOT_CONNECTED` (line 125, I18n.cpp)

4. **Mixed terminology for similar states:**
   - WiFi: "Connected" / "Disconnected"
   - Bluetooth: "ON" / "OFF" (for enabled state)
   - Bluetooth: "Not connected" (for connection state)
   - This creates confusion between "enabled" vs "connected" states

5. **Active/Inactive pattern missing:**
   - No StringId for "Active"/"Inactive"
   - Status shown via asterisks or icons instead

## Impact
- Users may confuse "ON" (enabled) with "Connected" (active connection)
- Inconsistent terminology makes it harder to scan and understand status
- Similar states described differently across features reduces clarity
- Makes it harder to establish a coherent mental model of the system

## Evidence
**File: `components/cdc_ui/src/I18n.cpp` lines 231-232, 286-288, 310-311, 125**
```cpp
REG(ON,             "On",               "Ein");
REG(OFF,            "Off",              "Aus");
REG(WIFI_CONNECTED,     "Connected!",       "Verbunden!");
REG(WIFI_DISCONNECTED,  "Disconnected",     "Getrennt");
REG(BLUETOOTH_ON,       "Bluetooth ON",     "Bluetooth EIN");
REG(BLUETOOTH_OFF,      "Bluetooth OFF",    "Bluetooth AUS");
REG(BLE_NOT_CONNECTED,  "Not connected",    "Nicht verbunden");
```

**File: `components/cdc_os_ui/src/BluetoothMenuUi.cpp` lines 128-130**
```cpp
s_bluetoothItems[BT_IDX_ENABLE] = {tr(StringId::BLUETOOTH_ON), '*', false, nullptr};
s_bluetoothItems[BT_IDX_ENABLE] = {tr(StringId::BLUETOOTH_OFF), 0, false, nullptr};
```

**File: `components/cdc_os_ui/src/WifiMenuUi.cpp` lines 207-209**
```cpp
s_wifiMainItems[WIFI_IDX_CONNECT] = {tr(StringId::WIFI_DISCONNECT), '*', false, nullptr};
s_wifiMainItems[WIFI_IDX_CONNECT] = {tr(StringId::WIFI_CONNECT), 0, false, nullptr};
```

## Recommended Fix
1. **Standardize status terminology:**
   - **Enabled/Disabled** for toggle states (Bluetooth on/off)
   - **Connected/Disconnected** for connection states (WiFi, BLE devices)
   - **Active/Inactive** for operational states

2. **Add missing StringIds:**
   - `ENABLED` / `DISABLED`
   - `ACTIVE` / `INACTIVE`

3. **Consistent naming pattern:**
   - "Bluetooth: Enabled" / "Bluetooth: Disabled"
   - "WiFi: Connected" / "WiFi: Disconnected"
   - "Device: Connected" / "Device: Disconnected"

4. **Remove exclamation marks from status:**
   - "Connected!" → "Connected"
   - More professional, less emphatic tone

## References
- Terminology Drift Across Features: Status labels
- UI Copy Consistency: Status terminology
