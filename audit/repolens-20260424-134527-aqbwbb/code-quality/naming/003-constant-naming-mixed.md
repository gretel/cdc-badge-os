---
title: "[MEDIUM] Inconsistent constant naming: kPrefix vs SCREAMING_SNAKE_CASE"
severity: MEDIUM
domain: cdc_views
lens: naming-conventions
labels:
  - "audit:code-quality/naming"
---

## Summary
The codebase uses two different naming conventions for constants:
1. `k` prefix (Google style): `kFooterHeight`, `kScrollIndicatorWidth` in `RenderHelpers.h`
2. SCREAMING_SNAKE_CASE: `ATTESTATION_ECC_SLOT`, `MAX_MODULES`, `MAX_HANDLERS` throughout the codebase

**Evidence**:
```cpp
// RenderHelpers.h uses kPrefix
constexpr int kFooterHeight = 16;
constexpr int kScrollIndicatorWidth = 8;

// ServiceRegistry.h, ModuleRegistry.h use SCREAMING_SNAKE_CASE
static constexpr size_t MAX_HANDLERS = 16;
static constexpr size_t DEFAULT_QUEUE_SIZE = 32;
static constexpr uint8_t MAX_MODULES = 16;

// EventBus.h also uses SCREAMING_SNAKE_CASE
static constexpr uint32_t eventMask(EventType type) {
    return 1u << static_cast<uint8_t>(type);
}
```

## Impact
- **Consistency**: No single standard for constant naming across the codebase
- **Maintainability**: Developers may be unsure which convention to follow for new constants
- **Code reviews**: Creates unnecessary debate about naming style

## Evidence
- Files: `components/cdc_views/include/cdc_views/RenderHelpers.h`, `components/cdc_core/include/cdc_core/ServiceRegistry.h`, `components/cdc_core/include/cdc_core/ModuleRegistry.h`, `components/cdc_core/include/cdc_core/EventBus.h`
- kPrefix constants: `kFooterHeight`, `kScrollIndicatorWidth`
- SCREAMING_SNAKE_CASE constants: `MAX_HANDLERS`, `DEFAULT_QUEUE_SIZE`, `MAX_MODULES`, `ATTESTATION_ECC_SLOT`, `NVS_PREFIX`

## Recommended Fix
Choose one convention and apply consistently. Given that the majority of the codebase uses SCREAMING_SNAKE_CASE, migrate `k` prefix constants:

```cpp
// Change from:
constexpr int kFooterHeight = 16;
constexpr int kScrollIndicatorWidth = 8;

// To:
constexpr int FOOTER_HEIGHT = 16;
constexpr int SCROLL_INDICATOR_WIDTH = 8;
```

## References
- C++ Core Guidelines: [I6: Capitalize macros](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#I6)
- Google C++ Style Guide: [Constant names](https://google.github.io/styleguide/cppguide.html#Constant_Names) - uses kPrefix
- ESP-IDF convention: Uses SCREAMING_SNAKE_CASE for most constants
