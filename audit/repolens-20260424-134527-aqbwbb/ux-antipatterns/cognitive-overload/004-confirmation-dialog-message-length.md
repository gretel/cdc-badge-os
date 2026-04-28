---
title: "[LOW] Confirmation dialogs use fixed-size message buffer with potential truncation"
severity: LOW
domain: UI/UX
lens: cognitive-overload
labels:
  - modal-dialogs
  - information-density
---

## Summary

The `ConfirmView` component uses a fixed-size message buffer (`MAX_MSG_LEN = 96` characters) with basic word wrap. Long messages can be difficult to read due to the small box dimensions (220x60 pixels) and simple word-wrap implementation that may not handle all edge cases well.

**Files:**
- `components/cdc_views/include/cdc_views/ConfirmView.h:18-50` (ConfirmView class)
- `components/cdc_views/src/ConfirmView.cpp:113-180` (message rendering logic)

## Impact

**User Experience:** Long confirmation messages may be truncated or displayed in a cramped way, making it hard for users to read important context before making a decision. The box height (60px) limits content to approximately 4-5 lines of text.

**Evidence:**
From `ConfirmView.h:21-22`:
```cpp
static constexpr uint16_t MAX_MSG_LEN = 96;
static constexpr int BOX_WIDTH = 220;
static constexpr int BOX_HEIGHT = 60;
```

From `ConfirmView.cpp:113-180`, the word-wrap logic is basic and may produce awkward line breaks:
```cpp
// Simple word wrap for longer messages
const char* ptr = message_;
int lineY = textY;
int maxLineWidth = BOX_WIDTH - (textX - boxX) - 10;
char lineBuf[48];
int lineLen = 0;
```

Example usage in `ExpertMenuUi.cpp:75-81` shows messages that can approach the limit:
```cpp
snprintf(confirmMsg, sizeof(confirmMsg), "%s\n\nNochmal laden?",
         error ? error : "Modul-Fehler");
```

## Recommended Fix

1. **Increase box dimensions** to accommodate longer messages (e.g., BOX_HEIGHT = 80-100)
2. **Add scroll support** for messages that exceed the box height
3. **Improve word-wrap algorithm** to handle hyphenation and better line breaking
4. **Consider dynamic sizing** based on message length

Quick fix: Increase `BOX_HEIGHT` from 60 to 80 pixels and adjust text positioning accordingly.

## References

- Nielsen Norman Group: "Dialog Box Design: Message Length and Readability"
- Material Design: "Dialogs" component patterns
