---
title: "[LOW] Hardware info screen presents dense technical data without visual hierarchy"
severity: LOW
domain: UI/UX
lens: cognitive-overload
labels:
  - information-walls
  - data-density
---

## Summary

The `HardwareInfo` screen (`HardwareInfo.cpp:28-140`) displays system information as a flat text block with basic section headers. The 512-character buffer contains ~20+ lines of technical data presented without visual grouping, icons, or progressive disclosure.

**Files:**
- `components/cdc_os_ui/src/HardwareInfo.cpp:28-140` (hardware info text generation)
- `components/cdc_views/src/InfoView.cpp:140-220` (scrollable info display)

## Impact

**User Experience:** Users must scroll through a dense block of technical information to find specific details. The flat text format doesn't leverage the display's ability to show structured data with visual hierarchy.

**Evidence:**
From `HardwareInfo.cpp:28-140`, all data is appended to a single text buffer:
```cpp
append("=== %s ===\n", tr(StringId::HARDWARE_INFO));
append("%s: %s\n", tr(StringId::HW_I2C_BUS), i2cOk ? okText : failText);
append("%s: %s\n", tr(StringId::HW_BQ25895), powerOk ? okText : failText);
// ... 20+ more lines of similar format
append("--- %s ---\n", tr(StringId::HW_SECTION_MEMORY));
append("%s: %lu/%lu KB\n", tr(StringId::HW_HEAP), ...);
// ... more lines
```

From `InfoView.cpp:40-50`, the text is displayed as-is with basic scrolling:
```cpp
void InfoView::init(const char* title, const char* text) {
    strncpy(textBuf_, text, MAX_TEXT_LEN - 1);
    textBuf_[MAX_TEXT_LEN - 1] = '\0';
    // ...
    totalLines_ = countLines();
}
```

## Recommended Fix

1. **Use a list view** instead of InfoView to display hardware info as structured rows
2. **Add visual indicators** (checkmarks, X marks) for status columns
3. **Group related items** with visual section headers
4. **Consider collapsible sections** for less common data (PSRAM, NVS stats)

Quick fix: Convert the hardware info display to use `ListView` with custom row rendering to show status icons and better visual hierarchy.

## References

- Nielsen Norman Group: "Data-Dense Displays: Making Information Scannable"
- Material Design: "Lists" and "Cards" patterns
