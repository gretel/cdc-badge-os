---
title: "[LOW] Tight loops in E-Paper display buffer initialization"
severity: LOW
domain: performance
lens: blocking-io
labels:
  - "performance"
  - "display"
  - "epaper"
---

## Summary

E-Paper display model files use tight loops to initialize LUT (Look-Up Table) arrays and configuration buffers. These loops run synchronously during display initialization and update phases, blocking the calling task.

**Affected files:**
- `components/CalEPD/models/wave12i48.cpp` (lines 90-102)
- `components/CalEPD/models/color/wave12i48BR.cpp` (lines 214-226)
- `components/CalEPD/models/gdew042t2.cpp` (lines 230-250)
- `components/CalEPD/models/color/gdew027c44.cpp` (lines 128-162)

**Evidence:**
```cpp
// components/CalEPD/models/wave12i48.cpp:90-102
for (int i=0;i<epd_resolution_m1s2.databytes;++i) {
    epd_resolution_m1s2.data[i] = 0x60;
}
for (int i=0;i<epd_resolution_m2s1.databytes;++i) {
    epd_resolution_m2s1.data[i] = 0x30;
}
// ... more loops

// components/CalEPD/models/color/gdew027c44.cpp:128-162
for (int i=0;i<epd_soft_start.databytes;++i) {
    epd_soft_start.data[i] = 0x00;
}
for (int i=0;i<lut_20_vcomDC.databytes;++i) {
    lut_20_vcomDC.data[i] = 0x00;
}
for (int i=0;i<lut_21.databytes;++i) {
    lut_21.data[i] = 0x00;
}
// ... more LUT initialization loops
```

**Pattern in QR code rendering:**
```cpp
// components/cdc_views/src/QRCodeView.cpp:60-75
for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
        bool black = esp_qrcode_get_module(qrcode, x, y);
        uint16_t color = black ? EPD_BLACK : EPD_WHITE;

        // Draw scaled pixel
        for (int dy = 0; dy < scale; dy++) {
            for (int dx = 0; dx < scale; dx++) {
                s_qrCtx.display->drawPixel(x0 + x * scale + dx, y0 + y * scale + dy, color);
            }
        }
    }
}
```

## Impact

1. **Initialization blocking**: Display initialization loops run synchronously, blocking for 5-20ms depending on LUT size.

2. **QR rendering blocking**: Nested loops in QR rendering (size x size x scale x scale) can result in thousands of iterations for large QR codes.

3. **No task yielding**: Loops run to completion without allowing other tasks to run.

4. **Memory bandwidth**: Sequential memory writes are efficient but still consume CPU cycles.

## Evidence

**Loop iteration counts:**
- LUT initialization: 40-210 bytes per LUT (typical E-Paper LUT size)
- QR rendering: 97 x 97 x scale x scale = up to 9,409 pixels for v20 QR at scale 1

**Blocking time estimates:**
- LUT init loop: ~1-5ms (simple assignments)
- QR rendering: ~10-50ms (nested loops with drawPixel calls)

## Recommended Fix

1. **Use memset for simple initialization** (faster, ~15 min):
```cpp
// Instead of loop
for (int i=0; i<databytes; ++i) {
    data[i] = 0x00;
}

// Use memset
memset(data, 0x00, databytes);

// For repeated patterns
for (int i=0; i<databytes; ++i) {
    data[i] = 0x30;
}

// Use memset with byte pattern
for (int i=0; i<databytes; ++i) {
    data[i] = 0x30;
}
// Can be optimized by compiler to memset-like operation
```

2. **Pre-compute LUTs at compile time** (best for static data):
```cpp
// Instead of runtime initialization
static const uint8_t lut_vcom[] = {
    // Pre-computed values
    0x00, 0x10, 0x20, ...
};

// Use static const array
static constexpr uint8_t LUT_VCOM[] = {
    0x00, 0x10, 0x20, ...
};
```

3. **Add yield points in QR rendering** (~30 min):
```cpp
for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
        // ... draw pixel ...
    }
    // Yield every 10 rows
    if (y % 10 == 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
```

4. **Batch pixel drawing** (reduce function call overhead):
```cpp
// Draw multiple pixels at once
void drawBlock(int x, int y, int width, int height, uint16_t color) {
    // Optimized block fill
    for (int dy = 0; dy < height; dy++) {
        for (int dx = 0; dx < width; dx++) {
            drawPixel(x + dx, y + dy, color);
        }
    }
}
```

**Estimated effort**: 30-60 minutes for initial optimizations

## References

- E-Paper LUT tables: 40-210 bytes typical
- QR code rendering: O(n²) where n is QR module count (21-97)
- ESP32 memset: ~10-20 cycles per byte for small sizes

</content>