---
title: "[MEDIUM] Duplicate/similar strings with slight variations"
severity: MEDIUM
domain: design-system/ui-copy-consistency
lens: ui-copy-consistency
labels:
  - "microcopy"
  - "i18n"
  - "deduplication"
---

## Summary
The codebase contains duplicate or near-duplicate strings with slight variations:

1. **PIN mismatch strings:**
   - `PINS_DONT_MATCH` = "PINs don't match" (line 251, I18n.cpp)
   - `PIN_MISMATCH` = "PINs don't match" (line 253, I18n.cpp)
   - **Identical English text, same German translation!**

2. **WiFi scanning vs BLE scanning:**
   - `WIFI_SCANNING` = "Scanning..."
   - `BLE_SCANNING` = "Scanning..."
   - Same text, separate string IDs (acceptable but could be consolidated)

3. **WiFi signal vs BLE signal:**
   - `WIFI_SIGNAL` = "Signal"
   - `BLE_SIGNAL` = "Signal"
   - Same text, separate string IDs

4. **Failed strings used in multiple contexts:**
   - `FAILED` = "Failed" (generic)
   - `WIFI_FAILED` = "Connection failed" (specific)
   - `NTP_FAILED` = "Sync failed" (specific)
   - Good pattern, but could be more systematic

5. **Error vs Error Generic:**
   - `ERROR_GENERIC` = "Error" (line 255, I18n.cpp)
   - Used as fallback, but "Error" is vague

## Impact
- **PINS_DONT_MATCH and PIN_MISMATCH are exact duplicates** - wastes string ID space
- Multiple string IDs for identical text creates confusion about which to use
- String ID space is limited (512 max), duplicates waste this resource
- Makes the i18n system harder to maintain
- Translation effort duplicated for identical text

## Evidence
**File: `components/cdc_ui/src/I18n.cpp` lines 251-253**
```cpp
REG(PINS_DONT_MATCH,  "PINs don't match",  "PINs stimmen nicht");
REG(PIN_TOO_SHORT,    "PIN too short",     "PIN zu kurz");
REG(PIN_MISMATCH,     "PINs don't match",  "PINs stimmen nicht");  // DUPLICATE!
```

**File: `components/cdc_ui/src/I18n.cpp` lines 299-300, 314-315**
```cpp
REG(WIFI_SCANNING,    "Scanning...",       "Suche...");
REG(BLE_SCANNING,     "Scanning...",       "Suche...");
REG(WIFI_SIGNAL,      "Signal",            "Signal");
REG(BLE_SIGNAL,       "Signal",            "Signal");
```

**File: `components/cdc_os_ui/src/views/PinChangeView.cpp` line 205**
```cpp
showMessage(tr(StringId::PIN_MISMATCH));
```

**File: `components/cdc_views/src/PinEntryView.cpp` line 165**
```cpp
showMessage(tr(StringId::WRONG_PIN), MessageIcon::ERROR, 1500);
```

## Recommended Fix
1. **Consolidate duplicate PIN strings:**
   - Remove `PINS_DONT_MATCH` or `PIN_MISMATCH` (keep one)
   - Update all references to use the retained StringId
   - Update `StringId` enum accordingly

2. **Consider consolidating generic scan/signal strings:**
   - Create shared `SCANNING` and `SIGNAL` StringIds
   - Use them for both WiFi and BLE contexts
   - Reduces total string count

3. **Document the naming convention:**
   - When to use generic vs specific strings
   - Example: Use `FAILED` for generic toast, `WIFI_FAILED` for WiFi-specific messages

4. **Audit for other duplicates:**
   - Search for identical English/German pairs
   - Check for near-duplicates that could be consolidated

## References
- Hardcoded Strings vs Centralized Copy: Duplicate strings
- UI Copy Consistency: String deduplication
