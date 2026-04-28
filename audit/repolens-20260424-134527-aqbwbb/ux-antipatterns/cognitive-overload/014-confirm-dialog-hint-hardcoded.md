---
title: "[LOW] ConfirmView uses hardcoded German hint text"
severity: LOW
domain: UI/UX
lens: cognitive-overload
labels:
  - "internationalization"
  - "discoverability"
---

## Summary
The `ConfirmView` in `components/cdc_views/src/ConfirmView.cpp` displays a hardcoded German footer hint "Y=Ja N=Nein" regardless of the current UI language setting, which may confuse English users.

**File:** `components/cdc_views/src/ConfirmView.cpp`
**Lines:** 185-190 (render function)

## Impact
- **Language Inconsistency:** Users switching to English still see German hints
- **Cognitive Dissonance:** The mismatch between UI language and action labels creates confusion
- **Discoverability:** Users may not understand what Y/N mean if they don't know German

## Evidence
The footer hint is hardcoded in German:

```cpp
// Line 185-190: Hardcoded German text
gfx->setCursor(boxX + BOX_WIDTH / 2 - 30, boxY + BOX_HEIGHT - 12);
gfx->print("Y=Ja N=Nein");  // Always German, ignores I18n!
```

Compare to other views that use localized strings:

```cpp
// ListView.cpp uses tr() for footer hint
const char* hint = getFooterHint();
render::drawFooterBar(gfx, width, height, prefix, hint, true);

// DateInputView uses tr()
const char* hint = getFooterHint();
render::drawFooterBar(gfx, width, height, nullptr, hint, false);
```

The ConfirmView should use `tr(StringId::...)` or similar I18n mechanism.

## Recommended Fix
Add localized string for confirm dialog hint and use it:

```cpp
// Add to i18n strings (e.g., components/cdc_ui/src/I18n.cpp):
// English: "Y=Yes N=No"
// German: "Y=Ja N=Nein"
static const char* confirmHint[] = {
    [Language::EN] = "Y=Yes N=No",
    [Language::DE] = "Y=Ja N=Nein"
};

// In ConfirmView::render():
const char* hint = tr(StringId::HINT_CONFIRM);  // Or direct lookup
gfx->setCursor(boxX + BOX_WIDTH / 2 - 30, boxY + BOX_HEIGHT - 12);
gfx->print(hint);
```

Or simpler, add a `getFooterHint()` method like other views:

```cpp
const char* ConfirmView::getFooterHint() const {
    return tr(StringId::HINT_CONFIRM);  // Y=Yes N=No / Y=Ja N=Nein
}

// In render():
const char* hint = getFooterHint();
gfx->setCursor(boxX + BOX_WIDTH / 2 - 30, boxY + BOX_HEIGHT - 12);
gfx->print(hint);
```

## References
- Internationalization: All user-facing text should be localized
- Consistency: UI hints should match the active language
- Cognitive Load: Inconsistent languages require extra mental processing
