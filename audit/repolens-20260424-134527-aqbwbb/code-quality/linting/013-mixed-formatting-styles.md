---
title: "[LOW] Mixed indentation styles in component files"
severity: LOW
domain: linting
lens: code-quality/linting
labels:
  - "audit:code-quality/linting"
---

## Summary
The codebase has inconsistent indentation styles across different components. Some files use 2-space indentation, others use 4-space, and some mix tabs and spaces.

**Location:** Multiple component directories

## Impact
- **Code review friction**: Diff noise from whitespace changes
- **Readability**: Inconsistent indentation makes code harder to read
- **Collaboration**: Different developers may use different editors with different defaults

## Evidence

### File: `components/CalEPD/epd.cpp` (4-space indentation)
```cpp
void Epd::cmd(uint8_t cmd) {
    IO.cmd(cmd);  // 4 spaces
}
```

### File: `components/Adafruit-GFX/Adafruit_GFX.cpp` (2-space indentation)
```cpp
void Adafruit_GFX::drawFastVLine(int16_t x, int16_t y, int16_t h) {
  drawFastLine(x, y, h, 0);  // 2 spaces
}
```

### File: `components/cdc_core/src/ServiceRegistry.cpp` (4-space indentation)
```cpp
void ServiceRegistry::registerService(ServiceType type, IService* service) {
    services_[type] = service;  // 4 spaces
}
```

### File: `components/mod_totp/src/TotpModule.cpp` (4-space indentation)
```cpp
void TotpModule::register_(void) {
    using namespace cdc::core;  // 4 spaces
}
```

The `.clang-format` files in third-party directories have different configurations:
- `third_party/libtropic/.clang-format`: Uses 4-space, Google style
- `managed_components/espressif__tinyusb/.clang-format`: Uses 2-space, LLVM style

## Recommended Fix

1. **Create a root-level `.clang-format`** (see related issue #001)

2. **Apply consistent formatting**:
   ```bash
   # Find all C++ files
   find components main -name "*.cpp" -o -name "*.h" | xargs clang-format -i
   ```

3. **Configure editor settings**:
   - Add `.editorconfig` file at root:
     ```ini
     [*.cpp]
     indent_style = space
     indent_size = 4
     end_of_line = lf
     insert_final_newline = true
     
     [*.h]
     indent_style = space
     indent_size = 4
     end_of_line = lf
     insert_final_newline = true
     ```

4. **Add formatting check to CI** (see related issue #003)

## References
- [EditorConfig](https://editorconfig.org/)
- [C++ Core Guidelines: Formatting](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-format)
- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
