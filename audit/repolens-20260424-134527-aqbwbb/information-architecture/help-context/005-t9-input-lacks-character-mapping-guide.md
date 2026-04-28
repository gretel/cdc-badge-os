---
title: "[LOW] T9 input view lacks character mapping guidance"
severity: LOW
domain: information-architecture
lens: help-context
labels:
  - "t9-input"
  - "text-entry"
  - "inline-help"
---

## Summary
The T9InputView component (components/cdc_views/src/T9InputView.cpp) uses multi-tap text input but provides no guidance on how the character mapping works, making it difficult for users to understand which characters are available on each key.

**Evidence:**
- File: `components/cdc_views/src/T9InputView.cpp:18-27`
- T9 character mapping at lines 18-27:
```cpp
static const char* t9_chars[] = {
    " 0",                           // 0 - space, 0
    ".?!,;:'\"()-_@#$%&*+=/\\1",   // 1 - symbols, 1
    "abc2ABC",                      // 2
    "def3DEF",                      // 3
    "ghi4GHI",                      // 4
    "jkl5JKL",                      // 5
    "mno6MNO",                      // 6
    "pqrs7PQRS",                    // 7
    "tuv8TUV",                      // 8
    "wxyz9WXYZ"                     // 9
};
```

- Footer hint at line 254-257:
```cpp
const char* T9InputView::getFooterHint() const {
    return tr(StringId::HINT_T9_INPUT);
}
```

- I18n string at `components/cdc_ui/src/I18n.cpp:358`:
```cpp
REG(HINT_T9_INPUT,      "[0-9] T9 [Y] OK",      "[0-9] T9 [Y] OK");
```

The hint only says "T9" but doesn't explain:
- Repeated key press cycles through characters
- Case sensitivity (abc vs ABC)
- Which symbols are on which key

## Impact
Users may:
1. Not know how to access symbols (key 1 has many symbols)
2. Be confused about uppercase vs lowercase letters
3. Not understand the multi-tap cycling behavior
4. Get frustrated trying to find specific characters
5. Need external help to figure out the input method

## Evidence
- T9 mapping table: `components/cdc_views/src/T9InputView.cpp:18-27`
- Multi-tap logic at lines 100-131:
```cpp
bool T9InputView::processKey(char key) {
    // ...
    if (sameKey && !timeout && len_ > 0) {
        // Cycle through characters for the same key
        charIndex_++;
        // ...
    }
    // ...
}
```

- Footer hint at line 254-257
- I18n at `components/cdc_ui/src/I18n.cpp:358`

## Recommended Fix
Add a help view or tooltip explaining T9 input:

**Option 1: Show info on first use**
```cpp
void T9InputView::onEnter(void* context) {
    (void)context;
    // Check if first time (via NVS or static flag)
    static bool shownHelp = false;
    if (!shownHelp) {
        shownHelp = true;
        const char* help = "T9 Input\n\n"
            "Press key multiple times\n"
            "to cycle characters.\n\n"
            "1 = Symbols\n"
            "2-9 = Letters\n\n"
            "Long-press N = Clear all";
        showInfo("T9 Help", help);
    }
    // ... rest of init ...
}
```

**Option 2: Add to footer hint (short version)**
```cpp
const char* T9InputView::getFooterHint() const {
    return tr(StringId::HINT_T9_INPUT_SHORT);  // New string: "[0-9] T9 [Y] OK [N] Back"
}
```

**Option 3: Show character map in view**
```cpp
void T9InputView::render(bool partial) {
    // ... existing render ...
    
    // Add small character map at bottom (above footer)
    gfx->setTextSize(1);
    gfx->setCursor(10, height - 25);
    gfx->print("2:abc 3:def 4:ghi 5:jkl");
    gfx->setCursor(10, height - 15);
    gfx->print("6:mno 7:pqrs 8:tuv 9:wxy");
}
```

**Option 4: Context menu help**
Add a help item to the context menu (key 3) when in T9 input mode:
```cpp
// In onKey for T9InputView
if (key == '3') {
    static ContextMenuItem items[] = {
        {"Help", []() { showT9Help(); }},
        // ... other items ...
    };
    showContextMenu("Actions", items, 2);
}
```

## References
- T9InputView: `components/cdc_views/src/T9InputView.cpp`
- I18n hints: `components/cdc_ui/src/I18n.cpp:358`
- Similar pattern: ContextMenuView at `components/cdc_views/src/ContextMenuView.cpp`
