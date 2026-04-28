---
title: "[MEDIUM] Large stack buffers in E-Paper display methods risk stack overflow"
severity: MEDIUM
domain: performance/memory
lens: memory-management
labels:
  - "stack-overflow"
  - "embedded"
---

## Summary

In `components/CalEPD/epd.cpp`, two methods use large 1024-byte stack-allocated buffers:

1. **`printerf()`** (line 100-112): Uses `char max_buffer[1024]` on stack for formatted output
2. **`draw_centered_text()`** (line 118-161): Uses `char max_buffer[1024]` on stack for formatted text

```cpp
// components/CalEPD/epd.cpp:100-112
void Epd::printerf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    char max_buffer[1024];  // 1KB on stack!
    int size = vsnprintf(max_buffer, sizeof max_buffer, format, args);
    va_end(args);
    // ...
}

// components/CalEPD/epd.cpp:118-127
void Epd::draw_centered_text(const GFXfont *font, int16_t x, int16_t y, uint16_t w, uint16_t h, const char* format, ...) {
    va_list args;
    va_start(args, format);
    char max_buffer[1024];  // 1KB on stack!
    int size = vsnprintf(max_buffer, sizeof max_buffer, format, args);
    va_end(args);
    string text = "";
    if (size < sizeof(max_buffer)) {
      text = std::string(max_buffer);  // Additional heap allocation
    }
    // ...
}
```

Additionally, at line 127, the code creates a `std::string` from the buffer, causing a **double allocation** - first the stack buffer, then a heap allocation for the string.

## Impact

- **Stack overflow risk**: ESP32-S3 has limited stack space per task (default 8192 bytes/8KB). Using 1024 bytes (12.5% of stack) in display methods that may be called from nested contexts increases risk of stack overflow.
- **Memory inefficiency**: `draw_centered_text()` allocates both stack (1024 bytes) AND heap (via `std::string`), wasting memory.
- **Fragmentation**: The `std::string` allocation in `draw_centered_text()` causes heap fragmentation on each call.

### Context
ESP32-S3 typical task stack sizes:
- Main task: 8192 bytes (8KB)
- Priority tasks: 4096-8192 bytes
- Display update may be called from UI task context

With 1KB stack buffer + other local variables + function call chain, stack overflow becomes a real risk.

## Evidence

**File**: `components/CalEPD/epd.cpp`

**Line 100-112** - `printerf()`:
```cpp
void Epd::printerf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    char max_buffer[1024];  // Stack buffer
    int size = vsnprintf(max_buffer, sizeof max_buffer, format, args);
    va_end(args);

    if (size < sizeof(max_buffer)) {
        print(std::string(max_buffer));  // Creates std::string
    } else {
      ESP_LOGE("Epd::printerf", "max_buffer out of range. Increase max_buffer!");
    }
}
```

**Line 118-127** - `draw_centered_text()`:
```cpp
void Epd::draw_centered_text(const GFXfont *font, int16_t x, int16_t y, uint16_t w, uint16_t h, const char* format, ...) {
    va_list args;
    va_start(args, format);
    char max_buffer[1024];  // Stack buffer
    int size = vsnprintf(max_buffer, sizeof max_buffer, format, args);
    va_end(args);
    string text = "";
    if (size < sizeof(max_buffer)) {
      text = std::string(max_buffer);  // Heap allocation on top of stack!
    }
    // ... uses text.c_str()
}
```

## Recommended Fix

### Option 1: Use static buffer (simplest, thread-safe for single display)

```cpp
void Epd::printerf(const char *format, ...) {
    static char buffer[256];  // Static, not on stack
    va_list args;
    va_start(args, format);
    int size = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (size < sizeof(buffer)) {
        print(buffer);  // Direct print, no std::string
    }
}

void Epd::draw_centered_text(const GFXfont *font, int16_t x, int16_t y, uint16_t w, uint16_t h, const char* format, ...) {
    static char buffer[256];  // Static, not on stack
    va_list args;
    va_start(args, format);
    int size = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (size >= sizeof(buffer)) return;

    setFont(font);
    // Use buffer directly, no std::string conversion
    int16_t text_x = 0, text_y = 0;
    uint16_t text_w = 0, text_h = 0;
    getTextBounds(buffer, x, y, &text_x, &text_y, &text_w, &text_h);
    // ... rest of logic
    setCursor(text_x, text_y);
    print(buffer);  // Print directly
}
```

### Option 2: Use PSRAM buffer for larger capacity

```cpp
// In header, declare static buffer
static char s_printBuffer[512];

void Epd::printerf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int size = vsnprintf(s_printBuffer, sizeof(s_printBuffer), format, args);
    va_end(args);

    if (size < sizeof(s_printBuffer)) {
        print(s_printBuffer);
    }
}
```

### Option 3: Reduce buffer size and avoid std::string

For most use cases, 256 bytes is sufficient for display text:

```cpp
void Epd::printerf(const char *format, ...) {
    char buffer[256];  // Smaller stack buffer
    va_list args;
    va_start(args, format);
    int size = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (size < sizeof(buffer)) {
        print(buffer);  // Direct print
    }
}
```

**Key improvements**:
1. Reduce stack usage from 1024 bytes to 256 bytes (75% reduction)
2. Eliminate `std::string` allocation in `draw_centered_text()`
3. Print directly from buffer without intermediate string conversion

## References

- ESP32-S3 Task Stack: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/freertos.html#stack-allocation
- FreeRTOS stack overflow detection: https://www.freertos.org/Stacks-and-stack-overflow-checking.html
- ESP32-S3 SRAM: 512KB total, typically 8KB per task stack

</content>