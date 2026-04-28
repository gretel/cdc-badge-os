---
title: "[LOW] Dead Code: Commented case statement in plasticlogic display driver"
severity: LOW
domain: dead-code
lens: code-quality
labels:
  - "audit:code-quality/dead-code"
---

## Summary
In the plasticlogic display driver, a commented-out `case 11:` statement exists in a switch block at `components/CalEPD/models/plasticlogic/plasticlogic.cpp:126`. The case has a comment "Defaults" but no implementation, suggesting incomplete code or an alternative that was disabled.

**Location:** `components/CalEPD/models/plasticlogic/plasticlogic.cpp:126`

```cpp
        // case 11: Defaults
```

## Impact
- **Confusion**: Developers may wonder what "Defaults" refers to and whether this case should be implemented
- **Incomplete refactoring**: Suggests a switch statement that was partially modified

## Evidence
File: `components/CalEPD/models/plasticlogic/plasticlogic.cpp`
- Line 126: `// case 11: Defaults`

This appears in a switch statement that likely handles different display sizes or configurations. The case number "11" likely refers to a 1.1" display size (based on context from other files).

## Recommended Fix
1. **Check if case 11 is still needed** - Verify if size 11 is handled elsewhere
2. **Either implement or remove**:
   - If the default behavior is correct, remove the commented line
   - If case 11 needs special handling, implement it

Example:
```cpp
        switch (size) {
            case 21:  // 2.1"
                // ... implementation
                break;
            case 31:  // 3.1"
                // ... implementation
                break;
            // case 11: Defaults - remove if not needed
        }
```

## References
- Switch statement best practices
- Code cleanup: Remove dead case statements
