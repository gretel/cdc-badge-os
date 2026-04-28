---
title: "[MEDIUM] InfoView displays long text without section structure or table of contents"
severity: MEDIUM
domain: information-architecture
lens: content-hierarchy
labels:
  - "audit:information-architecture/content-hierarchy"
---

## Summary
The `InfoView` component (`components/cdc_views/src/InfoView.cpp:1-259`) displays multi-line text content without any structural organization. Long content (e.g., hardware info with 50+ lines in `HardwareInfo.cpp:61-146`) is rendered as a flat, scrollable list without:
- Section headings or visual grouping
- Table of contents for navigation
- Anchor links for deep-linking to sections
- Clear content boundaries between different information categories

The `InfoView` supports up to 2048 characters (`MAX_TEXT_LEN = 2048`) and 5 visible lines, but provides no mechanism to organize or navigate large amounts of content scannably.

## Impact
Users must scroll linearly through potentially long text blocks to find specific information. For complex screens like hardware info (which shows I2C, power, keypad, display, TROPIC01, WiFi, BLE, memory, battery, temperature, and uptime data), users cannot quickly jump to relevant sections. This increases cognitive load and time-to-information.

## Evidence
File: `components/cdc_views/include/cdc_views/InfoView.h:8-18`
```cpp
/**
 * InfoView - Scrollable text display
 *
 * Displays multi-line text with vertical scrolling.
 * Good for help screens, about pages, long messages.
 */
```

File: `components/cdc_os_ui/src/HardwareInfo.cpp:50-146` shows unstructured content:
```cpp
append("=== %s ===\n", tr(StringId::HARDWARE_INFO));
// I2C Bus (no section heading structure)
append("%s: %s\n", tr(StringId::HW_I2C_BUS), i2cOk ? okText : failText);
// Power Management
append("%s: %s\n", tr(StringId::HW_BQ25895), powerOk ? okText : failText);
// ... continues flat for 15+ lines
```

The content uses `===` and `---` ASCII markers but these are not rendered as structured headings with navigation affordances.

## Recommended Fix
1. Add section heading detection in `InfoView::render()` - parse for lines starting with `===` or `---` and render them with visual distinction (bold, larger font, or underline)
2. Add a simple "jump to section" context menu (key '3') that shows a list of detected sections when content has more than 5 sections
3. Implement `addSection()` API method that allows callers to mark logical section boundaries for potential future navigation enhancement

This is a ~1 hour fix: add section detection logic and a simple visual distinction for headers.

## References
- InfoView header: `components/cdc_views/include/cdc_views/InfoView.h`
- HardwareInfo usage: `components/cdc_os_ui/src/HardwareInfo.cpp`
- ListView scroll indicators already exist (`ListView.cpp:260-268`) and could be adapted for section navigation
