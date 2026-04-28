---
title: "[LOW] Dead Code: Commented-out buffer data access loops in CalEPD models"
severity: LOW
domain: dead-code
lens: code-quality
labels:
  - "audit:code-quality/dead-code"
---

## Summary
Several CalEPD display model files contain commented-out buffer data access code that appears to be alternative implementations left in place. These blocks are complete statements that were commented out rather than removed.

**Locations identified:**
- `components/CalEPD/models/gdem029E97.cpp:102` - `//uint8_t data = i < sizeof(_mono_buffer) ? _mono_buffer[idx] : 0x00;`
- `components/CalEPD/models/gdew0213i5f.cpp:334` - `//uint8_t data = (idx < sizeof(_buffer)) ? _buffer[idx] : 0x00;`
- `components/CalEPD/models/custom/custom042.cpp:326` - `//uint8_t data = (idx < sizeof(_black_buffer)) ? _black_buffer[idx] : 0x00;`

**Example (gdem029E97.cpp:102):**
```cpp
      //uint8_t data = i < sizeof(_mono_buffer) ? _mono_buffer[idx] : 0x00;
```

**Example (gdew0213i5f.cpp:334):**
```cpp
        //uint8_t data = (idx < sizeof(_buffer)) ? _buffer[idx] : 0x00; // white is 0x00 in buffer
```

## Impact
- **Visual noise**: Commented code blocks add clutter to the source files
- **Confusion**: Developers may wonder which implementation is correct
- **Maintenance**: If the active code changes, these comments may become outdated or misleading

## Evidence
The commented code appears in buffer update/rendering loops where different data access patterns were tried. Each file has a similar pattern:
1. A commented data access line with ternary operator for bounds checking
2. An active data access line below it

Example context from `gdew0213i5f.cpp:334`:
```cpp
        //uint8_t data = (idx < sizeof(_buffer)) ? _buffer[idx] : 0x00; // white is 0x00 in buffer
        uint8_t data = _buffer[idx];  // Active line
```

## Recommended Fix
For each file:
1. **Verify the commented code is not needed** - Check git history to see if it was replaced by the active line
2. **Remove the commented line** if the active implementation is correct
3. **Keep the comment about buffer values** (e.g., "white is 0x00 in buffer") as a standalone comment if useful

Example fix:
```cpp
        // white is 0x00 in buffer
        uint8_t data = _buffer[idx];
```

## References
- Refactoring: Remove redundant commented code
- Code review best practices
