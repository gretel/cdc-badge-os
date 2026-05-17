---
title: "[LOW] TODO comments without tracking information"
severity: LOW
domain: linting
lens: code-quality/linting
labels:
  - "audit:code-quality/linting"
---

## Summary
Many TODO comments in the codebase lack context such as issue numbers, author names, or target dates. This makes it difficult to track and prioritize technical debt.

**Location:** Multiple component files

## Impact
- **Technical debt accumulation**: TODOs without tracking may be forgotten
- **Unclear ownership**: No one knows who is responsible for addressing the TODO
- **Hard to prioritize**: No context for why the TODO exists or how important it is

## Evidence

### File: `components/CalEPD/include/plasticlogic021.h`
```cpp
// TODO: Should be 2 bits per pixel: 
```
Missing: what should be done, why, who, when

### File: `components/CalEPD/epdParallel.cpp`
```cpp
// TODO: Implement printf
// TODO: Research a smarter way to do this
```
Missing: approach suggestions, issue references

### File: `components/CalEPD/epd7color.cpp`
```cpp
// TODO: Implement printf
// TODO: Research a smarter way to do this
```
Duplicate TODO - same as in `epdParallel.cpp`

### File: `components/CalEPD/models/fix/gdeh0213b73.cpp`
```cpp
// TODO
```
Completely uninformative

### File: `components/CalEPD/models/plasticlogic/plasticlogic.cpp`
```cpp
// display.print / println handling .TODO: Implement printf
```
Missing context, poor formatting

## Recommended Fix

### 1. Add structured TODO format:
```cpp
// TODO(@author): Brief description
// Issue: #123
// Priority: HIGH/MEDIUM/LOW
// Target: v1.0
```

### 2. Create a TODO tracking issue:
Create a GitHub issue to track all TODOs:
```markdown
## TODO Tracker

| File | TODO | Priority | Issue |
|------|------|----------|-------|
| `components/CalEPD/include/plasticlogic021.h` | 2 bits per pixel | MEDIUM | #XXX |
| `components/CalEPD/epdParallel.cpp` | Implement printf | LOW | #XXX |
```

### 3. Add TODO linting:
Add a CI check to list TODOs:
```yaml
- name: Check TODOs
  run: |
    echo "## TODOs found" >> $GITHUB_STEP_SUMMARY
    grep -r "TODO" components/ --include="*.cpp" --include="*.h" | \
      sed 's/:/ - /' | \
      sed 's/</\&lt;/g' | \
      sed 's/>/\&gt;/g' >> $GITHUB_STEP_SUMMARY
```

### 4. Consider using a TODO management tool:
- [todo.md](https://github.com/marototal/todo.md)
- [Taskwarrior](https://taskwarrior.org/)

## References
- [C++ Core Guidelines: C.17 - Use TODO comments](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Co-todo)
- [How to write good TODOs](https://www.atlassian.com/agile/project-management/todos)
