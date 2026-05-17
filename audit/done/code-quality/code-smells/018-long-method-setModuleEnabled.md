---
title: "[LOW] Long Method: ModuleRegistry::setModuleEnabled() is 60+ lines"
severity: LOW
domain: core
lens: code-smells
labels:
  - "long-method"
  - "cdc_core"
---

## Summary
`components/cdc_core/src/ModuleRegistry.cpp:552-611` contains `setModuleEnabled()` which is 60 lines long and performs string parsing/manipulation to update the disabled modules list.

## Impact
**Readability**: Hard to understand the logic at a glance.

**Testing**: Difficult to test edge cases without testing the whole method.

**Modification risk**: Changes can accidentally break the string parsing logic.

## Evidence
`components/cdc_core/src/ModuleRegistry.cpp:552-611`:
```cpp
void ModuleRegistry::setModuleEnabled(uint8_t index, bool enabled) {
    if (index >= count_) return;

    const char* name = modules_[index]->getName();
    bool currentlyEnabled = isModuleEnabledByName(name);

    if (enabled == currentlyEnabled) return;  // No change needed

    if (enabled) {
        // Remove name from disabled list
        char newList[MAX_DISABLED_LIST_SIZE] = {0};
        size_t newOffset = 0;
        size_t nameLen = strlen(name);

        const char* ptr = disabledModules_;
        while (*ptr) {
            while (*ptr == ',') ptr++;
            if (*ptr == '\0') break;

            const char* end = ptr;
            while (*end && *end != ',') end++;
            size_t tokenLen = end - ptr;

            // Copy token if it's not the one to remove
            if (!(tokenLen == nameLen && strncmp(ptr, name, nameLen) == 0)) {
                if (newOffset > 0 && newOffset < sizeof(newList) - 1) {
                    newList[newOffset++] = ',';
                }
                if (newOffset + tokenLen < sizeof(newList)) {
                    memcpy(newList + newOffset, ptr, tokenLen);
                    newOffset += tokenLen;
                }
            }

            ptr = end;
        }
        newList[newOffset] = '\0';
        memcpy(disabledModules_, newList, sizeof(disabledModules_));
    } else {
        // Add name to disabled list
        size_t currentLen = strlen(disabledModules_);
        size_t nameLen = strlen(name);

        if (currentLen + nameLen + 2 < MAX_DISABLED_LIST_SIZE) {
            if (currentLen > 0) {
                disabledModules_[currentLen++] = ',';
            }
            memcpy(disabledModules_ + currentLen, name, nameLen + 1);
        } else {
            LOG_W(TAG, "Disabled list full, cannot add '%s'", name);
            return;
        }
    }

    saveDisabledList();
}
```

The method has two distinct paths (enable and disable) with string parsing logic.

## Recommended Fix
Extract helper methods:
```cpp
void ModuleRegistry::setModuleEnabled(uint8_t index, bool enabled) {
    if (index >= count_) return;

    const char* name = modules_[index]->getName();
    bool currentlyEnabled = isModuleEnabledByName(name);

    if (enabled == currentlyEnabled) return;

    if (enabled) {
        char newList[MAX_DISABLED_LIST_SIZE] = {0};
        removeNameFromList(disabledModules_, name, newList);
        memcpy(disabledModules_, newList, sizeof(disabledModules_));
    } else {
        addNameToList(disabledModules_, name);
    }

    saveDisabledList();
}

// Helper methods
void ModuleRegistry::removeNameFromList(const char* list, const char* name, char* result);
void ModuleRegistry::addNameToList(char* list, const char* name);
```

**Estimated effort**: ~1 hour to extract methods.

## References
- Refactoring.com: "Long Method" - https://refactoring.com/catalog/extractMethod
- Martin Fowler, "Refactoring: Improving the Design of Existing Code", Chapter 6
