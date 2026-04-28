---
title: "[LOW] Generic loop variable names: excessive use of i, j in non-trivial contexts"
severity: LOW
domain: cdc_core
lens: naming-conventions
labels:
  - "audit:code-quality/naming"
---

## Summary
The codebase uses generic single-letter loop variables (`i`, `j`) in contexts where more descriptive names would improve readability. While `i` is acceptable for simple loop counters, nested loops or complex logic benefit from descriptive names.

**Evidence**:
```cpp
// ModuleRegistry.cpp - nested loops with generic names
for (uint8_t i = 0; i < count_; i++) {
    for (uint8_t j = i + 1; j < count_; j++) {
        if (items[j].priority < items[i].priority) {
            ModuleMenuItem tmp = items[i];
            items[i] = items[j];
            items[j] = tmp;
        }
    }
}

// EventBus.cpp - simple loop with generic name
for (uint8_t i = 0; i < MAX_HANDLERS; i++) {
    if (handlers_[i].active) {
        handlers_[i].handler(event);
    }
}

// KeyFingerprint.cpp - loop with generic name
for (int i = 0; i < KEY_FINGERPRINT_WORD_COUNT; i++) {
    indices[i] = ...;
}
```

## Impact
- **Readability**: In nested loops, `i` and `j` don't convey what is being iterated
- **Maintainability**: When code is modified, it's harder to understand the purpose
- **Clarity**: `index`, `handlerIndex`, `itemIndex` would be clearer than `i`

## Evidence
- File: `components/cdc_core/src/ModuleRegistry.cpp` - nested loops for sorting
- File: `components/cdc_core/src/EventBus.cpp` - loop for handler iteration
- File: `components/cdc_core/src/KeyFingerprint.cpp` - loop for word generation
- Pattern: `for (uint8_t i = 0; i < count_; i++)`, `for (uint8_t j = i + 1; ...)`

## Recommended Fix
Use descriptive loop variable names:
```cpp
// ModuleRegistry.cpp - bubble sort
for (uint8_t currentIndex = 0; currentIndex < totalCount; currentIndex++) {
    for (uint8_t compareIndex = currentIndex + 1; compareIndex < totalCount; compareIndex++) {
        if (items[compareIndex].priority < items[currentIndex].priority) {
            ModuleMenuItem tmp = items[currentIndex];
            items[currentIndex] = items[compareIndex];
            items[compareIndex] = tmp;
        }
    }
}

// EventBus.cpp - handler iteration
for (uint8_t handlerIndex = 0; handlerIndex < MAX_HANDLERS; handlerIndex++) {
    if (handlers_[handlerIndex].active) {
        handlers_[handlerIndex].handler(event);
    }
}

// KeyFingerprint.cpp - word index
for (uint8_t wordIndex = 0; wordIndex < KEY_FINGERPRINT_WORD_COUNT; wordIndex++) {
    indices[wordIndex] = ...;
}
```

## References
- C++ Core Guidelines: [ES.47: Use descriptive names for loop variables](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#es47)
- Clean Code: Robert Martin recommends descriptive variable names
