---
title: "[MEDIUM] T9InputView multi-tap text entry lacks integration tests"
severity: MEDIUM
domain: ui
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:cdc_views"
  - "area:t9-input"
---

## Summary
The `T9InputView` component (`components/cdc_views/include/cdc_views/T9InputView.h`) implements multi-tap text entry (like old cell phones), but **no integration tests** verify correct character cycling, long-press digit insertion, and timeout behavior.

## Impact
- **Character cycling**: Keys may not cycle through characters correctly
- **Long-press**: Digits may not insert on long-press
- **Timeout**: Characters may not commit correctly
- **Special chars**: Underscore, space may not work

## Evidence

**T9InputView API** (`components/cdc_views/include/cdc_views/T9InputView.h:20-116`):
```cpp
class T9InputView : public ViewBase {
    void init(const char* title, const char* initialText = nullptr, uint16_t maxLen = MAX_TEXT_LEN);
    void setOnSave(SaveCallback callback);
    const char* getText() const;
    void forceInsertDigit(char digit);
    
    // Keys: 0-9 = Multi-tap, N = Backspace, Y = Confirm
};
```

**Character mapping** (implicit in `components/cdc_views/src/T9InputView.cpp`):
```
2: ABCabc
3: DEFdef
4: GHIghi
5: JKLjkl
6: MNOmno
7: PQRSpqrs
8: TUVtuv
9: WXYZwxyz
0: Space + special chars
1: Punctuation
```

**Key handling** (`components/cdc_views/src/T9InputView.cpp:50-150`):
```cpp
void T9InputView::onKey(char key) {
    if (key >= '2' && key <= '9') {
        // Multi-tap: cycle through characters
        const char* chars = getCharsForDigit(key);
        int pos = (lastCharPos_ + 1) % strlen(chars);
        text_[len_] = chars[pos];
        lastCharPos_ = pos;
    } else if (key == '0') {
        // Space or special chars
    } else if (key == 'N') {
        // Backspace
    } else if (key == 'Y') {
        // Confirm
        if (onSave_) onSave_(text_);
    }
}

void T9InputView::onLongPress(char key) {
    if (key >= '0' && key <= '9') {
        // Insert digit directly
        forceInsertDigit(key);
    }
}
```

**Usage in modules**:
- `TotpModule` - Account names, issuer names
- `PasswordModule` - Title, username, URL, notes

**Current test coverage**: None

## Recommended Fix

Create integration test `test_t9_input/` that verifies:

1. **Multi-tap cycling**: Keys cycle through correct characters
2. **Long-press digits**: Digits insert directly on long-press
3. **Backspace**: N removes last character
4. **Space**: 0 inserts space
5. **Complete words**: Type words like "hello" correctly
6. **Timeout**: Characters commit after timeout

**Test structure** (example):
```cpp
// test/test_t9_input/test_t9_entry.cpp
#include "cdc_views/T9InputView.h"

static char s_savedText[128];
static void onSave(const char* text) {
    strcpy(s_savedText, text);
}

void test_t9_multi_tap() {
    T9InputView input;
    input.init("Test");
    input.setOnSave(onSave);
    
    // Type "A" (2 once)
    input.onKey('2');
    ASSERT_EQ(input.getText()[0], 'A');
    
    // Type "B" (2 twice)
    input.onKey('2');  // Move to next
    ASSERT_EQ(input.getText()[0], 'B');
}

void test_t9_long_press_digit() {
    T9InputView input;
    input.init("Test");
    
    // Long press 2 inserts '2'
    input.onLongPress('2');
    ASSERT_EQ(input.getText()[0], '2');
}

void test_t9_type_hello() {
    T9InputView input;
    input.init("Test");
    
    // Type "hello"
    // h (4 3 times), e (3 2 times), l (5 2 times), l, o (6 3 times)
    input.onKey('4'); input.onKey('4'); input.onKey('4');  // h
    input.onKey('3'); input.onKey('3');                    // e
    input.onKey('5'); input.onKey('5');                    // l
    input.onKey('5'); input.onKey('5');                    // l
    input.onKey('6'); input.onKey('6'); input.onKey('6');  // o
    
    ASSERT_STREQ(input.getText(), "hello");
}

void test_t9_space() {
    T9InputView input;
    input.init("Test");
    
    input.onKey('0');
    ASSERT_EQ(input.getText()[0], ' ');
}
```

## References
- [T9InputView header](components/cdc_views/include/cdc_views/T9InputView.h)
- [T9InputView implementation](components/cdc_views/src/T9InputView.cpp)
- [Usage in modules](components/mod_totp/src/TotpModule.cpp)

</content>