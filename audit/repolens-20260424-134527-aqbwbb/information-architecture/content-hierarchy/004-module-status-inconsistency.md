---
title: "[LOW] Module status list uses inconsistent status labels without clear visual grouping"
severity: LOW
domain: information-architecture
lens: content-hierarchy
labels:
  - "audit:information-architecture/content-hierarchy"
---

## Summary
The module status list in `ExpertMenuUi.cpp` (`components/cdc_os_ui/src/ExpertMenuUi.cpp:115-155`) displays module states using inconsistent status labels: `[ON]`, `[--]`, `[OFF]`, and `[FAIL]`. These labels:
- Use different visual patterns (brackets with varying content)
- Lack consistent color or icon support
- Do not group modules by state (all enabled modules together, all disabled together)
- Mix status with module name in a single string, making scanning harder

## Impact
Users managing multiple modules must parse each line individually to understand module states. The inconsistent labeling makes it harder to quickly identify:
- Which modules are currently active vs. inactive
- Which modules have errors requiring attention
- The overall system state at a glance

## Evidence
File: `components/cdc_os_ui/src/ExpertMenuUi.cpp:115-155`
```cpp
for (uint8_t i = 0; i < count && i < MODULES_VIEW_MAX; i++) {
    core::IModule* module = moduleReg.getModuleAt(i);
    if (module) {
        const char* status;
        if (moduleReg.hasModuleSlotError(i)) {
            status = "[FAIL]";
        } else {
            bool enabled = moduleReg.isModuleEnabled(i);
            status = enabled
                ? (module->getState() == core::ServiceState::STARTED ? "[ON]" : "[--]")
                : "[OFF]";
        }
        snprintf(s_moduleLabels[i], sizeof(s_moduleLabels[i]),
                 "%s %s", module->getName(), status);
        s_modulesItems[i] = {s_moduleLabels[i], 0, false, nullptr};
    }
}
```

Status labels are embedded in the module name string with no visual separation or grouping.

## Recommended Fix
1. Standardize status labels to a consistent format (e.g., all in brackets with same width)
2. Add icons to status labels (e.g., ✓ for ON, ○ for OFF, ✗ for FAIL)
3. Consider sorting modules by state (errors first, then enabled, then disabled)
4. Use fixed-width status column for better alignment

Approximate effort: 1 hour to refactor the status formatting and add simple icons.

## References
- Module status renderer: `components/cdc_os_ui/src/ExpertMenuUi.cpp:115-155`
- Icon support in ListView: `components/cdc_views/src/ListView.cpp:235-248`
