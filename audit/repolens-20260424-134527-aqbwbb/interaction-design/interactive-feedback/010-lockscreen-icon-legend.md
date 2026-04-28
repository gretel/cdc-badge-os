---
title: "[LOW] LockScreenView status icons lack clear meaning"
severity: LOW
domain: cdc_os_ui
lens: interactive-feedback
labels:
  - "icon-clarity"
  - "status-indicators"
  - "visual-language"
---

## Summary
The `LockScreenView` component (`components/cdc_os_ui/src/views/LockScreenView.cpp:438-558`) renders multiple status icons (USB, WiFi, BLE, battery, charging, etc.) but provides no legend or tooltip to explain what each icon means. Users must memorize the icon set or guess at their meaning.

## Impact
- **Discoverability**: New users won't know what each icon represents
- **Context switching**: Users may not understand why an icon appears/disappears
- **Accessibility**: Icon-only status indicators can be ambiguous

## Evidence
File: `components/cdc_os_ui/src/views/LockScreenView.cpp`, lines 438-558

```cpp
void LockScreenView::renderStatusIcons(void* gfxPtr, int x, int y) {
    auto* gfx = static_cast<Gdey029T94*>(gfxPtr);
    int iconX = x;
    const int iconSpacing = 14;

    // Lock icon
    if ((statusIcons_ & StatusIcon::LOCK) != StatusIcon::NONE) {
        // Draw padlock
        gfx->drawRect(iconX, y + 4, 8, 6, EPD_BLACK);
        gfx->drawCircle(iconX + 4, y + 3, 3, EPD_BLACK);
        iconX -= iconSpacing;
    }

    // WiFi icon - classic arc style
    if ((statusIcons_ & StatusIcon::WIFI) != StatusIcon::NONE) {
        int cx = iconX + 4;
        int cy = y + 10;
        // Base dot
        gfx->fillCircle(cx, cy, 1, EPD_BLACK);
        // Arc 1 (small)
        gfx->drawLine(cx - 2, cy - 3, cx, cy - 4, EPD_BLACK);
        // ... more arc lines ...
        iconX -= iconSpacing;
    }

    // BLE icon
    if ((statusIcons_ & StatusIcon::BLE) != StatusIcon::NONE) {
        // Bluetooth rune
        gfx->drawLine(iconX + 4, y, iconX + 4, y + 10, EPD_BLACK);
        // ... more rune lines ...
        iconX -= iconSpacing;
    }

    // USB icon
    if ((statusIcons_ & StatusIcon::USB) != StatusIcon::NONE) {
        int ux = iconX, uy = y;
        // Main stem
        gfx->drawLine(ux + 5, uy + 4, ux + 5, uy + 12, EPD_BLACK);
        // ... more USB lines ...
        iconX -= iconSpacing;
    }

    // ... more icons ...
}
```

The icons are drawn but there's no:
- Legend or key to explain icon meanings
- Tooltip on hover (not applicable for E-Paper but could be context-menu)
- Consistent visual language across all icons

## Recommended Fix
1. **Add a context menu option to show icon legend**:
```cpp
// In LockScreenView::onKey() - add a new key for legend
if (key == '7') {  // New key for legend
    showIconLegend();
    return InputResult::CONSUMED;
}

// New method
void LockScreenView::showIconLegend() {
    static ContextMenuItem items[] = {
        {"USB = Connected", []() { hideContextMenu(); }},
        {"BLE = Bluetooth on", []() { hideContextMenu(); }},
        {"WiFi = Connected", []() { hideContextMenu(); }},
        {"Battery = Level shown", []() { hideContextMenu(); }},
    };
    showContextMenu("Icons", items, 4);
}
```

2. **Improve icon clarity with more distinctive designs**:
```cpp
// Make WiFi icon more recognizable
if ((statusIcons_ & StatusIcon::WIFI) != StatusIcon::NONE) {
    int cx = iconX + 4;
    int cy = y + 10;
    // Draw complete WiFi symbol with clear arcs
    gfx->drawCircle(cx, cy, 3, EPD_BLACK);  // Center dot
    gfx->drawArc(cx, cy, 5, 6, 0, 180, EPD_BLACK);  // Inner arc
    gfx->drawArc(cx, cy, 8, 9, 0, 180, EPD_BLACK);  // Outer arc
    iconX -= iconSpacing;
}
```

3. **Add hover-like feedback via long-press**:
```cpp
// In onTick() - detect long-press on specific area
// When an icon is long-pressed, show a toast with its meaning
if (key == '3' && iconIsUnderKey()) {
    showToastInfo("USB connected", 2000);
}
```

## References
- [Icon design best practices](https://www.nngroup.com/articles/icon-usability/)
- [Status indicator design patterns](https://www.w3.org/WAI/tutorials/status/)
