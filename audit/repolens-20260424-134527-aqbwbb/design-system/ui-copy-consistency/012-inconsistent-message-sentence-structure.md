---
title: "[LOW] Inconsistent message sentence structure and punctuation"
severity: LOW
domain: design-system/ui-copy-consistency
lens: UI-copy-consistency
labels:
  - "microcopy"
  - "error-messages"
  - "punctuation"
---

## Summary
Error and status messages across the application lack consistent sentence structure and punctuation patterns:

1. **Mixed sentence structures:**
   - Fragment style: "Wrong PIN", "Locked out", "Too many attempts"
   - Full sentence style: "PINs don't match", "Time not set"
   - Passive voice: "No WiFi configured", "Connection failed"
   - Active voice: "Sync failed", "Init failed"

2. **Inconsistent punctuation:**
   - No periods: "Wrong PIN", "Locked out", "Connection failed"
   - Exclamation marks: "Connected!", "Time synced!"
   - Ellipsis for ongoing: "Scanning...", "Connecting...", "Syncing time..."
   - Mixed: Some messages end with periods in code comments but not in actual strings

3. **Inconsistent article usage:**
   - With article: "No networks", "No devices found"
   - Without article: "No WiFi configured", "No keyboard connected"
   - Mixed patterns in similar contexts

4. **Tense inconsistency:**
   - Past tense: "Connection failed", "Sync failed", "Init failed"
   - Present tense: "Time not set", "Slot map invalid"
   - Present continuous: "Scanning...", "Connecting..."

5. **Capitalization inconsistency in messages:**
   - Title words: "Bluetooth ON", "IP Mode", "DHCP (Auto)"
   - Sentence case: "Connection failed", "Too many attempts"
   - Mixed: "BLE n/a" (acronym lowercase)

## Impact
- **Reduced polish**: Inconsistent patterns make the interface feel less refined
- **Cognitive load**: Users must adjust to different message patterns
- **Translation difficulty**: Translators need to infer patterns for consistency
- **Maintenance burden**: New developers lack clear conventions to follow

## Evidence
**Mixed sentence structures:**

**File: `components/cdc_ui/src/I18n.cpp` lines 247-253**
```cpp
REG(WRONG_PIN,          "Wrong PIN",            "Falsche PIN");           // Fragment
REG(LOCKED_OUT,         "Locked out",           "Gesperrt");              // Fragment
REG(TOO_MANY_ATTEMPTS,  "Too many attempts",    "Zu viele Versuche");     // Fragment
REG(PINS_DONT_MATCH,    "PINs don't match",     "PINs stimmen nicht");    // Full sentence
REG(PIN_TOO_SHORT,      "PIN too short",        "PIN zu kurz");           // Fragment
```

**File: `components/cdc_ui/src/I18n.cpp` lines 285-289**
```cpp
REG(WIFI_NO_NETWORKS,   "No networks",          "Keine Netzwerke");       // Fragment
REG(WIFI_FAILED,        "Connection failed",    "Verbindung fehlgeschlagen"); // Passive
REG(WIFI_NO_CONFIG,     "No WiFi configured",   "Kein WLAN konfiguriert");   // Passive
```

**File: `components/mod_totp/src/TotpModule.cpp` line 358**
```cpp
ui::showToastError(mstr(STR_TIME_INVALID));  // "Time not set" - Present tense
```

**Inconsistent punctuation:**

**File: `components/cdc_ui/src/I18n.cpp` lines 284-287, 306-307**
```cpp
REG(WIFI_SCANNING,      "Scanning...",          "Suche...");              // Ellipsis
REG(WIFI_CONNECTING,    "Connecting...",        "Verbinde...");           // Ellipsis
REG(NTP_SYNCING,        "Syncing time...",      "Synchronisiere Zeit...");// Ellipsis
REG(WIFI_CONNECTED,     "Connected!",           "Verbunden!");            // Exclamation
REG(NTP_SUCCESS,        "Time synced!",         "Zeit synchronisiert!");  // Exclamation
```

**File: `components/cdc_ui/src/I18n.cpp` line 289**
```cpp
REG(WIFI_FAILED,        "Connection failed",    "Verbindung fehlgeschlagen"); // No punctuation
```

**Inconsistent article usage:**

**File: `components/cdc_ui/src/I18n.cpp` lines 285, 311, 314**
```cpp
REG(WIFI_NO_NETWORKS,   "No networks",          "Keine Netzwerke");
REG(BLE_NO_DEVICES,     "No devices found",     "Keine Geraete gefunden");
REG(BLE_NOT_CONNECTED,  "Not connected",        "Nicht verbunden");
```

**File: `components/mod_totp/src/TotpModule.cpp` line 75**
```cpp
i18n.registerTranslation(s_strIdBase + STR_NO_KEYBOARD, ui::Language::EN, "No keyboard connected");
```

**Inconsistent capitalization:**

**File: `components/cdc_ui/src/I18n.cpp` lines 306-307**
```cpp
REG(BLUETOOTH_ON,       "Bluetooth ON",         "Bluetooth EIN");
REG(BLUETOOTH_OFF,      "Bluetooth OFF",        "Bluetooth AUS");
```

**File: `components/mod_ble_serial/src/BleSerialModule.cpp` lines 257, 262**
```cpp
ui::showToastError("BLE n/a");  // Lowercase "n/a"
ui::showToastError("BLE disabled");
```

## Recommended Fix
1. **Establish sentence structure convention:**
   - **Error messages**: Use consistent fragment style (no full sentences)
   - **Status messages**: Use consistent pattern (either all fragments or all phrases)
   - **Recommended pattern**: "Action + Status" (e.g., "Connection failed", "PIN incorrect")

2. **Standardize punctuation:**
   - **No periods** for short messages (fits display constraints)
   - **Ellipsis only** for ongoing processes ("Scanning...")
   - **Remove exclamation marks** for a more professional tone
   - **No exclamation marks** for status messages

3. **Fix specific inconsistencies:**
   ```cpp
   // Remove exclamation marks:
   "Connected!" → "Connected"
   "Time synced!" → "Time synced"
   
   // Standardize article usage:
   "No networks" → "No networks found" (consistent with "No devices found")
   Or: "No devices found" → "No devices" (consistent with "No networks")
   
   // Standardize tense:
   "Time not set" → "Time unavailable" or "Set time first"
   "Slot map invalid" → "Invalid slot map"
   ```

4. **Standardize capitalization:**
   - Use consistent case for acronyms: "N/A" not "n/a"
   - Apply consistent pattern to all status labels

5. **Document the convention:**
   - Create style guide section on message patterns
   - Include examples of correct punctuation and structure

## References
- Error Message Tone and Phrasing guidelines
- UI Copy Consistency: Sentence structure patterns
- Punctuation consistency in microcopy
