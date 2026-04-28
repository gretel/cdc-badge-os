---
title: "[LOW] Feature Envy: ListView reaches deeply into ListItem internals"
severity: LOW
domain: cdc_views
lens: code-smells
labels:
  - "refactor:encapsulate-data"
  - "maintainability"
---

## Summary
The `ListView` class accesses internal fields of `ListItem` directly (label, icon, userData, iconDisabled), showing feature envy - the list view knows too much about the internal structure of menu items.

**Location:** `components/cdc_views/src/ListView.cpp:210-250`

## Impact
- **Tight coupling**: Changes to `ListItem` structure require changes to `ListView`
- **Encapsulation violation**: `ListItem` exposes internal details
- **Limited extensibility**: Adding new item properties requires modifying all renderers

## Evidence
```cpp
// ListView.cpp:215-250 - ListView reaches into ListItem internals
const ListItem& item = items_[itemIndex];
bool isSelected = (itemIndex == selection_);

if (isSelected) {
    gfx->fillRect(2, y + 1, rowWidth - 4, itemHeight_ - 2, EPD_BLACK);
    gfx->setTextColor(EPD_WHITE);
} else {
    gfx->setTextColor(EPD_BLACK);
}

bool handled = false;
if (itemRenderer_) {
    handled = itemRenderer_(gfx, item, itemIndex,
                            0, y, rowWidth, itemHeight_,
                            isSelected, itemRendererCtx_);
}

if (!handled) {
    int textX = ITEM_PADDING_X;
    if (item.icon) {  // Accessing ListItem.icon directly
        char iconStr[2] = {static_cast<char>(item.icon), '\0'};
        gfx->setCursor(textX, y + 4);
        gfx->print(iconStr);
        if (item.iconDisabled) {  // Accessing ListItem.iconDisabled directly
            uint16_t color = isSelected ? EPD_WHITE : EPD_BLACK;
            gfx->drawLine(textX, y + 10, textX + 6, y + 10, color);
        }
        textX += 10;
    }

    gfx->setCursor(textX, y + 4);
    if (item.label) {  // Accessing ListItem.label directly
        gfx->print(item.label);
    }
}
```

```cpp
// IModule.h:26-34 - ListItem structure exposing all fields
struct ModuleMenuItem {
    const char* label;              // All public - no encapsulation
    uint8_t priority;
    ui::IView* (*getView)();
    bool (*isVisible)();
    const char* moduleName;
    MenuLocation location;
    void (*onSelect)();
};
```

## Recommended Fix
Add accessor methods to `ListItem` and delegate rendering logic:

```cpp
// Option 1: Add rendering methods to ListItem
struct ListItem {
    const char* label;
    char icon = 0;
    bool iconDisabled = false;
    void* userData = nullptr;

    // Encapsulated access
    const char* getLabel() const { return label; }
    char getIcon() const { return icon; }
    bool isIconDisabled() const { return iconDisabled; }
    
    // Delegate rendering
    void render(Gdey029T94* gfx, int x, int y, int width, int height,
                bool isSelected) const {
        if (icon) {
            // Render icon
        }
        if (label) {
            // Render label
        }
    }
};

// Option 2: Use strategy pattern for rendering
struct ListItem {
    using RendererFunc = std::function<void(Gdey029T94*, int, int, int, int, bool)>;
    
    const char* label;
    RendererFunc customRenderer = nullptr;
    
    void render(Gdey029T94* gfx, int x, int y, int w, int h, bool sel) const {
        if (customRenderer) {
            customRenderer(gfx, x, y, w, h, sel);
        } else {
            // Default rendering
        }
    }
};
```

## References
- Martin Fowler, "Refactoring: Improving the Design of Existing Code" - Feature Envy smell
- Encapsulation principle: Objects should hide their internal state and expose behavior
