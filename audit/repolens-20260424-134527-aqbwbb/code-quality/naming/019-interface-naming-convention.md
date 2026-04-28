---
title: "[LOW] Interface naming: inconsistent I-prefix convention"
severity: LOW
domain: naming-conventions
lens: code-quality
labels:
  - "audit:code-quality/naming"
---

## Summary
The codebase uses the `I` prefix for interface classes consistently, but there are some naming patterns that could be clarified. The convention is mostly well-applied.

**Evidence:**

1. **components/cdc_hal/include/cdc_hal/II2cBus.h**:
   ```cpp
   class II2cBus : public core::IService {
   ```

2. **components/cdc_hal/include/cdc_hal/IKeypad.h**:
   ```cpp
   class IKeypad : public core::IService {
   ```

3. **components/cdc_hal/include/cdc_hal/ISecureElement.h**:
   ```cpp
   class ISecureElement : public core::IService {
   ```

4. **components/cdc_hal/include/cdc_hal/ISpiBus.h**:
   ```cpp
   // No interface class, just free functions
   spi_host_device_t getSharedSpiHost();
   ```

5. **components/cdc_ui/include/cdc_ui/IView.h**:
   ```cpp
   class IView {
   ```

6. **components/cdc_core/include/cdc_core/IService.h**:
   ```cpp
   class IService {
   ```

7. **components/cdc_core/include/cdc_core/IModule.h**:
   ```cpp
   class IModule {
   ```

8. **components/serial_cmd/include/serial_cmd/ICommandRegistry.h**:
   ```cpp
   class ICommandRegistry {
   ```

## Impact
- **Consistency**: The `I` prefix convention is well-applied across interfaces
- **Clarity**: Clear distinction between interfaces (abstract classes) and implementations
- **Minor issue**: Some interfaces could benefit from clearer naming

## Evidence
The convention is consistent:
- `II2cBus`, `IKeypad`, `ISecureElement`, `ISpiBus` (HAL interfaces)
- `IView`, `I18n` (UI interfaces - note `I18n` breaks convention)
- `IService`, `IModule`, `ICommandRegistry` (core interfaces)

## Recommended Fix
The `I` prefix convention is well-established. Only minor cleanup needed:

**1. Consider renaming `I18n` for consistency:**
```cpp
// Current:
class I18n { ... };

// Option A (keep as-is, it's a common abbreviation):
class I18n { ... };  // OK - widely recognized abbreviation

// Option B (more explicit):
class Intl { ... };  // or: Internationalization
```

**2. Document the convention:**
Add to style guide:
- Abstract interfaces: `I` prefix (e.g., `IView`, `IKeypad`)
- Concrete implementations: No prefix (e.g., `ListView`, `PinManager`)
- Service base classes: `IService` pattern

**Steps:**
1. Decide if `I18n` should follow the `I` prefix convention (it does, just looks different)
2. Document the convention in a style guide
3. No major refactoring needed

## References
- [Google C++ Style Guide - Naming](https://google.github.io/styleguide/cppguide.html#Class_Names)
- [C++ Core Guidelines - Naming](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-naming)
