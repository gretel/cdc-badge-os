---
title: "[MEDIUM] Expert menu lacks explanation of technical terms and actions"
severity: MEDIUM
domain: information-architecture
lens: help-context
labels:
  - "expert-menu"
  - "contextual-help"
  - "i18n"
---

## Summary
The Expert menu (components/cdc_os_ui/src/ExpertMenuUi.cpp) presents technical actions like "TR01 Cache Rebuild" and "TR01 Cache Cleanup" without any inline explanation or contextual help to explain what these actions do, their purpose, or potential side effects.

**Evidence:**
- File: `components/cdc_os_ui/src/ExpertMenuUi.cpp:175-199`
- Lines 175-185: `runTropicCacheRebuild()` - no explanation shown before execution
- Lines 187-199: `runTropicCacheCleanup()` - no explanation shown before execution
- File: `components/cdc_ui/include/cdc_ui/I18n.h:135-136`
- String IDs `TR01_CACHE_REBUILD` and `TR01_CACHE_CLEANUP` contain only labels, no descriptions

The expert menu is shown at line 218-227 with these items:
```cpp
s_expertItems[0] = {tr(StringId::HARDWARE_INFO), 0, false, nullptr};
s_expertItems[1] = {tr(StringId::TR01_CACHE_REBUILD), 0, false, nullptr};
s_expertItems[2] = {tr(StringId::TR01_CACHE_CLEANUP), 0, false, nullptr};
```

## Impact
Users accessing the expert menu (which already shows a warning toast at line 218) may:
1. Execute cache rebuild/cleanup without understanding what data might be affected
2. Not know when each action is appropriate (e.g., after slot map changes vs. routine maintenance)
3. Potentially disrupt module state if run at the wrong time

This is particularly important since the expert menu is for "advanced tools" (see `components/cdc_core/include/cdc_core/IModule.h:21`), making clear explanations even more critical.

## Evidence
- Expert menu rebuild at `components/cdc_os_ui/src/ExpertMenuUi.cpp:220-227`
- Cache rebuild function at `components/cdc_os_ui/src/ExpertMenuUi.cpp:175-185`
- Cache cleanup function at `components/cdc_os_ui/src/ExpertMenuUi.cpp:187-199`
- I18n string definitions at `components/cdc_ui/src/I18n.cpp:225-226`

## Recommended Fix
Add a context menu or info view before executing cache operations:

1. **Option A (Quick):** Add a confirmation dialog with explanation before running each action:
```cpp
static void runTropicCacheRebuild() {
    static char confirmMsg[128];
    snprintf(confirmMsg, sizeof(confirmMsg),
        "Rebuilds cached TROPIC metadata.\n\n"
        "Use after slot changes or if modules show stale data.\n\n"
        "Continue?");
    showConfirm(confirmMsg, /*...*/);
}
```

2. **Option B (Better):** Create a helper function to show info first:
```cpp
static void showCacheRebuildInfo() {
    const char* info = "Cache Rebuild\n\n"
        "Rebuilds TROPIC01 metadata cache from secure element.\n\n"
        "Use when:\n"
        "- Modules show incorrect slot data\n"
        "- After firmware updates\n"
        "- When debugging slot allocation\n\n"
        "Takes 2-5 seconds.";
    showInfo("Cache Rebuild", info);
}
```

## References
- Expert menu definition: `components/cdc_os_ui/src/ExpertMenuUi.cpp:208-227`
- I18n strings: `components/cdc_ui/include/cdc_ui/I18n.h:135-136`
- Similar pattern exists for module retry confirmation at `components/cdc_os_ui/src/ExpertMenuUi.cpp:65-80`
