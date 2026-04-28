---
title: "[LOW] Missing or incomplete Doxygen documentation"
severity: LOW
domain: linting
lens: code-quality/linting
labels:
  - "audit:code-quality/linting"
---

## Summary
Many public functions and classes lack complete Doxygen documentation. While the project has a `Doxyfile` configured and generates documentation, not all public APIs are documented consistently.

**Location:** Multiple component header files

## Impact
- **Poor discoverability**: Developers need to read implementation to understand usage
- **Onboarding friction**: New contributors struggle to understand the codebase
- **API drift**: Undocumented APIs may change without notice

## Evidence

### File: `components/cdc_core/include/cdc_core/IModule.h`
```cpp
class IModule {
public:
    virtual void register_() = 0;  // No documentation
    virtual void init() = 0;       // No documentation
    virtual void loop() = 0;       // No documentation
};
```

### File: `components/cdc_hal/include/cdc_hal/IKeypad.h`
```cpp
class IKeypad {
public:
    virtual char readKey() = 0;  // No documentation
};
```

### File: `components/cdc_ui/include/cdc_ui/IView.h`
```cpp
class IView {
public:
    virtual void render() = 0;
    virtual InputResult onKey(char key) = 0;
};
```

### File: `components/usb_badge/include/usb_badge/usb_hid.h`
```cpp
bool usb_hid_send(const char* str);  // No documentation
```

### File: `components/serial_cmd/src/SerialCmd.cpp`
```cpp
void SerialCmd::registerCommand(const char* cmd, void (*handler)(const char*));
// Missing: documentation for parameters
```

## Recommended Fix

### 1. Document all public interfaces:
```cpp
/**
 * \brief Interface for CDC modules.
 * 
 * All modules must implement this interface for registration
 * and lifecycle management.
 */
class IModule {
public:
    /**
     * \brief Register the module with the system.
     * 
     * Called during system initialization to register
     * the module's services and views.
     */
    virtual void register_() = 0;

    /**
     * \brief Initialize the module.
     * 
     * Called after all modules are registered. Use for
     * one-time setup like NVS initialization.
     */
    virtual void init() = 0;

    /**
     * \brief Main module loop.
     * 
     * Called periodically for module housekeeping.
     * Keep it fast (< 10ms) to avoid blocking other modules.
     */
    virtual void loop() = 0;
};
```

### 2. Update Doxyfile:
```
# Enable documentation for all files
FILE_PATTERNS = *.h *.cpp

# Warn about undocumented items
WARN_IF_UNDOC_MEMBERS = YES
WARN_IF_UNDOC_CLASS = YES
WARN_IF_UNDOC_ENUM = YES
```

### 3. Add CI check for documentation:
```yaml
- name: Check documentation
  run: |
    doxygen Doxyfile 2>&1 | grep -i "warning" || echo "No warnings"
```

## References
- [Doxygen manual](https://www.doxygen.nl/manual/)
- [C++ Core Guidelines: C.4 - Document all types and functions](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-doc)
- [ESP-IDF documentation guidelines](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/contribute/contribute-guidelines.html#documentation)
