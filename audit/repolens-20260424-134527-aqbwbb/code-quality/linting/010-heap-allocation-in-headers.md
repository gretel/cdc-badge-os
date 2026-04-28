---
title: "[MEDIUM] Heap allocation in header file constructors"
severity: MEDIUM
domain: linting
lens: code-quality/linting
labels:
  - "audit:code-quality/linting"
---

## Summary
Several header files contain heap allocation (`heap_caps_malloc`) in class member initialization, which mixes memory management with class definition and can lead to memory leaks if not properly freed.

**Location:** Multiple header files in `components/CalEPD/include/`

## Impact
- **Memory leak risk**: If destructors don't free the allocated memory, it will leak
- **Constructor exception safety**: If allocation fails or a later constructor throws, memory may leak
- **Hidden side effects**: Memory allocation in header files is not obvious to readers
- **Testing difficulty**: Harder to mock or test classes with implicit heap allocation

## Evidence

### File: `components/CalEPD/include/wave12i48.h`
```cpp
class Wave12i48 {
private:
    uint8_t* _buffer = (uint8_t*)heap_caps_malloc(WAVE12i48_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    // ...
};
```

### File: `components/CalEPD/include/wave12i48BR.h`
```cpp
class Wave12i48BR {
private:
    uint8_t* _buffer_black = (uint8_t*)heap_caps_malloc(WAVE12I48_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    uint8_t* _buffer_red = (uint8_t*)heap_caps_malloc(WAVE12I48_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    // ...
};
```

### File: `components/CalEPD/include/color/gdey073d46.h`
```cpp
class Gdey073D46 {
private:
    uint8_t* _buffer = (uint8_t*)heap_caps_malloc(GDEY073D46_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    // ...
};
```

### File: `components/CalEPD/include/gdew075T7Grays.h`
```cpp
class Gdew075T7Grays {
private:
    uint8_t* _buffer = (uint8_t*)heap_caps_malloc(GDEW075T7_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    // ...
};
```

**Note:** Some files have the allocation commented out, showing inconsistency:
- `components/CalEPD/include/gdew075T7.h` (commented)
- `components/CalEPD/include/goodisplay/gdey075T7.h` (commented)

## Recommended Fix

### Option 1: Move allocation to constructor with RAII
```cpp
// In header:
class Wave12i48 {
private:
    uint8_t* _buffer;
    
public:
    Wave12i48();
    ~Wave12i48();
    // ...
};

// In source:
Wave12i48::Wave12i48() 
    : _buffer(static_cast<uint8_t*>(heap_caps_malloc(WAVE12I48_BUFFER_SIZE, MALLOC_CAP_SPIRAM))) {
    if (!_buffer) {
        LOG_E("Wave12i48", "Failed to allocate buffer");
    }
}

Wave12i48::~Wave12i48() {
    if (_buffer) {
        heap_caps_free(_buffer);
    }
}
```

### Option 2: Use std::unique_ptr with custom deleter
```cpp
// In header:
#include <memory>

class Wave12i48 {
private:
    std::unique_ptr<uint8_t[], decltype(&heap_caps_free)> _buffer{
        nullptr, &heap_caps_free
    };
    
public:
    Wave12i48() {
        _buffer.reset(static_cast<uint8_t*>(
            heap_caps_malloc(WAVE12I48_BUFFER_SIZE, MALLOC_CAP_SPIRAM)
        ));
    }
    // No need for destructor - unique_ptr handles cleanup
};
```

### Option 3: Use heap_caps_calloc for zero-initialization and cleaner code
```cpp
// In constructor:
_buffer = static_cast<uint8_t*>(heap_caps_calloc(
    1, WAVE12I48_BUFFER_SIZE, MALLOC_CAP_SPIRAM
));
```

## References
- [C++ Core Guidelines: C.6 - Put constructors and destructors in the class](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-constructor)
- [C++ Core Guidelines: R.10 - Use unique_ptr for exclusive ownership](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rr-unique-ptr)
- [ESP32 memory management](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/mem_alloc.html)
