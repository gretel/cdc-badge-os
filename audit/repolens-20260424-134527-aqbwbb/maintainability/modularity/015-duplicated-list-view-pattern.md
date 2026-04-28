---
title: "[MEDIUM] Duplicated list-view boilerplate in module UI implementations"
severity: MEDIUM
domain: modularity
lens: modularity
labels:
  - "audit:maintainability/modularity"
---

## Summary
The `mod_totp` and `mod_password` modules contain nearly identical list-view boilerplate code for displaying and managing collections of items. This includes buffer management, list rebuilding, and selection handling logic that could be extracted into a reusable component.

**Locations:**
- `components/mod_totp/src/TotpModule.cpp`: lines 200-350 (list view implementation)
- `components/mod_password/src/PasswordModule.cpp`: lines 150-280 (list view implementation)

## Impact
**Code Duplication:**
- Both modules implement similar list-view patterns independently
- Any improvements or bug fixes to list management must be applied in multiple places
- Increased maintenance burden as features evolve

**Missed Abstraction Opportunity:**
- The pattern "display list of items with add/edit/delete" is common across modules
- A reusable `ModuleListView` component could serve TOTP, Password, and future modules

**Inconsistency Risk:**
- Slight variations in implementation may lead to different user experiences
- Harder to ensure uniform behavior across similar features

## Evidence
**Common patterns in both modules:**

1. **Static buffer declarations:**
```cpp
// TotpModule.cpp
static char* s_listLabels[MAX_ITEMS];
static uint16_t s_listSlots[MAX_ITEMS];
static uint16_t s_capacity = 0;

// PasswordModule.cpp
static char* s_listLabels[MAX_ITEMS];
static PasswordStore::EntryIndex* s_entries = nullptr;
static uint16_t s_capacity = 0;
```

2. **Buffer allocation helper:**
```cpp
// TotpModule.cpp
static bool ensureListBuffers() {
    if (s_listLabels != nullptr) return true;
    s_capacity = TotpStore::instance().capacity();
    s_listLabels = new (psram) char*[s_capacity + 1];
    s_listSlots = new (psram) uint16_t[s_capacity];
    // ...
}

// PasswordModule.cpp
static bool ensureListBuffers() {
    if (s_listLabels != nullptr) return true;
    s_capacity = PasswordStore::instance().capacity();
    s_listLabels = new (psram) char*[s_capacity + 1];
    s_entries = new (psram) PasswordStore::EntryIndex[s_capacity];
    // ...
}
```

3. **List rebuild pattern:**
```cpp
// TotpModule.cpp
static void rebuildList() {
    if (!ensureListBuffers()) {
        cdc::core::ModuleRegistry::instance().reportModuleError(TotpModule::instance().getName(),
                                                               "TOTP list allocation failed");
        return;
    }
    s_accountCount = 0;
    s_listItems[0] = {mstr(STR_ADD_ACCOUNT), 0, false, nullptr};
    // ... iterate and populate
}

// PasswordModule.cpp
static void rebuildList() {
    if (!ensureListBuffers()) {
        cdc::core::ModuleRegistry::instance().reportModuleError(PasswordModule::instance().getName(),
                                                               "Password list allocation failed");
        return;
    }
    s_entryCount = 0;
    s_listItems[0] = {mstr(STR_NEW_ENTRY), 0, false, nullptr};
    // ... iterate and populate
}
```

4. **List select handler:**
```cpp
// TotpModule.cpp
static void onListSelect(uint16_t index, void* userData) {
    if (index == 0) { wizardStart(); return; }
    wizardEdit(s_listSlots[index - 1]);
}

// PasswordModule.cpp
static void onListSelect(uint16_t index, void* userData) {
    if (index == 0) { wizardStart(); return; }
    uint16_t slot = static_cast<uint16_t>(reinterpret_cast<uintptr_t>(s_listItems[index].userData));
    wizardEdit(slot);
}
```

5. **List view initialization:**
```cpp
// Both modules:
s_listView.init(mstr(STR_XXX), s_listItems, static_cast<uint16_t>(count + 1));
s_listView.setHint(mstr(STR_HINT_LIST));
```

## Recommended Fix
Extract the common list-view pattern into a reusable component:

**Option 1: Create a ModuleListView helper class**

Create `components/cdc_views/include/cdc_views/ModuleListView.h`:
```cpp
namespace cdc::ui {

class ModuleListView {
public:
    struct Item {
        const char* label;
        void* userData;
        uint16_t icon;
        bool iconDisabled;
    };

    using OnSelectFn = std::function<void(uint16_t index)>;

    ModuleListView();
    ~ModuleListView();

    void init(const char* title, uint16_t capacity, OnSelectFn onSelect);
    void rebuild(std::function<void(Item* items, uint16_t& count)> populateFn);
    void show();

    const char* getTitle() const { return title_; }

private:
    char** labels_ = nullptr;
    Item* items_ = nullptr;
    uint16_t capacity_ = 0;
    OnSelectFn onSelectFn_;
    ui::ListView listView_;
    char* title_ = nullptr;
};

} // namespace cdc::ui
```

**Option 2: Create a module-ui base class**

Create `components/cdc_os_ui/include/cdc_os_ui/ModuleUiBase.h`:
```cpp
namespace cdc::ui {

class ModuleUiBase {
public:
    struct ListItem {
        const char* label;
        void* userData;
        uint16_t icon;
        bool iconDisabled;
    };

    ModuleUiBase(const char* title, uint16_t firstIndexHint = 0);
    virtual ~ModuleUiBase();

    void setupList(uint16_t capacity);
    void refreshList(std::function<void(ListItem* items, uint16_t& count)> populateFn);
    void onListItemSelect(uint16_t index);

protected:
    virtual void onAddItem() = 0;
    virtual void onEditItem(uint16_t index) = 0;
    virtual void onDeleteItem(uint16_t index);

    ui::ListView listView_;
    uint16_t capacity_ = 0;
    char** labels_ = nullptr;
    ListItem* items_ = nullptr;

private:
    const char* title_;
    uint16_t firstItemHint_;
};

} // namespace cdc::ui
```

**Usage in modules:**
```cpp
// TotpModule.cpp
class TotpUi : public ui::ModuleUiBase {
public:
    TotpUi() : ModuleUiBase("TOTP", 1) {}

protected:
    void onAddItem() override { wizardStart(); }
    void onEditItem(uint16_t index) override { wizardEdit(slots_[index]); }

private:
    uint16_t* slots_ = nullptr;
};
```

## References
- Template Method pattern: https://en.wikipedia.org/wiki/Template_method_pattern
- Strategy pattern for populate callbacks: https://en.wikipedia.org/wiki/Strategy_pattern
- Existing project pattern: `cdc_views` component already contains reusable UI components
- DRY principle: https://en.wikipedia.org/wiki/Don%27t_repeat_yourself
