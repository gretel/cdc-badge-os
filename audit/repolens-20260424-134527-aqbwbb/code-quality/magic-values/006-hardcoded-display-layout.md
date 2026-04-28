---
title: "[MEDIUM] Hardcoded display layout and UI geometry constants"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "magic-values"
---

## Summary
Display dimensions, layout coordinates, and UI geometry values are hardcoded throughout the view components. While some constants are defined (e.g., `TITLE_Y`, `LIST_START_Y`), many magic numbers remain scattered in the rendering logic.

**Files affected:**
- `components/cdc_views/src/ListView.cpp` - Display: 296x128, item height: 18, visible items: 4
- `components/cdc_views/src/ConfirmView.cpp:87,91,184` - Coordinates: 20, 30, 12
- `components/cdc_views/src/DateInputView.cpp:156,157` - Year range: 2000-2099
- `components/cdc_views/src/T9InputView.cpp:292` - Margins: 5, 30
- `components/cdc_views/src/ToastView.cpp` - Duration: 1500ms
- `components/cdc_views/src/MessageBox.cpp` - Timeout: 2000ms

**Magic values found:**
- `296`, `128` - Display resolution (width x height)
- `30` - Header bar height
- `16` - Footer bar height
- `18` - List item height
- `4` - Visible list items (derived from available space)
- `5` - Title Y position
- `10` - Item padding X, icon spacing
- `8` - Scroll indicator width
- `20` - Text Y offset, icon X position
- `12` - Button Y offset
- `30` - Text X offset for buttons
- `100` - Max items in list
- `16` - String buffer for position counter
- `24` - Label string buffer size
- `1500` - Toast duration (ms)
- `2000` - MessageBox timeout (ms)
- `2000`, `2099` - Valid year range

## Impact
- **Display portability**: Changing display requires searching through multiple files
- **UI consistency**: Similar layouts may have slightly different spacing
- **Maintainability**: Layout adjustments require manual calculation in multiple places
- **Readability**: `y + 10` is less clear than `y + ICON_SPACING`

## Evidence
```cpp
// components/cdc_views/src/ListView.cpp:5,29,30
// Display is 296x128, VISIBLE_ITEMS is fixed at 4.
// Available height: 128 - 30 (header) - 16 (footer) = 82px.
// With `itemHeight_=18`: 82 / 18 = 4 visible rows.

// components/cdc_views/src/ListView.cpp:21
static constexpr int TITLE_Y = 5;
static constexpr int LIST_START_Y = 30;
static constexpr int ITEM_PADDING_X = 10;
static constexpr int SCROLL_INDICATOR_WIDTH = 8;
static constexpr uint8_t VISIBLE_ITEMS = 4;  // Derived, not configurable

// components/cdc_views/src/ListView.cpp:241
gfx->drawLine(textX, y + 10, textX + 6, y + 10, color);  // Magic: icon height

// components/cdc_views/src/ListView.cpp:243
textX += 10;  // Magic: icon width

// components/cdc_views/src/ListView.cpp:262
char positionStr[16];  // Magic: buffer size for "999/999  "

// components/cdc_views/src/ConfirmView.cpp:87
int textY = boxY + 20;  // Magic: text vertical offset

// components/cdc_views/src/ConfirmView.cpp:91
int iconX = boxX + 20;  // Magic: icon horizontal offset

// components/cdc_views/src/DateInputView.cpp:156,157
if (year_ < 2000) year_ = 2000;
if (year_ > 2099) year_ = 2099;  // Magic: year range

// components/cdc_views/src/ToastView.h:37,58
void init(const char* message, Icon icon = Icon::NONE, uint16_t durationMs = 1500, ...);
uint16_t durationMs_ = 1500;  // Magic: default toast duration

// components/cdc_views/include/cdc_views/MessageBox.h:88,95
inline void showSuccess(const char* message, uint32_t timeoutMs = 2000) {
inline void showError(const char* message, uint32_t timeoutMs = 2000) {
```

## Recommended Fix
1. **Create display layout constants** in `components/cdc_views/include/cdc_views/DisplayLayout.h`:
   ```cpp
   #pragma once
   
   // Display dimensions (GDEY029T94 2.9" E-Paper)
   static constexpr uint16_t DISPLAY_WIDTH = 296;
   static constexpr uint16_t DISPLAY_HEIGHT = 128;
   
   // Layout spacing
   static constexpr uint16_t HEADER_HEIGHT = 30;
   static constexpr uint16_t FOOTER_HEIGHT = 16;
   static constexpr uint16_t TITLE_Y = 5;
   static constexpr uint16_t LIST_START_Y = 30;
   
   // List view geometry
   static constexpr uint16_t ITEM_PADDING_X = 10;
   static constexpr uint16_t ITEM_HEIGHT = 18;
   static constexpr uint16_t SCROLL_INDICATOR_WIDTH = 8;
   static constexpr uint16_t ICON_WIDTH = 10;
   static constexpr uint16_t ICON_HEIGHT = 10;
   
   // Calculated values
   static constexpr uint16_t LIST_AVAIL_HEIGHT = DISPLAY_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT;
   static constexpr uint8_t VISIBLE_ITEMS = LIST_AVAIL_HEIGHT / ITEM_HEIGHT;  // = 4
   
   // String buffer sizes
   static constexpr uint8_t POSITION_STR_LEN = 16;  // "999/999  "
   static constexpr uint8_t LABEL_MAX_LEN = 24;
   ```

2. **Create UI timing constants**:
   ```cpp
   // components/cdc_views/include/cdc_views/UiTiming.h
   static constexpr uint16_t TOAST_DURATION_MS = 1500;
   static constexpr uint16_t MESSAGEBOX_TIMEOUT_MS = 2000;
   static constexpr uint16_t PIN_ENTRY_TIMEOUT_MS = 1000;
   ```

3. **Create content constraints**:
   ```cpp
   // components/cdc_views/include/cdc_views/ContentLimits.h
   static constexpr uint16_t DATE_MIN_YEAR = 2000;
   static constexpr uint16_t DATE_MAX_YEAR = 2099;
   static constexpr uint16_t MAX_LIST_ITEMS = 100;
   ```

4. **Refactor usage**:
   ```cpp
   // Before:
   char positionStr[16];
   gfx->drawLine(textX, y + 10, textX + 6, y + 10, color);
   
   // After:
   char positionStr[POSITION_STR_LEN];
   gfx->drawLine(textX, y + ICON_HEIGHT, textX + ICON_WIDTH, y + ICON_HEIGHT, color);
   ```

5. **Consider Kconfig for display selection**:
   ```cmake
   config DISPLAY_WIDTH
       int "Display width in pixels"
       default 296
       
   config DISPLAY_HEIGHT
       int "Display height in pixels"
       default 128
   ```

## References
- [Good Display GDEY029T94 datasheet](http://www.e-paper-display.com/download_detail/downloadsId%3d534.html)
- [E-Paper display driver guide](https://www.waveshare.com/wiki/2.9inch_e-Paper_Module)
