---
title: "[LOW] MessageBox and ToastView lack loading/processing state indicator"
severity: LOW
domain: cdc_views
lens: interactive-feedback
labels:
  - "loading-state"
  - "processing-feedback"
  - "task-state"
---

## Summary
The `MessageBox` and `ToastView` components provide static message display but lack a dedicated visual state for showing ongoing operations. The `Icon::TASK` exists in ToastView but is a static hourglass icon without any animation or progress indication.

## Impact
- **Uncertainty**: Users don't know if an operation is still in progress or stuck
- **Wait time estimation**: No way to indicate how much longer an operation might take
- **Differentiation**: Static "working" message doesn't clearly communicate "this is taking time"

## Evidence
File: `components/cdc_views/src/ToastView.cpp`, lines 134-143

```cpp
case Icon::TASK:
    // Simple hourglass icon
    gfx->drawLine(iconX - 5, iconY - 6, iconX + 5, iconY - 6, EPD_BLACK);
    gfx->drawLine(iconX - 5, iconY + 6, iconX + 5, iconY + 6, EPD_BLACK);
    gfx->drawLine(iconX - 5, iconY - 6, iconX + 5, iconY + 6, EPD_BLACK);
    gfx->drawLine(iconX + 5, iconY - 6, iconX - 5, iconY + 6, EPD_BLACK);
    gfx->fillTriangle(iconX - 3, iconY - 4, iconX + 3, iconY - 4, iconX, iconY - 1, EPD_BLACK);
    gfx->fillTriangle(iconX - 3, iconY + 4, iconX + 3, iconY + 4, iconX, iconY + 1, EPD_BLACK);
    break;
```

File: `components/cdc_views/src/MessageBox.cpp`, lines 134-178

The MessageBox has no task/loading icon option at all - only NONE, SUCCESS, ERROR, INFO, and WARNING.

Usage example from `components/cdc_os_ui/src/ExpertMenuUi.cpp`:
```cpp
static void runTropicCacheRebuild() {
    showToastTask(tr(StringId::TASK_WORKING), 0);  // Static "Working" message
    bool ok = core::TropicStorage::instance().rebuild();
    ViewStack::instance().hideModal();
    // ...
}
```

## Recommended Fix
1. **Add a blinking/dynamic indicator for task state**:
```cpp
// In ToastView::render()
if (icon_ == Icon::TASK) {
    // Blink effect based on time
    uint32_t blinkPhase = (startMs_ / 500) % 2;
    if (blinkPhase == 0) {
        // Draw full hourglass
        // ... existing code ...
    } else {
        // Draw partial hourglass (less filled)
        // ... modified code with less fill ...
    }
}
```

2. **Add progress bar option for known-duration operations**:
```cpp
// New method in ToastView
void ToastView::initWithProgress(const char* message, uint8_t progressPercent, uint16_t durationMs);

void ToastView::render(bool partial) {
    // ... existing code ...
    
    // Draw progress bar below message if in progress mode
    if (progressPercent_ > 0) {
        int barWidth = BOX_WIDTH - 40;
        gfx->drawRect(boxX + 10, boxY + BOX_HEIGHT - 15, barWidth, 8, EPD_BLACK);
        gfx->fillRect(boxX + 12, boxY + BOX_HEIGHT - 13, barWidth * progressPercent_ / 100, 4, EPD_BLACK);
    }
}
```

3. **Add TASK icon to MessageBox**:
```cpp
// In MessageBox.h - add TASK to MessageIcon enum
enum class MessageIcon : uint8_t {
    NONE = 0,
    SUCCESS,
    ERROR,
    INFO,
    WARNING,
    TASK  // Add this
};

// In MessageBox::render() - add TASK case similar to ToastView
```

## References
- [WAI-ARIA Progressbar Pattern](https://www.w3.org/WAI/ARIA/apg/patterns/progressbar/)
- Loading state design patterns for embedded interfaces
