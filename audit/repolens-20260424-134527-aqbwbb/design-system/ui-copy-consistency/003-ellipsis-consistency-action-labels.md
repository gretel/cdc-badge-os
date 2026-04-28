---
title: "[LOW] Ellipsis usage inconsistent in action labels"
severity: LOW
domain: design-system/ui-copy-consistency
lens: ui-copy-consistency
labels:
  - "microcopy"
  - "action-labels"
  - "punctuation"
---

## Summary
The application lacks a consistent convention for ellipsis (...) in action labels. While the codebase doesn't heavily use ellipsis, the pattern should be established for future consistency:

1. **Loading/progress states use ellipsis:**
   - "Scanning..." (line 284, I18n.cpp)
   - "Connecting..." (line 287, I18n.cpp)
   - "Syncing time..." (line 306, I18n.cpp)

2. **Static states without ellipsis:**
   - "Connection failed" (line 289, I18n.cpp)
   - "Connected!" (line 286, I18n.cpp) - uses exclamation instead
   - "Time synced!" (line 307, I18n.cpp) - uses exclamation

3. **No clear pattern for whether actions that trigger a next step should use ellipsis**

## Impact
- Users may be uncertain about the expected behavior (is this immediate or does it take time?)
- Inconsistent punctuation makes the interface feel less polished
- Future developers won't have a clear convention to follow

## Evidence
**File: `components/cdc_ui/src/I18n.cpp` lines 284-287, 306-307**
```cpp
REG(WIFI_SCANNING,      "Scanning...",          "Suche...");
REG(WIFI_CONNECTING,    "Connecting...",        "Verbinde...");
REG(NTP_SYNCING,        "Syncing time...",      "Synchronisiere Zeit...");
REG(WIFI_CONNECTING,    "Connecting...",        "Verbinde...");
REG(WIFI_CONNECTED,     "Connected!",           "Verbunden!");
REG(NTP_SUCCESS,        "Time synced!",         "Zeit synchronisiert!");
```

**File: `components/cdc_ui/src/I18n.cpp` line 289**
```cpp
REG(WIFI_FAILED,        "Connection failed",    "Verbindung fehlgeschlagen");
```

## Recommended Fix
Establish and document a clear ellipsis convention:

1. **Use ellipsis for:**
   - Actions that require additional user input (e.g., "Save..." if it opens a dialog)
   - Ongoing/background processes (e.g., "Scanning...", "Connecting...")

2. **Do NOT use ellipsis for:**
   - Instant actions (e.g., "Save", "Delete")
   - Status messages (e.g., "Connected", "Synced")
   - Error states (e.g., "Connection failed")

3. **Consider removing exclamation marks:**
   - "Connected!" → "Connected"
   - "Time synced!" → "Time synced"
   - More professional, less emphatic tone

## References
- Button and Action Label Inconsistency: Ellipsis convention
- UI Copy: Punctuation consistency
