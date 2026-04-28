---
title: "[LOW] Date/Time input views lack format examples in footer hints"
severity: LOW
domain: information-architecture
lens: help-context
labels:
  - "date-input"
  - "time-input"
  - "footer-hints"
---

## Summary
The DateInputView and TimeInputView components (components/cdc_views/src/DateInputView.cpp, components/cdc_views/src/TimeInputView.cpp) use footer hints that only show key mappings but don't clarify the expected input format or field order.

**Evidence:**
- DateInputView at `components/cdc_views/src/DateInputView.cpp:197-200`:
```cpp
const char* DateInputView::getFooterHint() const {
    return tr(StringId::HINT_DATE_INPUT);
}
```

- TimeInputView at `components/cdc_views/src/TimeInputView.cpp:154-157`:
```cpp
const char* TimeInputView::getFooterHint() const {
    return tr(StringId::HINT_TIME_INPUT);
}
```

- I18n definitions at `components/cdc_ui/src/I18n.cpp:362-363`:
```cpp
REG(HINT_DATE_INPUT,    "[0-9] [Y] OK [N] Clear",   "[0-9] [Y] OK [N] Loeschen");
REG(HINT_TIME_INPUT,    "[0-9] [Y] OK [N] Clear",   "[0-9] [Y] OK [N] Loeschen");
```

The render methods show the format visually:
- Date: `DD / MM / YYYY` at line 231
- Time: `HH : MM` at line 194

But the footer hints don't reinforce this.

## Impact
Users may:
1. Not know if date format is DD/MM/YYYY or MM/DD/YYYY (common confusion)
2. Not realize time is 24-hour format (00-23 for hours)
3. Enter wrong number of digits without clear format reference
4. Need to guess the expected format order

## Evidence
- Date format in render: `components/cdc_views/src/DateInputView.cpp:231`
```cpp
snprintf(dateStr, sizeof(dateStr), "%02d / %02d / %04d", day_, month_, year_);
```

- Time format in render: `components/cdc_views/src/TimeInputView.cpp:194`
```cpp
snprintf(timeStr, sizeof(timeStr), "%02d : %02d", hour_, minute_);
```

- Footer hints at `components/cdc_ui/src/I18n.cpp:362-363` - only key mappings, no format info

## Recommended Fix
Update the footer hints to include format examples:

**Option 1: Enhanced hints in I18n**
```cpp
// In components/cdc_ui/src/I18n.cpp
REG(HINT_DATE_INPUT,    "[0-9] DD/MM/YYYY [Y] OK",  "[0-9] TT/MM/JJJJ [Y] OK");
REG(HINT_TIME_INPUT,    "[0-9] HH:MM [Y] OK",       "[0-9] HH:MM [Y] OK");
```

**Option 2: View-specific hints**
```cpp
// In DateInputView.cpp
const char* DateInputView::getFooterHint() const {
    static char hint[40];
    snprintf(hint, sizeof(hint), "[0-9] DD/MM/YY [Y] OK");
    return hint;
}

// In TimeInputView.cpp
const char* TimeInputView::getFooterHint() const {
    static char hint[30];
    snprintf(hint, sizeof(hint), "[0-9] HH:MM [Y] OK");
    return hint;
}
```

**Option 3: Add subtitle under title**
```cpp
void DateInputView::render(bool partial) {
    // ... title ...
    
    // Add format subtitle
    gfx->setTextSize(1);
    gfx->setCursor((width - 40) / 2, TITLE_Y + 12);
    gfx->print("Format: DD/MM/YYYY");
    
    // ... rest of render ...
}
```

## References
- DateInputView: `components/cdc_views/src/DateInputView.cpp`
- TimeInputView: `components/cdc_views/src/TimeInputView.cpp`
- I18n hints: `components/cdc_ui/src/I18n.cpp:362-363`
