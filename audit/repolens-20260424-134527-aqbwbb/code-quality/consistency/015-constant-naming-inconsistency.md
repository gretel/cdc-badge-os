---
title: "[MEDIUM] Constant Naming Style Inconsistency: kPrefix vs UPPER_SNAKE_CASE"
severity: MEDIUM
domain: code-style
lens: code-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
The codebase uses inconsistent naming conventions for module-level constants:

1. **Most of the codebase (cdc_core, cdc_hal, cdc_ui, cdc_views classes)**: UPPER_SNAKE_CASE
2. **RenderHelpers.h (namespace scope)**: `k` prefix (kFooterHeight, kScrollIndicatorWidth)

### Evidence

**Most of the codebase uses UPPER_SNAKE_CASE:**

```cpp
// components/cdc_core/include/cdc_core/EventBus.h
static constexpr size_t MAX_HANDLERS = 16;
static constexpr size_t DEFAULT_QUEUE_SIZE = 32;

// components/cdc_core/include/cdc_core/ModuleRegistry.h
static constexpr uint8_t MAX_MODULES = 16;
static constexpr uint8_t MAX_MENU_ITEMS = 32;
static constexpr uint8_t MAX_INITIALIZERS = 16;
static constexpr const char* NVS_PREFIX = "mod_";

// components/cdc_core/include/cdc_core/PinManager.h
static constexpr uint8_t BADGE_PIN_MIN = 4;
static constexpr uint8_t BADGE_PIN_MAX = 8;
static constexpr uint8_t PW1_MIN = 6;
static constexpr uint8_t PIN_MAX = 16;
static constexpr uint32_t LOCKOUT_DURATION_MS = 60000;

// components/cdc_hal/include/cdc_hal/ISecureElement.h
static constexpr uint8_t ECC_SLOT_COUNT = 32;
static constexpr uint16_t RMEM_SLOT_COUNT = 512;
static constexpr uint16_t RMEM_SLOT_SIZE = 476;

// components/cdc_ui/include/cdc_ui/I18n.h
static constexpr uint16_t MAX_STRINGS = 512;
static constexpr uint8_t LANG_COUNT = static_cast<uint8_t>(Language::COUNT);

// components/cdc_views/include/cdc_views/ListView.h
static constexpr uint16_t MAX_ITEMS = 512;
static constexpr uint8_t DEFAULT_ITEM_HEIGHT = 18;
static constexpr uint8_t MIN_VISIBLE_ITEMS = 2;
static constexpr uint8_t MAX_VISIBLE_ITEMS = 8;
```

**RenderHelpers.h uses `k` prefix (inconsistent):**

```cpp
// components/cdc_views/include/cdc_views/RenderHelpers.h
constexpr int kFooterHeight = 16;
constexpr int kScrollIndicatorWidth = 8;
```

## Impact
- **Cognitive overhead**: Developers must remember which naming convention to use where
- **Search difficulty**: Searching for constants requires checking both patterns
- **Inconsistent style**: The `k` prefix style is older C++ convention, while UPPER_SNAKE_CASE is more common in modern embedded C++
- **Namespace difference**: `k` prefix is used for namespace-level constants, UPPER_SNAKE_CASE for class-level, but this distinction is not documented

## Recommended Fix

**Standardize on UPPER_SNAKE_CASE** (consistent with the rest of the codebase):

1. Rename constants in `RenderHelpers.h`:
   - `kFooterHeight` → `FOOTER_HEIGHT`
   - `kScrollIndicatorWidth` → `SCROLL_INDICATOR_WIDTH`

2. Update usages in `RenderHelpers.cpp`:
   - Replace `kFooterHeight` with `FOOTER_HEIGHT`
   - Replace `kScrollIndicatorWidth` with `SCROLL_INDICATOR_WIDTH`

### Specific changes needed:

**File: `components/cdc_views/include/cdc_views/RenderHelpers.h`**
```cpp
// BEFORE
constexpr int kFooterHeight = 16;
constexpr int kScrollIndicatorWidth = 8;

// AFTER
constexpr int FOOTER_HEIGHT = 16;
constexpr int SCROLL_INDICATOR_WIDTH = 8;
```

**File: `components/cdc_views/src/RenderHelpers.cpp`**
```cpp
// BEFORE
gfx->fillRect(0, height - kFooterHeight, width, kFooterHeight, EPD_BLACK);
// ...
const int midX = x + (kScrollIndicatorWidth / 2);
const int leftX = x + 1;
const int rightX = x + kScrollIndicatorWidth - 1;
// ...
gfx->fillRect(x, y, kScrollIndicatorWidth, listHeight, EPD_WHITE);
// ...
const int barX = x + (kScrollIndicatorWidth / 2) - 2;

// AFTER
gfx->fillRect(0, height - FOOTER_HEIGHT, width, FOOTER_HEIGHT, EPD_BLACK);
// ...
const int midX = x + (SCROLL_INDICATOR_WIDTH / 2);
const int leftX = x + 1;
const int rightX = x + SCROLL_INDICATOR_WIDTH - 1;
// ...
gfx->fillRect(x, y, SCROLL_INDICATOR_WIDTH, listHeight, EPD_WHITE);
// ...
const int barX = x + (SCROLL_INDICATOR_WIDTH / 2) - 2;
```

## References
- C++ Core Guidelines: Use consistent naming conventions
- Modern C++: UPPER_SNAKE_CASE for constants is more common in embedded/embedded-C++
- Project convention: UPPER_SNAKE_CASE used consistently in cdc_core, cdc_hal, cdc_ui, cdc_views

</content>