---
title: "[HIGH] Large E-Paper frame buffers allocated without corresponding free/destructor"
severity: HIGH
domain: performance/memory
lens: memory-management
labels:
  - "memory-leak"
  - "embedded"
  - "large-allocation"
---

## Summary

Multiple E-Paper display classes in `components/CalEPD/` allocate large frame buffers (60KB-320KB) using `heap_caps_malloc` in header file member initializers, but have no destructor to free them. This causes significant memory to be permanently allocated for the application lifetime.

**Locations**:
- `components/CalEPD/include/wave12i48.h:55` - 160KB buffer
- `components/CalEPD/include/wave12i48BR.h:60-61` - 320KB total (2 x 160KB)
- `components/CalEPD/include/color/gdey073d46.h:41` - 126KB buffer
- `components/CalEPD/include/gdew075T7Grays.h:56` - 116KB buffer

```cpp
// wave12i48.h:55 - 160KB buffer
uint8_t* _buffer = (uint8_t*)heap_caps_malloc(WAVE12I48_BUFFER_SIZE, MALLOC_CAP_SPIRAM);

// wave12i48BR.h:60-61 - 320KB total (two buffers)
uint8_t* _buffer_black = (uint8_t*)heap_caps_malloc(WAVE12I48_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
uint8_t* _buffer_red = (uint8_t*)heap_caps_malloc(WAVE12I48_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
```

No corresponding `free()` or destructor found in any of these classes.

## Impact

- **Large memory leak**: Each display class instance permanently consumes 60KB-320KB of PSRAM.
- **Multiple instances**: If multiple display models are included in the build, total leak could exceed 500KB.
- **Embedded context**: ESP32-S3 typically has 2-8MB PSRAM. Losing 320KB+ (16-32%) on startup is significant.
- **No recovery**: Memory is allocated at class instantiation and never freed, even if the display is dormant.

### Memory Breakdown

| Display Class | Buffer Size | Total |
|---------------|-------------|-------|
| Wave12I48 | 1304×984/8 = 160,488 bytes | ~160KB |
| Wave12I48RB (black + red) | 2 × 160,488 bytes | ~320KB |
| GDEY073D46 | 800×480/8 = 48,000 bytes | ~48KB |
| GDEW075T7Grays | 800×480 = 38,400 bytes | ~38KB |

**Note**: The actual display used (CDC Badge v1.0 uses ED047TC1) may not need all these buffers, but if multiple display classes are compiled in, their static buffers consume memory.

## Evidence

**File**: `components/CalEPD/include/wave12i48.h`

**Line 54-55**:
```cpp
//uint8_t _buffer[WAVE12I48_BUFFER_SIZE];
uint8_t* _buffer = (uint8_t*)heap_caps_malloc(WAVE12I48_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
```

**File**: `components/CalEPD/include/wave12i48BR.h`

**Line 60-61**:
```cpp
uint8_t* _buffer_black = (uint8_t*)heap_caps_malloc(WAVE12I48_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
uint8_t* _buffer_red = (uint8_t*)heap_caps_malloc(WAVE12I48_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
```

**File**: `components/CalEPD/include/color/gdey073d46.h`

**Line 41**:
```cpp
uint8_t* _buffer = (uint8_t*)heap_caps_malloc(GDEY073D46_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
```

**No destructor found**: None of these classes have a destructor to free the buffers.

## Recommended Fix

### Option 1: Add destructor to each display class (recommended)

```cpp
// In wave12i48.h
class Wave12I48 : public Epd {
public:
    ~Wave12I48();  // Add destructor
    // ... rest of class
};

// In wave12i48.cpp
Wave12I48::~Wave12I48() {
    if (_buffer) {
        free(_buffer);
        _buffer = nullptr;
    }
}
```

```cpp
// In wave12i48BR.h
class Wave12I48RB : public Epd {
public:
    ~Wave12I48RB();  // Add destructor
    // ... rest of class
};

// In wave12i48BR.cpp (or header)
Wave12I48RB::~Wave12I48RB() {
    if (_buffer_black) {
        free(_buffer_black);
        _buffer_black = nullptr;
    }
    if (_buffer_red) {
        free(_buffer_red);
        _buffer_red = nullptr;
    }
}
```

### Option 2: Use static buffers with max size (most efficient)

For the specific display model used (ED047TC1), pre-allocate a single static buffer:

```cpp
// In epdParallel.h or specific display header
class EpdParallel : public Epd {
private:
    static uint8_t s_buffer[ED047TC1_WIDTH * ED047TC1_HEIGHT / 2];
    // ...
};
```

### Option 3: Lazy allocation with cleanup method

Allocate buffer when first needed, provide explicit cleanup:

```cpp
class Wave12I48 {
public:
    void allocateBuffer();  // Allocate on first use
    void freeBuffer();      // Explicit cleanup
    // ...
private:
    uint8_t* _buffer = nullptr;
};
```

### Option 4: Conditional compilation

Only compile buffer for the display model actually used:

```cpp
#ifdef USE_WAVE12I48
    uint8_t* _buffer = (uint8_t*)heap_caps_malloc(WAVE12I48_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
#endif
```

## Priority

**HIGH** because:
1. Large memory footprint (160-320KB per display class)
2. Permanent leak for application lifetime
3. Significant percentage of available PSRAM
4. Easy to fix with destructor

## References

- ESP32-S3 PSRAM: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/memory.html
- heap_caps_malloc: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/memory.html#multi-capability-allocation
- CalEPD library: https://github.com/ZinggJM/GxEPD2 (similar patterns)

</content>