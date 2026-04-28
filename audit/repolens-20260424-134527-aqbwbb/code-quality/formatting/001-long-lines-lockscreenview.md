---
title: "[MEDIUM] Long lines and compact single-line function bodies in LockScreenView.cpp"
severity: MEDIUM
domain: code-quality/formatting
lens: formatting-consistency
labels:
  - "audit:code-quality/formatting"
---

## Summary
The file `components/cdc_os_ui/src/views/LockScreenView.cpp` contains 7 lines exceeding 120 characters (137 chars each). These are single-line function definitions for module context callback wrappers that combine the function signature, conditional check, and function call all on one line.

**Affected lines:**
- Line 267: `static void moduleContextCallback0() { if (s_moduleContextItems[0].callback) { s_moduleContextItems[0].callback(); } hideContextMenu(); }`
- Line 272: `static void moduleContextCallback1() { if (s_moduleContextItems[1].callback) { s_moduleContextItems[1].callback(); } hideContextMenu(); }`
- Line 277: `static void moduleContextCallback2() { if (s_moduleContextItems[2].callback) { s_moduleContextItems[2].callback(); } hideContextMenu(); }`
- Line 282: `static void moduleContextCallback3() { if (s_moduleContextItems[3].callback) { s_moduleContextItems[3].callback(); } hideContextMenu(); }`
- Line 287: `static void moduleContextCallback4() { if (s_moduleContextItems[4].callback) { s_moduleContextItems[4].callback(); } hideContextMenu(); }`
- Line 292: `static void moduleContextCallback5() { if (s_moduleContextItems[5].callback) { s_moduleContextItems[5].callback(); } hideContextMenu(); }`
- Line 297: `static void moduleContextCallback6() { if (s_moduleContextItems[6].callback) { s_moduleContextItems[6].callback(); } hideContextMenu(); }`

## Impact
- **Readability**: Lines exceeding 120 characters are harder to read on standard terminal widths and IDE windows
- **Diff noise**: Long lines create wider diffs when modified
- **Consistency**: Other callback functions in the codebase use multi-line formatting with proper braces

## Evidence
File: `components/cdc_os_ui/src/views/LockScreenView.cpp:267-297`

Current format (all on one line):
```cpp
static void moduleContextCallback0() { if (s_moduleContextItems[0].callback) { s_moduleContextItems[0].callback(); } hideContextMenu(); }
```

## Recommended Fix
Refactor the 7 callback functions to use multi-line formatting with proper brace placement:

```cpp
static void moduleContextCallback0() {
    if (s_moduleContextItems[0].callback) {
        s_moduleContextItems[0].callback();
    }
    hideContextMenu();
}
```

Apply the same pattern to functions at lines 272, 277, 282, 287, 292, and 297.

## References
- C++ Core Guidelines: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
- Common line length conventions: 100-120 characters for C/C++ projects
