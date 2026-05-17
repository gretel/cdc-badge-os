---
title: "[LOW] Unnecessary std::string allocations in E-Paper display methods"
severity: LOW
domain: performance/memory
lens: memory-management
labels:
  - "heap-allocation"
  - "embedded"
---

## Summary

In `components/CalEPD/epd.cpp`, the `draw_centered_text()` method creates an unnecessary `std::string` object that causes heap allocation on every call:

**Location**: `components/CalEPD/epd.cpp:125-127`

```cpp
void Epd::draw_centered_text(const GFXfont *font, int16_t x, int16_t y, uint16_t w, uint16_t h, const char* format, ...) {
    va_list args;
    va_start(args, format);
    char max_buffer[1024];
    int size = vsnprintf(max_buffer, sizeof max_buffer, format, args);
    va_end(args);
    string text = "";  // Creates std::string object
    if (size < sizeof(max_buffer)) {
      text = std::string(max_buffer);  // Heap allocation!
      // ...
    }
    getTextBounds(text.c_str(), x, y, &text_x, &text_y, &text_w, &text_h);
    // ...
    print(text);  // Uses std::string
}
```

The `std::string` is used only to call `c_str()` for `getTextBounds()` and `print()`. Both methods accept `const char*` directly.

## Impact

- **Heap allocation on every call**: Each call to `draw_centered_text()` allocates heap memory for the string object and its internal buffer
- **Memory fragmentation**: Repeated allocations/deallocations fragment the heap, especially problematic on embedded systems with limited memory
- **Performance overhead**: String construction and destruction add unnecessary CPU cycles
- **Double buffering**: Data is copied from stack buffer to string, then used via `c_str()`

### Context
This method is likely called during display refresh cycles, potentially multiple times per screen update. On ESP32-S3 with limited heap (typically 2-8MB PSRAM + 512KB SRAM), these repeated small allocations add up.

## Evidence

**File**: `components/CalEPD/epd.cpp`

**Line 125-127**:
```cpp
string text = "";
if (size < sizeof(max_buffer)) {
  text = std::string(max_buffer);  // Heap allocation
}
```

**Line 139**: Uses `text.c_str()` - could use `max_buffer` directly:
```cpp
getTextBounds(text.c_str(), x, y, &text_x, &text_y, &text_w, &text_h);
```

**Line 160**: Uses `text` in print - could use `max_buffer` directly:
```cpp
print(text);  // Calls print(const std::string&)
```

**Line 70-76**: The `print(std::string&)` method:
```cpp
void Epd::print(const std::string& text){
   for(auto c : text) {
     if (c==195 || c==194) continue;
     c = _unicodeEasy(c);
     write(uint8_t(c));
   }
}
```

This iterates character-by-character anyway, so passing `const char*` would be more efficient.

## Recommended Fix

### Option 1: Use const char* directly (recommended)

```cpp
void Epd::draw_centered_text(const GFXfont *font, int16_t x, int16_t y, uint16_t w, uint16_t h, const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[256];  // Smaller buffer, static if needed
    int size = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (size < 0 || (size_t)size >= sizeof(buffer)) return;

    setFont(font);
    int16_t text_x = 0, text_y = 0;
    uint16_t text_w = 0, text_h = 0;

    // Use buffer directly, no std::string
    getTextBounds(buffer, x, y, &text_x, &text_y, &text_w, &text_h);

    uint16_t ty = (h/2)+y+(text_h/2);
    if (text_h > (height()/3)) {
      text_x += (w-text_w)/2.2;
      ty -= text_h*1.8;
    } else {
      text_x += (w-text_w)/2;
    }

    setCursor(text_x, ty);
    print(buffer);  // Use const char* version
}
```

### Option 2: Add const char* overload for print()

If the `print()` method needs to handle UTF-8 processing:

```cpp
// In header
void print(const char* text);  // Add new overload

// In cpp
void Epd::print(const char* text){
   while (*text) {
     char c = *text++;
     if (c==195 || c==194) continue;
     c = _unicodeEasy(c);
     write(uint8_t(c));
   }
}

// Then in draw_centered_text:
print(buffer);  // No string allocation
```

### Option 3: Use static buffer for larger text

If 256 bytes is not enough:

```cpp
// In header or cpp (static)
static char s_textBuffer[512];

void Epd::draw_centered_text(...) {
    va_list args;
    va_start(args, format);
    int size = vsnprintf(s_textBuffer, sizeof(s_textBuffer), format, args);
    va_end(args);

    if (size < 0 || (size_t)size >= sizeof(s_textBuffer)) return;

    // Use s_textBuffer directly throughout
    getTextBounds(s_textBuffer, x, y, &text_x, &text_y, &text_w, &text_h);
    // ...
    print(s_textBuffer);
}
```

## References

- ESP32 heap fragmentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/memory.html
- C++ std::string overhead: https://en.cppreference.com/w/cpp/string/basic_string
- Small String Optimization (SSO): Modern C++ strings may avoid heap for short strings (<15 chars), but larger strings always allocate

</content>