---
title: "[LOW] Display framebuffer not cached for partial updates on E-Paper display"
severity: LOW
domain: performance/caching
lens: embedded-firmware
labels:
  - "display-cache"
  - "epaper-optimization"
---

## Summary
The E-Paper display implementation (`components/cdc_hal/src/EpaperDisplay.cpp`) uses CalEPD library with multiple buffers but doesn't maintain a software framebuffer cache for tracking dirty regions. This can lead to unnecessary full-display refreshes when only small portions changed.

**Evidence:**
- File: `components/cdc_hal/src/EpaperDisplay.cpp`
- CalEPD library (`components/CalEPD/include/goodisplay/gdey029T94.h`) provides `_buffer1`, `_buffer2`, `_mono_buffer`
- Display has `updateWindow()` for partial updates but implementation may not leverage it optimally
- No software-level framebuffer diffing to detect changed regions

## Impact
**Performance Cost:**
- E-Paper displays are slow: full refresh takes 2-5 seconds
- Partial updates are faster (500ms-1s) but require accurate dirty-region tracking
- Without framebuffer caching, every UI update may trigger full refresh
- Power consumption: Full refresh requires more energy than partial updates

**User Experience:**
- Slower UI response when updating small elements (time, status indicators)
- Screen flicker from unnecessary full refreshes
- Longer time to display updated information

## Evidence
From `components/cdc_hal/src/EpaperDisplay.cpp`:

```cpp
/**
 * Render worker task processing async flush requests.
 */
static void renderTask(void* arg) {
    // ...
    if (doFull) {
        s_epd_display->update();  // Full refresh
    } else {
        s_epd_display->updateWindow(0, 0, HEIGHT, WIDTH, false);  // Partial
    }
}

// Display methods like drawPixel, drawLine modify buffers
// but no tracking of which regions changed
void EpaperDisplay::drawPixel(int16_t x, int16_t y, uint16_t color) override {
    // Modifies buffer but no dirty-region tracking
}
```

The CalEPD library has buffers but no software caching layer to track what changed since last flush.

## Recommended Fix
Implement a simple dirty-region tracker:

1. **Add bitmap to track changed regions**:
```cpp
// Divide screen into 16x16 pixel blocks (8x8 blocks for 128x296 display)
static uint8_t s_dirtyBlocks[8][8];  // 1 = dirty, 0 = clean

static void markDirty(int16_t x, int16_t y, int16_t w, int16_t h) {
    int16_t startX = x / 16;
    int16_t endX = (x + w) / 16;
    int16_t startY = y / 16;
    int16_t endY = (y + h) / 16;
    
    for (int16_t by = startY; by <= endY && by < 8; by++) {
        for (int16_t bx = startX; bx <= endX && bx < 8; bx++) {
            s_dirtyBlocks[by][bx] = 1;
        }
    }
}
```

2. **Wrap drawing methods to mark dirty regions**:
```cpp
void EpaperDisplay::drawPixel(int16_t x, int16_t y, uint16_t color) {
    // Call parent drawPixel
    // ...
    markDirty(x, y, 1, 1);
}

void EpaperDisplay::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    // Call parent drawLine
    // ...
    markDirty(x0, y0, x1-x0, y1-y0);
}
```

3. **Calculate bounding box from dirty blocks on flush**:
```cpp
void EpaperDisplay::flush(RefreshMode mode) {
    if (mode == RefreshMode::PARTIAL) {
        // Find bounding box of dirty regions
        int16_t minX = 0, minY = 0, maxX = WIDTH, maxY = HEIGHT;
        // ... calculate from s_dirtyBlocks
        
        s_epd_display->updateWindow(minX, minY, maxX-minX, maxY-minY, false);
        
        // Clear dirty bitmap
        memset(s_dirtyBlocks, 0, sizeof(s_dirtyBlocks));
    } else {
        s_epd_display->update();
    }
}
```

## References
- E-Paper display timing: https://www.good-display.com/product/389.html
- CalEPD library: https://github.com/ZinggJM/GxEPD2
- Partial update optimization: https://www.waveshare.com/wiki/2.9inch_e-Paper_Module_(B)
