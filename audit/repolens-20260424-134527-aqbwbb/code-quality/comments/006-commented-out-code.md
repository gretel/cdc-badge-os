---
title: "[LOW] Commented-out code blocks in CalEPD component"
severity: LOW
domain: code-quality/comments
lens: comments
labels:
  - "audit:code-quality/comments"
---

## Summary
Commented-out code blocks found in the CalEPD component that should either be deleted or un-commented with explanation:

**File 1**: `components/CalEPD/epdParallel.cpp:98-101`
```cpp
/* 
  // Optionally if we would need to access GFX
  // Adafruit_GFX::setTextColor(color);
 */
```

**File 2**: `components/CalEPD/models/fix/gdeh0213b73.cpp:128-137`
```cpp
  /* for (uint16_t y = 0; y < GDEH0213B73_HEIGHT; y++)
  {
    for (uint16_t x = 0; x < GDEH0213B73_WIDTH / 8; x++)
    {
      uint16_t idx = y * (GDEH0213B73_WIDTH / 8) + x;
      uint8_t data = (idx < sizeof(_buffer)) ? _buffer[idx] : 0x00;
      IO.data(data);
    }
  } */
```

## Impact
- **Dead weight**: Commented code adds to file size and cognitive load
- **Version control**: Git already tracks history; no need to keep commented code for reference
- **Confusion**: Developers may wonder if the code should be used or when to un-comment

## Evidence

**epdParallel.cpp:98-101** - A brief commented block about GFX access:
```cpp
/* 
  // Optionally if we would need to access GFX
  // Adafruit_GFX::setTextColor(color);
 */
```

**gdeh0213b73.cpp:128-137** - Alternative buffer update loop that was replaced:
```cpp
  /* for (uint16_t y = 0; y < GDEH0213B73_HEIGHT; y++)
  {
    for (uint16_t x = 0; x < GDEH0213B73_WIDTH / 8; x++)
    {
      uint16_t idx = y * (GDEH0213B73_WIDTH / 8) + x;
      uint8_t data = (idx < sizeof(_buffer)) ? _buffer[idx] : 0x00;
      IO.data(data);
    }
  } */
```

## Recommended Fix

**Option 1 - Delete** (recommended):
Git history preserves the code. Delete the commented blocks:
```cpp
// Remove these lines entirely from epdParallel.cpp
```

**Option 2 - Add context and move to end of file**:
If the code might be needed later, add a comment with context:
```cpp
// Alternative implementation (replaced for performance):
// Old method sent data byte-by-byte vs. current buffer approach
/*
for (uint16_t y = 0; y < GDEH0213B73_HEIGHT; y++) {
  for (uint16_t x = 0; x < GDEH0213B73_WIDTH / 8; x++) {
    // ...
  }
}
*/
```

## References
- C++ Core Guidelines: "Don't leave commented-out code in the source"
- Git best practices: Use `git log -S` or `git log -p` to find deleted code
