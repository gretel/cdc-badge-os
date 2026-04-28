---
title: "[LOW] Lazy Class: T9InputView::getChar() and getCharCount() could be static helper functions"
severity: LOW
domain: ui
lens: code-smells
labels:
  - "lazy-class"
  - "cdc_views"
---

## Summary
In `components/cdc_views/src/T9InputView.cpp:81-98`, the methods `getChar()` and `getCharCount()` are instance methods but don't access any instance state. They only use the static `t9_chars` table and their parameters.

## Impact
**Unnecessary indirection**: These methods could be called without an instance.

**Confusing API**: Developers might think these methods depend on instance state when they don't.

**Testing overhead**: Need to create an instance just to test these utility functions.

## Evidence
`components/cdc_views/src/T9InputView.cpp:81-98`:
```cpp
char T9InputView::getChar(char key, uint8_t index) {
    if (key < '0' || key > '9') return '\0';
    const char* chars = t9_chars[key - '0'];
    uint8_t count = strlen(chars);
    return chars[index % count];
}

uint8_t T9InputView::getCharCount(char key) {
    if (key < '0' || key > '9') return 0;
    return strlen(t9_chars[key - '0']);
}
```

Both methods only access:
- The static `t9_chars` array
- Their parameters

No instance variables (`this->`) are accessed.

## Recommended Fix
1. **Make them static**:
```cpp
class T9InputView : public ViewBase {
public:
    static char getChar(char key, uint8_t index);
    static uint8_t getCharCount(char key);
    // ... rest of class
};
```

2. **Or extract to helper class**:
```cpp
class T9Helper {
public:
    static char getChar(char key, uint8_t index);
    static uint8_t getCharCount(char key);
};
```

**Estimated effort**: ~15 minutes to change to static methods.

## References
- Refactoring.com: "Lazy Class" - https://refactoring.com/catalog/extractClass
- Martin Fowler, "Refactoring: Improving the Design of Existing Code", Chapter 7
