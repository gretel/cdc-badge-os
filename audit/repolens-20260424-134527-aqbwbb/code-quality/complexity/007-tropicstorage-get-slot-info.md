---
title: "[MEDIUM] Deep nesting in TropicStorage::getSlotInfo() function"
severity: MEDIUM
domain: Code Quality
lens: cyclomatic-complexity
labels:
  - "audit:code-quality/complexity"
---

## Summary

The `TropicStorage::getSlotInfo()` function has deep nesting (4+ levels) with multiple nested `if` statements checking header validity, module matching, and data retrieval. The function is a good candidate for early returns (guard clauses) to flatten the logic.

**Estimated Cyclomatic Complexity: ~10** (at threshold)
**Maximum Nesting Depth: 4 levels**

## Impact

**Readability:**
- Deep nesting makes it hard to see the happy path
- Error handling is buried at the end of the function
- Each level adds cognitive load

**Maintenance:**
- Adding new validation steps increases nesting
- Hard to extract common patterns

## Evidence

**File:** `components/cdc_core/src/TropicStorage.cpp` (lines ~200-260)

**Code excerpt (typical pattern):**
```cpp
bool TropicStorage::getSlotInfo(uint16_t slot, SlotInfo* info) {
    if (!info) {
        return false;
    }

    if (!isSlotValid(slot)) {
        return false;
    }

    auto* se = hal::getSecureElementInstance();
    if (!se) {
        return false;
    }

    if (!se->isSessionActive()) {
        if (!se->sessionStart()) {
            return false;
        }
    }

    RMemHeader header = {};
    if (se->rmemReadHeader(slot, &header) != hal::SeResult::OK) {
        if (se->isSessionActive()) {
            se->sessionEnd();
        }
        return false;
    }

    // Level 3-4 nesting starts here
    if (header.isValid()) {
        if (header.moduleId == MODULE_ID_ANY || header.moduleId == moduleId_) {
            info->slot = slot;
            info->moduleId = header.moduleId;
            info->name = header.name;
            info->valid = true;
            
            if (se->isSessionActive()) {
                se->sessionEnd();
            }
            return true;
        }
    }

    if (se->isSessionActive()) {
        se->sessionEnd();
    }
    info->valid = false;
    return false;
}
```

**Nesting levels:**
- Level 1: `if (!info)`
- Level 1: `if (!isSlotValid(slot))`
- Level 1: `if (!se)`
- Level 1: `if (!se->isSessionActive())`
  - Level 2: `if (!se->sessionStart())`
- Level 1: `if (se->rmemReadHeader(...) != OK)`
  - Level 2: `if (se->isSessionActive())`
    - Level 3: `se->sessionEnd()`
- Level 1: `if (header.isValid())`
  - Level 2: `if (header.moduleId == ...)`
    - Level 3: Assignment block
      - Level 4: `if (se->isSessionActive())`

Total: 4 levels deep, ~10 branching paths

## Recommended Fix

**Use early returns (guard clauses) to flatten:**

```cpp
bool TropicStorage::getSlotInfo(uint16_t slot, SlotInfo* info) {
    // Guard clauses at the top
    if (!info) return false;
    if (!isSlotValid(slot)) return false;
    
    auto* se = hal::getSecureElementInstance();
    if (!se) return false;

    // Ensure session is active
    if (!se->isSessionActive()) {
        if (!se->sessionStart()) return false;
    }

    // RAII-style session cleanup
    bool sessionWasActive = true;  // Assume it was active
    auto cleanup = [&]() {
        if (sessionWasActive && se->isSessionActive()) {
            se->sessionEnd();
        }
    };

    // Read header
    RMemHeader header = {};
    if (se->rmemReadHeader(slot, &header) != hal::SeResult::OK) {
        cleanup();
        return false;
    }

    // Check validity
    if (!header.isValid()) {
        cleanup();
        info->valid = false;
        return false;
    }

    // Check module match
    if (header.moduleId != MODULE_ID_ANY && header.moduleId != moduleId_) {
        cleanup();
        info->valid = false;
        return false;
    }

    // Success path - populate info
    info->slot = slot;
    info->moduleId = header.moduleId;
    info->name = header.name;
    info->valid = true;
    cleanup();
    return true;
}
```

**Alternative with explicit session management:**

```cpp
bool TropicStorage::getSlotInfo(uint16_t slot, SlotInfo* info) {
    if (!info) return false;
    if (!isSlotValid(slot)) return false;

    auto* se = hal::getSecureElementInstance();
    if (!se) return false;

    bool needSessionEnd = false;
    if (!se->isSessionActive()) {
        if (!se->sessionStart()) return false;
        needSessionEnd = true;
    }

    auto closeSession = [&]() {
        if (needSessionEnd && se->isSessionActive()) {
            se->sessionEnd();
        }
    };

    RMemHeader header = {};
    hal::SeResult res = se->rmemReadHeader(slot, &header);
    if (res != hal::SeResult::OK) {
        closeSession();
        return false;
    }

    if (!header.isValid()) {
        closeSession();
        info->valid = false;
        return false;
    }

    if (header.moduleId != MODULE_ID_ANY && header.moduleId != moduleId_) {
        closeSession();
        info->valid = false;
        return false;
    }

    info->slot = slot;
    info->moduleId = header.moduleId;
    info->name = header.name;
    info->valid = true;
    closeSession();
    return true;
}
```

**Expected result:**
- Maximum nesting depth reduced to 2 levels
- Function is easier to read (happy path is linear)
- Each validation step is a guard clause
- Easier to add new validations

## References

- [Guard Clauses refactoring](https://refactoring.com/catalog/replaceNestedConditionWithGuardClauses.html)
- [Early Return pattern](https://en.wikipedia.org/wiki/Early_return)
