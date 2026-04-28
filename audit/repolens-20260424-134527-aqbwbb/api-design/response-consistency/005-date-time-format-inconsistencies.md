---
title: "[LOW] Date/time display formats inconsistent across UI components"
severity: LOW
domain: api-design
lens: response-consistency
labels:
  - "audit:api-design/response-consistency"
  - "ui-consistency"
  - "date-time"
---

## Summary

Date and time display formats use inconsistent spacing and separators across different UI components, creating a disjointed user experience.

**Locations:**
- `components/cdc_views/src/TimeInputView.cpp` - Line 191
- `components/cdc_views/src/DateInputView.cpp` - Line 232
- `components/cdc_os_ui/src/SettingsHandlers.cpp` - Lines 131, 133
- `components/cdc_os_ui/src/SleepManager.cpp` - Lines 162, 164

**Current format inconsistencies:**

| Component | Time Format | Date Format | Example |
|-----------|-------------|-------------|---------|
| TimeInputView | `%02d : %02d` (spaces around colon) | N/A | `14 : 30` |
| SettingsHandlers | `%02d:%02d` (no spaces) | `%02d.%02d.%04d` (dots) | `14:30`, `25.04.2026` |
| SleepManager | `%02d:%02d` (no spaces) | `%02d.%02d.%04d` (dots) | `14:30`, `25.04.2026` |
| DateInputView | N/A | `%02d / %02d / %04d` (slashes with spaces) | `25 / 04 / 2026` |

## Impact

**User experience inconsistency:** Users see different date/time formats depending on where they look:
- Time on the display: `14 : 30` (with spaces)
- Time in settings: `14:30` (compact)
- Date in list view: `25 / 04 / 2026` (with slashes and spaces)
- Date in settings: `25.04.2026` (with dots, compact)

**Visual alignment:** Inconsistent spacing may cause:
- Misaligned text in lists
- Different width requirements for same data
- Jarring visual transitions between screens

**Localization:** Different components may need different locale-specific formatting, but current hard-coded formats make this difficult.

## Evidence

**TimeInputView.cpp (line 191):**
```cpp
snprintf(timeStr, sizeof(timeStr), "%02d : %02d", hour_, minute_);
// Output: "14 : 30" (7 characters with spaces around colon)
```

**DateInputView.cpp (line 232):**
```cpp
snprintf(dateStr, sizeof(dateStr), "%02d / %02d / %04d", day_, month_, year_);
// Output: "25 / 04 / 2026" (13 characters with slashes and spaces)
```

**SettingsHandlers.cpp (lines 131, 133):**
```cpp
// Line 131 - Time without spaces
snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);
// Output: "14:30" (5 characters, compact)

// Line 133 - Date with dots
snprintf(buf, sizeof(buf), "%02d.%02d.%04d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
// Output: "25.04.2026" (10 characters, compact)
```

**SleepManager.cpp (lines 162, 164):**
```cpp
// Line 162 - Time without spaces (same as SettingsHandlers)
snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);

// Line 164 - Date with dots (same as SettingsHandlers)
snprintf(buf, sizeof(buf), "%02d.%02d.%04d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
```

**Format comparison:**

| Format | Characters | Visual |
|--------|------------|--------|
| `%02d : %02d` | 7 | `14 : 30` |
| `%02d:%02d` | 5 | `14:30` |
| `%02d / %02d / %04d` | 13 | `25 / 04 / 2026` |
| `%02d.%02d.%04d` | 10 | `25.04.2026` |

## Recommended Fix

### Option 1: Standardize on compact format (recommended)

**Time format:** `%02d:%02d` (no spaces)
**Date format:** `%02d.%02d.%04d` (dots, no spaces)

**Update TimeInputView.cpp (line 191):**
```cpp
// Before
snprintf(timeStr, sizeof(timeStr), "%02d : %02d", hour_, minute_);

// After
snprintf(timeStr, sizeof(timeStr), "%02d:%02d", hour_, minute_);
```

**Update DateInputView.cpp (line 232):**
```cpp
// Before
snprintf(dateStr, sizeof(dateStr), "%02d / %02d / %04d", day_, month_, year_);

// After
snprintf(dateStr, sizeof(dateStr), "%02d.%02d.%04d", day_, month_, year_);
```

### Option 2: Standardize on spaced format (for better readability)

**Time format:** `%02d : %02d` (spaces around colon)
**Date format:** `%02d / %02d / %04d` (slashes with spaces)

**Update SettingsHandlers.cpp (lines 131, 133):**
```cpp
// Line 131
snprintf(buf, sizeof(buf), "%02d : %02d", tm->tm_hour, tm->tm_min);

// Line 133
snprintf(buf, sizeof(buf), "%02d / %02d / %04d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
```

**Update SleepManager.cpp (lines 162, 164):**
```cpp
// Line 162
snprintf(buf, sizeof(buf), "%02d : %02d", tm->tm_hour, tm->tm_min);

// Line 164
snprintf(buf, sizeof(buf), "%02d / %02d / %04d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
```

### Option 3: Create format helper functions

For better maintainability and localization support:

```cpp
// In a shared header (e.g., cdc_views/DateTimeFormat.h)
#ifndef CDC_DATETIME_FORMAT_H
#define CDC_DATETIME_FORMAT_H

#include <cstdio>
#include <ctime>

/**
 * \brief Formats time in HH:MM format (compact).
 * \param tm Pointer to tm struct.
 * \param buf Output buffer.
 * \param bufSize Buffer size.
 */
inline void formatTime(const struct tm* tm, char* buf, size_t bufSize) {
    snprintf(buf, bufSize, "%02d:%02d", tm->tm_hour, tm->tm_min);
}

/**
 * \brief Formats date in DD.MM.YYYY format.
 * \param tm Pointer to tm struct.
 * \param buf Output buffer.
 * \param bufSize Buffer size.
 */
inline void formatDate(const struct tm* tm, char* buf, size_t bufSize) {
    snprintf(buf, bufSize, "%02d.%02d.%04d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
}

#endif
```

Then use in all components:
```cpp
#include "cdc_views/DateTimeFormat.h"

// In TimeInputView.cpp
formatTime(&tm, timeStr, sizeof(timeStr));

// In SettingsHandlers.cpp
formatTime(tm, buf, sizeof(buf));
```

## References

- Existing findings: `003-inconsistent-date-time-formats.md` (serial commands)
- ISO 8601 date/time format standards
- UI/UX best practices for consistent formatting
