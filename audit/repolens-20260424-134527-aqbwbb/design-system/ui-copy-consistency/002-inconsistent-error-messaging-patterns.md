---
title: "[MEDIUM] Inconsistent error message tone and structure"
severity: MEDIUM
domain: design-system/ui-copy-consistency
lens: ui-copy-consistency
labels:
  - "microcopy"
  - "error-messages"
  - "i18n"
---

## Summary
Error messages across the application mix different tones and sentence structures:

1. **Second-person vs impersonal:**
   - "Wrong PIN" (line 247, I18n.cpp) - impersonal
   - "PINs don't match" (line 251, I18n.cpp) - second-person implication
   - "Too many attempts" (line 253, I18n.cpp) - impersonal

2. **Mixed sentence patterns:**
   - "Connection failed" (line 289, I18n.cpp) - passive
   - "No networks" (line 285, I18n.cpp) - fragment
   - "No WiFi configured" (line 288, I18n.cpp) - passive voice
   - "Sync failed" (line 309, I18n.cpp) - short fragment
   - "Sync timeout" (line 310, I18n.cpp) - technical jargon

3. **Error strings used directly in toasts** without context (ExpertMenuUi.cpp lines 50, 98, 183, 197):
   - `showToastError(tr(StringId::FAILED), ...)`
   - Generic "Failed" lacks specificity

## Impact
- Users receive error feedback in inconsistent voices (some addressing them directly, others clinical)
- Generic "Failed" messages don't communicate what specifically went wrong
- Mixed sentence structure makes the interface feel less polished
- Technical jargon ("timeout", "configured") may confuse non-technical users

## Evidence
**File: `components/cdc_ui/src/I18n.cpp` lines 247-253**
```cpp
REG(WRONG_PIN,          "Wrong PIN",            "Falsche PIN");
REG(LOCKED_OUT,         "Locked out",           "Gesperrt");
REG(TOO_MANY_ATTEMPTS,  "Too many attempts",    "Zu viele Versuche");
REG(PINS_DONT_MATCH,    "PINs don't match",     "PINs stimmen nicht");
REG(PIN_TOO_SHORT,      "PIN too short",        "PIN zu kurz");
REG(PIN_MISMATCH,       "PINs don't match",     "PINs stimmen nicht");
```

**File: `components/cdc_ui/src/I18n.cpp` lines 285-289, 309-310**
```cpp
REG(WIFI_NO_NETWORKS,   "No networks",          "Keine Netzwerke");
REG(WIFI_FAILED,        "Connection failed",    "Verbindung fehlgeschlagen");
REG(WIFI_NO_CONFIG,     "No WiFi configured",   "Kein WLAN konfiguriert");
REG(NTP_FAILED,         "Sync failed",          "Sync fehlgeschlagen");
REG(NTP_TIMEOUT,        "Sync timeout",         "Sync Timeout");
```

**File: `components/cdc_os_ui/src/ExpertMenuUi.cpp` lines 50, 98, 183, 197**
```cpp
showToastError(error ? error : tr(StringId::FAILED), TOAST_DURATION_MEDIUM_MS);
showToastError(tr(StringId::FAILED), TOAST_DURATION_MEDIUM_MS);
showToastSuccess(tr(StringId::OK));
showToastError(tr(StringId::FAILED));
```

## Recommended Fix
1. **Standardize error message patterns:**
   - Use consistent voice: either all second-person ("You need to...") or all impersonal ("Connection failed")
   - Recommended: Impersonal, action-oriented pattern ("Connection failed", "PIN incorrect")

2. **Replace generic "Failed" with specific messages:**
   - Create specific error strings for each context (e.g., "Cache rebuild failed", "Cleanup failed")
   - Include the specific action that failed in the message

3. **Consistent punctuation:**
   - Decide on period vs no-period convention for error messages
   - Apply consistently across all error strings

4. **Simplify technical jargon:**
   - "Sync timeout" → "Sync timed out" or "Connection timed out"
   - "No WiFi configured" → "WiFi not configured" (consistent verb form)

## References
- Error Message Tone and Phrasing guidelines
- UI Copy Consistency: Consistent sentence structure
