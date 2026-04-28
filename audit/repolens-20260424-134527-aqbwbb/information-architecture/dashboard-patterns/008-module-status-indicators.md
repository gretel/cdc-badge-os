---
title: "[LOW] Module status indicators use inconsistent visual encoding"
severity: LOW
domain: UI/UX - Dashboard Patterns
labels:
  - "audit:information-architecture/dashboard-patterns"
---

## Summary

The modules management view displays status indicators for each module, but the visual encoding is inconsistent. Found in:

- `components/cdc_os_ui/src/ExpertMenuUi.cpp:240-270` - `rebuildModulesView()`

**Current status indicators:**
- `[ON]` - Module enabled and started
- `[--]` - Module enabled but not started
- `[OFF]` - Module disabled
- `[FAIL]` - Module has slot error

**Issues:**
1. No consistent color coding (all text is same color on E-Paper)
2. No icon/badge support for status (e.g., checkmark, warning)
3. Status text varies in length, causing alignment issues
4. No visual grouping of "active" vs "inactive" modules

## Impact

**User Experience:**
- Harder to quickly scan and identify problem modules
- Status meaning must be read, not just perceived
- No visual hierarchy between module states

**Design Consistency:**
- Status indicators should follow a consistent pattern across all dashboard views
- Lock screen uses icons for status (USB, BLE, WiFi) but module view uses text

## Evidence

**Current Implementation:**
```cpp
// components/cdc_os_ui/src/ExpertMenuUi.cpp:240-270
static void rebuildModulesView() {
    auto& moduleReg = core::ModuleRegistry::instance();
    uint8_t count = moduleReg.getModuleCount();

    if (count == 0) {
        s_modulesItems[0] = {"(none)", 0, false, nullptr};
        count = 1;
    } else {
        for (uint8_t E=0; i < count && i < MODULES_VIEW_MAX; i++) {
            core::IModule* module = moduleReg.getModuleAt(i);
            if (module) {
                const char* status;
                if (moduleReg.hasModuleSlotError(i)) {
                    status = "[FAIL]";  // 6 chars
                } else {
                    bool enabled = moduleReg.isModuleEnabled(i);
                    status = enabled
                        ? (module->getState() == core::ServiceState::STARTED ? "[ON]" : "[--]")  // 4 chars
                        : "[OFF]";  // 5 chars
                }
                snprintf(s_moduleLabels[i], sizeof(s_moduleLabels[i]),
                         "%s %s", module->getName(), status);  // Variable width!
                s_modulesItems[i] = {s_moduleLabels[i], 0, false, nullptr};
            }
        }
        if (count > MODULES_VIEW_MAX) count = MODULES_VIEW_MAX;
    }

    if (s_modulesView) {
        s_modulesView->init(tr(StringId::MODULES), s_modulesItems, count);
    }
}
```

**What's Missing:**
- No fixed-width status field for alignment
- No icon support for visual status
- No consistent status encoding (colors, icons, badges)

## Recommended Fix

Standardize module status indicators:

**Option 1: Fixed-Width Status Field (Simple)**

Use fixed-width status strings:
```cpp
// Align status to fixed width
const char* getStatusText(core::IModule* module, bool hasError) {
    if (hasError) return " [FAIL]";   // 6 chars
    bool enabled = moduleReg.isModuleEnabled(i);
    if (!enabled) return " [ OFF]";   // 6 chars
    return (module->getState() == core::ServiceState::STARTED) ? " [ ON]" : " [--]"; // 6 chars
}
```

**Option 2: Icon-Based Status (Better Visual)**

Add status icons to the ListItem structure:
```cpp
struct ListItem {
    const char* label;
    uint8_t icon;        // New field: status icon
    bool iconDisabled;   // New field: gray out icon
    void* userData;
};

// Then render with icon
gfx->drawIcon(statusIcon, x, y);
gfx->print(moduleName);
```

**Option 3: Color-Coded Badges (Best for Color Displays)**

Use background colors for status (if display supports it):
```cpp
// Draw status badge with background
void drawStatusBadge(Graphics* gfx, const char* status, bool isError, int x, int y) {
    gfx->fillRect(x, y, 40, 12, isError ? EPD_RED : EPD_BLACK);
    gfx->setCursor(x + 2, y + 10);
    gfx->print(status);
}
```

**Implementation Steps:**

1. **Option 1 (Quick fix, ~30 min):**
   - Standardize status text to fixed width
   - Update `rebuildModulesView()` to use consistent formatting

2. **Option 2 (Better, ~1 hour):**
   - Add `icon` field to `ListItem` struct
   - Update `ListView::render()` to draw icons
   - Define status icons (ON, OFF, FAIL)

3. **Option 3 (Best, ~2 hours):**
   - Add badge rendering helper
   - Update all status displays to use badges

## References

- Dashboard pattern: Status indicators should be scannable at a glance
- UX principle: Visual encoding (icons, colors) is faster to process than text
- Similar pattern: Lock screen uses icons for status (USB, BLE, WiFi)
