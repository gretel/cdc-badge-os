---
title: "[LOW] PIN change wizard shows step number but not step names clearly"
severity: LOW
domain: UI/UX
lens: cognitive-overload
labels:
  - multi-step-flows
  - form-fatigue
---

## Summary

The `PinChangeView` component (`PinChangeView.cpp:1-384`) implements a 3-step PIN change wizard that displays "1/3", "2/3", "3/3" but the step titles ("CURRENT_PIN", "NEW_PIN", "CONFIRM_PIN") are displayed inline without clear visual separation. Users may confuse which step they are on during the flow.

**Files:**
- `components/cdc_os_ui/src/views/PinChangeView.cpp:1-384` (PIN change wizard)
- `components/cdc_os_ui/include/cdc_os_ui/views/PinChangeView.h` (header)

## Impact

**User Experience:** While the step counter (1/3, 2/3, 3/4) is present, the step titles are rendered in the same visual style as other text, making it harder to quickly identify the current step. The flow also requires verifying the current PIN first, which adds cognitive load.

**Evidence:**
From `PinChangeView.cpp:330-335`:
```cpp
const char* stepTitle = getStepTitle();
char stepStr[48];
snprintf(stepStr, sizeof(stepStr), "%d/3: %s", static_cast<int>(step_) + 1, stepTitle);
gfx->getTextBounds(stepStr, 0, 0, &x1, &y1, &w, &h);
gfx->setCursor((width - w) / 2, STEP_Y);
gfx->print(stepStr);
```

The step string combines the number and title without visual distinction between them.

## Recommended Fix

1. **Visually distinguish the step number from the title** (e.g., bold the number, use different color)
2. **Add a progress bar** below the step title
3. **Consider simplifying the flow** for users who don't remember their current PIN (add a "forgot PIN" recovery option)

Quick fix: Add a separator between step number and title: "1 / 3: Current PIN" or use different text sizes for the number vs. the title.

## References

- Nielsen Norman Group: "Multi-Step Forms: Progress Indicators"
- Material Design: "Steppers" component patterns
