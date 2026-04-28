---
title: "[LOW] T9InputView::processKey cycles characters but has edge case with empty buffer"
severity: LOW
domain: cdc_views/T9InputView
lens: edge-case-testing
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `T9InputView.cpp` (file: `components/cdc_views/src/T9InputView.cpp:105-137`), the `processKey` function cycles through characters for the same key press, but when the buffer is empty (`len_ == 0`), the cycling logic at line 120 accesses `text_[len_ - 1]` which could wrap around.

Lines 112-121:
```cpp
if (sameKey && !timeout && len_ > 0) {
    // Cycle through characters for the same key
    charIndex_++;
    uint8_t charCount = getCharCount(key);
    if (charIndex_ >= charCount) {
        charIndex_ = 0;
    }
    // Replace last character
    text_[len_ - 1] = getChar(key, charIndex_);  // Line 120
    cursorActive_ = true;
} else {
    // New key or timeout - commit previous and add new
    if (len_ < maxLen_) {
        text_[len_++] = getChar(key, 0);
        text_[len_] = '\0';
        charIndex_ = 0;
        cursorActive_ = true;
    }
}
```

The condition at line 112 checks `len_ > 0`, which protects the access at line 120. However, the condition `sameKey && !timeout && len_ > 0` means:
- On first key press: `len_ == 0`, so the else branch adds the first character
- On second press of same key: `len_ == 1`, so it cycles the first character

The edge case is that the code at line 120 uses `len_ - 1` which is correct when `len_ > 0`, but this assumes `len_` is never 0 when reaching that line. The check at line 112 provides this protection, but the code is fragile.

## Impact
- **Subtle race condition**: If `len_` could be modified by another thread between the check and access, there could be an underflow
- **Maintainability**: The `len_ - 1` pattern is error-prone and could be accidentally broken during refactoring
- **Testing gap**: The exact boundary of `len_ == 0` transitioning to `len_ == 1` may not be fully tested

## Evidence
File: `components/cdc_views/src/T9InputView.cpp`, lines 105-137

```cpp
bool T9InputView::processKey(char key) {
    if (key < '0' || key > '9') return false;

    uint32_t now = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
    bool sameKey = (key == lastKey_);
    bool timeout = (now - lastPressMs_) > TIMEOUT_MS;

    if (sameKey && !timeout && len_ > 0) {  // Line 112
        // Cycle through characters for the same key
        charIndex_++;
        uint8_t charCount = getCharCount(key);
        if (charIndex_ >= charCount) {
            charIndex_ = 0;
        }
        // Replace last character
        text_[len_ - 1] = getChar(key, charIndex_);  // Line 120
        cursorActive_ = true;
    } else {
        // New key or short-circuit when buffer full
        if (len_ < maxLen_) {
            text_[len_++] = getChar(key, 0);
            text_[len_] = '\0';
            charIndex_ = 0;
            cursorActive_ = true;
        }
    }

    lastKey_ = key;
    lastPressMs_ = now;
    dirty_ = true;

    return true;
}
```

The edge case at line 120:
- `text_[len_ - 1]` when `len_ == 0` would wrap to `text_[UINT_MAX]` (array overflow)
- Protected by line 112 check, but fragile

## Recommended Fix
Make the boundary check more explicit and defensive:

```cpp
bool T9InputView::processKey(char key) {
    if (key < '0' || key > '9') return false;

    uint32_t now = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
    bool sameKey = (key == lastKey_);
    bool timeout = (now - lastPressMs_) > TIMEOUT_MS;

    if (sameKey && !timeout && len_ > 0) {
        // Cycle through characters for the same key
        charIndex_++;
        uint8_t charCount = getCharCount(key);
        if (charIndex_ >= charCount) {
            charIndex_ = 0;
        }
        // Replace last character (safe because len_ > 0)
        uint8_t lastIndex = static_cast<uint8_t>(len_ - 1);
        text_[lastIndex] = getChar(key, charIndex_);
        cursorActive_ = true;
    } else {
        // New key or timeout - commit previous and add new
        if (len_ < maxLen_) {
            text_[len_++] = getChar(key, 0);
            text_[len_] = '\0';
            charIndex_ = 0;
            cursorActive_ = true;
        }
    }

    lastKey_ = key;
    lastPressMs_ = now;
    dirty_ = true;

    return true;
}
```

Add explicit unit tests for:
- First key press when `len_ == 0`
- Second press of same key when `len_ == 1`
- Rapid presses of different keys
- Key press after timeout

## References
- CWE-131: Incorrect Calculation of Multi-Byte String Length
- CWE-758: Reference to a Rely-On-Default Memory Location (underflow)
- ESP32-S3 T9 input best practices
