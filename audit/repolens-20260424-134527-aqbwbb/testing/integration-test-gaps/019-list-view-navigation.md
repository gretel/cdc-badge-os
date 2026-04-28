---
title: "[MEDIUM] ListView rendering and keyboard navigation lacks integration tests"
severity: MEDIUM
domain: ui
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:cdc_views"
  - "area:list-view"
---

## Summary
The `ListView` component (`components/cdc_views/include/cdc_views/ListView.h`) renders scrollable lists with keyboard navigation, but **no integration tests** verify that key handling, scrolling, and selection callbacks work correctly for various list sizes.

## Impact
- **Scrolling bugs**: Lists may not scroll correctly with many items
- **Selection**: Wrong item may be selected
- **Callbacks**: Select/menu callbacks may not fire correctly
- **Large lists**: Lists with 100+ items may have performance issues

## Evidence

**ListView API** (`components/cdc_views/include/cdc_views/ListView.h:32-175`):
```cpp
class ListView : public ViewBase {
    void init(const char* title, const ListItem* items, uint16_t count);
    void setOnSelect(SelectCallback callback);
    void setOnMenu(MenuCallback callback);
    
    // Keys: 2=Up, 8=Down, Y=Select, N=Back, 3=Menu
};
```

**List item structure** (`components/cdc_views/include/cdc_views/ListView.h:13-18`):
```cpp
struct ListItem {
    const char* label;
    uint8_t icon;
    bool iconDisabled;
    void* userData;
};
```

**Rendering** (`components/cdc_views/src/ListView.cpp:50-200`):
```cpp
void ListView::render() {
    // Calculate visible items based on display height
    uint8_t visibleItems = calculateVisibleItems();
    
    // Draw visible items with selected highlight
    for (uint8_t i = 0; i < visibleItems; i++) {
        uint16_t index = scrollOffset_ + i;
        drawItem(index, index == selectedIndex_);
    }
}
```

**Key handling** (`components/cdc_views/src/ListView.cpp:200-300`):
```cpp
void ListView::onKey(char key) {
    switch (key) {
        case '2':  // Up
            if (selectedIndex_ > 0) {
                selectedIndex_--;
                updateScrollOffset();
            }
            break;
        case '8':  // Down
            if (selectedIndex_ < count_ - 1) {
                selectedIndex_++;
                updateScrollOffset();
            }
            break;
        case 'Y':  // Select
            if (onSelect_) onSelect_(selectedIndex_, items_[selectedIndex_].userData);
            break;
        case '3':  // Menu
            if (onMenu_) onMenu_(selectedIndex_, items_[selectedIndex_].userData);
            break;
    }
}
```

**Usage in modules**:
- `TotpModule` - 100+ TOTP entries
- `PasswordModule` - 100+ password entries
- `Fido2Ui` - FIDO2 credentials list
- `AppUi` - Main menu, tools menu, settings menu

**Current test coverage**: None

## Recommended Fix

Create integration test `test_list_view/` that verifies:

1. **Navigation**: Up/down keys move selection correctly
2. **Scrolling**: Scroll offset updates when navigating
3. **Selection**: Y key triggers select callback
4. **Menu**: 3 key triggers menu callback
5. **Large lists**: Lists with 100+ items work correctly
6. **Callbacks**: userData passed correctly to callbacks

**Test structure** (example):
```cpp
// test/test_list_view/test_navigation.cpp
#include "cdc_views/ListView.h"

static uint16_t s_selectedIndex = 0;
static void* s_selectedData = nullptr;

static void onSelect(uint16_t index, void* userData) {
    s_selectedIndex = index;
    s_selectedData = userData;
}

void test_list_navigation() {
    ListItem items[5] = {
        {"Item 1", 0, false, (void*)1},
        {"Item 2", 0, false, (void*)2},
        {"Item 3", 0, false, (void*)3},
        {"Item 4", 0, false, (void*)4},
        {"Item 5", 0, false, (void*)5},
    };
    
    ListView list;
    list.init("Test", items, 5);
    list.setOnSelect(onSelect);
    
    // Navigate down
    list.onKey('8');
    list.onKey('8');
    
    // Select
    list.onKey('Y');
    
    ASSERT_EQ(s_selectedIndex, 2);
    ASSERT_EQ(s_selectedData, (void*)3);
}

void test_list_large_list() {
    ListItem items[100];
    for (int i = 0; i < 100; i++) {
        items[i].label = "Item";
        items[i].userData = (void*)i;
    }
    
    ListView list;
    list.init("Test", items, 100);
    
    // Navigate to end
    for (int i = 0; i < 99; i++) {
        list.onKey('8');
    }
    
    // Should be at last item
    ASSERT_EQ(list.getSelectedIndex(), 99);
}
```

## References
- [ListView header](components/cdc_views/include/cdc_views/ListView.h)
- [ListView implementation](components/cdc_views/src/ListView.cpp)
- [Usage in modules](components/mod_totp/src/TotpModule.cpp:200)

</content>