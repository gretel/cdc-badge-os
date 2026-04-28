---
title: "[HIGH] Expert menu lacks context about risks and breaking changes"
severity: HIGH
domain: information-architecture
lens: help-context
labels:
  - "audit:information-architecture/help-context"
---

## Summary
The expert menu (`components/cdc_os_ui/src/ExpertMenuUi.cpp`) provides access to advanced operations (TR01 Cache Rebuild, TR01 Cache Cleanup, etc.) with minimal explanation of what these operations do and what risks they carry.

**Evidence** (`components/cdc_os_ui/src/ExpertMenuUi.cpp`):
```cpp
REG(EXPERT,             "Expert",               "Experte");
REG(EXPERT_WARNING,     "Possible breaking",    "Moeglich riskant");
```

The expert warning string "Possible breaking" is vague and doesn't explain:
- What specific operations might break
- What data might be lost
- What the user should backup first
- How to recover if something goes wrong

**Example operation** (TR01 Cache Rebuild):
- No explanation of what the TR01 cache stores
- No warning about time required (might take minutes)
- No indication of what happens if interrupted

## Impact
Users may:
1. Run expert operations without understanding consequences
2. Interrupt long-running operations causing corruption
3. Not know how to recover from failures
4. Lose data without warning

## Evidence
- File: `components/cdc_os_ui/src/ExpertMenuUi.cpp`
- Lines: 210 (expert warning toast)
- Warning text is vague: "Possible breaking"
- No detailed explanations before operations
- No confirmation dialogs with risk details

## Recommended Fix
Add comprehensive risk context:

1. **Detailed confirmation dialogs**:
   ```
   TR01 Cache Rebuild?
   - Rebuilds secure element session cache
   - Takes 2-5 minutes
   - Don't interrupt!
   - Safe to run, no data loss
   [Y] Proceed  [N] Cancel  [3] Learn more
   ```

2. **Progress indication** for long operations:
   ```
   Rebuilding cache...
   Step 2/4 (45%)
   Please wait...
   ```

3. **Recovery instructions** on failure:
   ```
   Cache rebuild failed.
   Retry or reboot badge?
   ```

4. **Info key** (key '3') on each operation:
   Full explanation of what happens

## References
- ESP32 secure element best practices: https://docs.espressif.com/projects/esp-tropic/
