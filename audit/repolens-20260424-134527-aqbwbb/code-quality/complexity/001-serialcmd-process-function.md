---
title: "[HIGH] High cyclomatic complexity in SerialCmd::process() function"
severity: HIGH
domain: Code Quality
lens: cyclomatic-complexity
labels:
  - "audit:code-quality/complexity"
---

## Summary

The `SerialCmd::process()` function in `components/serial_cmd/src/SerialCmd.cpp` (lines 1217-1318) has high cyclomatic complexity due to deeply nested branching logic handling escape sequences, special characters, and input buffering. The function contains:

- 2 levels of nesting for escape sequence handling (`EscState::ESC` → `EscState::BRACKET`)
- Multiple switch statements with 7+ cases each
- Complex conditional logic for history navigation with nested `if/else` chains

**Estimated Cyclomatic Complexity: ~15-18** (threshold is 10)

## Impact

**Maintenance Burden:**
- Understanding all execution paths requires tracking multiple state machines simultaneously
- Adding new escape sequences or special keys requires modifications across nested branches
- Bug fixes may inadvertently affect unrelated input handling paths

**Readability:**
- Function spans ~100 lines requiring scrolling to see complete logic
- State transitions are implicit through `s_escState` global variable
- History navigation logic is duplicated across cases

## Evidence

**File:** `components/serial_cmd/src/SerialCmd.cpp:1217-1318`

**Code excerpt showing nesting:**
```cpp
bool SerialCmd::process() {
    int c = Console::getchar();
    if (c < 0) return false;

    // Handle escape sequences (arrow keys) - Level 1
    if (s_escState == EscState::ESC) {
        if (c == '[') {
            s_escState = EscState::BRACKET;
            return false;
        }
        s_escState = EscState::NONE;
    } else if (s_escState == EscState::BRACKET) {
        s_escState = EscState::NONE;
        switch (c) {  // Level 2 + switch
            case 'A':  // Arrow up
                if (s_historyPos < s_historyCount) {  // Level 3
                    const char* hist = historyGet(s_historyPos);
                    if (hist) {  // Level 4
                        redrawLine(hist, s_cmdBufferPos);
                        s_historyPos++;
                    }
                }
                return false;
            case 'B':  // Arrow down
                if (s_historyPos > 0) {
                    s_historyPos--;
                    if (s_historyPos == 0) {
                        redrawLine("", s_cmdBufferPos);
                    } else {
                        const char* hist = historyGet(s_historyPos - 1);
                        if (hist) {
                            redrawLine(hist, s_cmdBufferPos);
                        }
                    }
                }
                return false;
            default:
                return false;
        }
    }

    // Handle special characters - Another switch with 7 cases
    switch (c) {
        case 0x1B:  // ESC
        case '\r':
        case '\n':
        case 0x7F:
        case 0x08:
        case 0x03:
        case 0x15:
        default:
    }
}
```

**Branching count:**
- Line 1223: `if (s_escState == EscState::ESC)` (+1)
- Line 1224: `if (c == '[')` (+1)
- Line 1230: `else if (s_escState == EscState::BRACKET)` (+1)
- Line 1232: `switch (c)` with 4 cases (+3)
- Line 1234: `if (s_historyPos < s_historyCount)` (+1)
- Line 1236: `if (hist)` (+1)
- Line 1242: `if (s_historyPos > 0)` (+1)
- Line 1244: `if (s_historyPos == 0)` (+1)
- Line 1248: `if (hist)` (+1)
- Line 1258: `switch (c)` with 7 cases (+6)
- Line 1287: `if (s_cmdBufferPos > 0)` (+1)
- Line 1303: `if (c >= 0x20 && c < 0x7F && s_cmdBufferPos < CMD_BUFFER_SIZE - 1)` (+1)

Total: ~18 independent paths

## Recommended Fix

**Refactor into smaller functions:**

1. Extract escape sequence handling:
```cpp
static bool handleEscapeSequence(int c) {
    if (s_escState == EscState::ESC) {
        if (c == '[') {
            s_escState = EscState::BRACKET;
            return false;
        }
        s_escState = EscState::NONE;
    } else if (s_escState == EscState::BRACKET) {
        return handleBracketSequence(c);
    }
    return false;
}

static bool handleBracketSequence(int c) {
    s_escState = EscState::NONE;
    switch (c) {
        case 'A': handleHistoryUp(); break;
        case 'B': handleHistoryDown(); break;
        default: return false;
    }
    return false;
}

static void handleHistoryUp() {
    if (s_historyPos >= s_historyCount) return;
    const char* hist = historyGet(s_historyPos);
    if (hist) {
        redrawLine(hist, s_cmdBufferPos);
        s_historyPos++;
    }
}

static void handleHistoryDown() {
    if (s_historyPos == 0) {
        redrawLine("", s_cmdBufferPos);
        return;
    }
    s_historyPos--;
    const char* hist = historyGet(s_historyPos - 1);
    if (hist) {
        redrawLine(hist, s_cmdBufferPos);
    }
}
```

2. Extract special character handling:
```cpp
static bool handleSpecialChar(int c) {
    switch (c) {
        case 0x1B:
            s_escState = EscState::ESC;
            return false;
        case '\r':
        case '\n':
            return handleEnterKey();
        case 0x7F:
        case 0x08:
            return handleBackspace();
        case 0x03:
            return handleCtrlC();
        case 0x15:
            return handleCtrlU();
        default:
            return handlePrintableChar(c);
    }
}
```

3. Update `process()` to use extracted functions:
```cpp
bool SerialCmd::process() {
    int c = Console::getchar();
    if (c < 0) return false;

    if (handleEscapeSequence(c)) return true;
    if (handleSpecialChar(c)) return true;
    
    return false;
}
```

**Expected result:**
- Main `process()` function reduced to ~20 lines
- Each helper function has complexity < 5
- Improved testability with isolated functions

## References

- [Cyclomatic Complexity - Wikipedia](https://en.wikipedia.org/wiki/Cyclomatic_complexity)
- [McCabe Complexity Thresholds](https://www.confluence.atlassian.com/codeanalysis/cyclomatic-complexity-104391.html)
- [Refactoring: Flatten Nested Conditionals](https://refactoring.com/catalog/flattenCondition.html)
