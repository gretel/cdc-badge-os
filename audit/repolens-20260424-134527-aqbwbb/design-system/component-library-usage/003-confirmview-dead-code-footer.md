---
title: "[LOW] ConfirmView has dead code: getFooterHint() override never used"
severity: LOW
domain: component-library-usage
lens: ui-components
labels:
  - "audit:design-system/component-library-usage"
---

## Summary
The `ConfirmView` component has a `getFooterHint()` method override in the header that is never actually called during rendering. The footer is hardcoded directly in the `render()` function, making the override dead code.

**Locations:**
- `components/cdc_views/include/cdc_views/ConfirmView.h:57` - Dead `getFooterHint()` override
- `components/cdc_views/src/ConfirmView.cpp:185` - Hardcoded footer in render function

## Impact
1. **Dead Code Confusion**: Developers reading the header will see `getFooterHint()` returning `"Y=OK  N=Abbruch"`, but the actual rendered footer shows `"Y=Ja  N=Nein"`.

2. **Maintenance Burden**: If someone tries to modify the footer by changing `getFooterHint()`, their changes won't take effect.

3. **Inconsistent Pattern**: Other views like `ListView`, `PinEntryView`, `SliderView` use `getFooterHint()` and call it from render, but `ConfirmView` breaks this pattern.

## Evidence
**ConfirmView.h (line 57) - The override:**
```cpp
const char* getFooterHint() const override { return "Y=OK  N=Abbruch"; }
```

**ConfirmView.cpp (line 185) - What's actually rendered:**
```cpp
gfx->print("Y=Ja  N=Nein");
```

**Compare with ListView.cpp (correct pattern):**
```cpp
// Line 173-177
const char* ListView::getFooterHint() const {
    return tr(StringId::HINT_OK_BACK);
}

// Line 268 - Called from render
const char* hint = getFooterHint();
if (hint) {
    render::drawFooterBar(gfx, width, height, nullptr, hint);
}
```

**ConfirmView.cpp render function (no call to getFooterHint):**
```cpp
// Lines 183-186 - Direct hardcoding, no getFooterHint() call
gfx->setCursor(boxX + BOX_WIDTH / 2 - 30, boxY + BOX_HEIGHT - 12);
gfx->print("Y=Ja  N=Nein");
```

## Recommended Fix
**Option 1: Use getFooterHint() pattern (recommended)**

1. Update `ConfirmView.h` to use I18n:
```cpp
const char* getFooterHint() const override { return tr(StringId::HINT_APPROVE_DENY); }
```

2. Update `ConfirmView.cpp` render function (around line 183):
```cpp
const char* hint = getFooterHint();
gfx->setCursor(boxX + BOX_WIDTH / 2 - 30, boxY + BOX_HEIGHT - 12);
gfx->print(hint);
```

**Option 2: Remove dead code**

If the inline return is preferred for simplicity, remove the override from header and hardcode in render:
```cpp
// Remove from ConfirmView.h:57
// const char* getFooterHint() const override { return "Y=OK  N=Abbruch"; }
```

**Recommended: Option 1** for consistency with other views and to enable I18n.

## References
- `components/cdc_views/src/ListView.cpp:173,268` - Correct getFooterHint() pattern
- `components/cdc_views/src/PinEntryView.cpp:218,295` - Another correct example
- `components/cdc_views/include/cdc_views/ConfirmView.h:57` - Dead code location
