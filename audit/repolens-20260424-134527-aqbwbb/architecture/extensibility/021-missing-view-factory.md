---
title: "[MEDIUM] Missing View Factory Pattern for Dynamic View Creation"
severity: MEDIUM
domain: architecture/extensibility
lens: view-factory
labels:
  - "audit:architecture/extensibility"
---

## Summary
UI views are created using hardcoded direct instantiation (`new ListView()`, `new InfoView()`, etc.) scattered throughout module code. There is no central view factory or registry that would allow views to be created dynamically, registered by modules, or swapped for different implementations without modifying calling code.

**Files affected:**
- `components/cdc_os_ui/src/AppUi.cpp` (lines 504-671) - hardcoded view instantiation
- `components/mod_gpg/src/GpgModule.cpp` (lines 206-213) - static view instances
- `components/mod_totp/src/TotpModule.cpp` - static view instances
- `components/mod_password/src/PasswordModule.cpp` - static view instances
- `components/cdc_os_ui/src/AppUi.cpp:585` - `s_pinEntry = new PinEntryView()`
- `components/cdc_os_ui/src/AppUi.cpp:591` - `s_mainMenu = new ListView()`

## Impact
- **View extensibility**: Adding a new view type requires finding all places that create views and potentially modifying them
- **Theme/skin support**: Cannot dynamically swap views for themed versions (e.g., high-contrast, compact)
- **Testing difficulty**: Hard to inject mock views for UI testing
- **Memory management**: Static view instances are allocated at compile time, limiting flexibility
- **Module isolation**: Views are tightly coupled to their modules, making reuse difficult

## Evidence

### Hardcoded View Instantiation in AppUi
`components/cdc_os_ui/src/AppUi.cpp:585-671`:
```cpp
void AppUi::initViews() {
    // ...
    s_pinEntry = new PinEntryView();
    s_mainMenu = new ListView();
    s_toolsMenu = new ListView();
    s_settingsMenu = new ListView();
    s_brightnessSlider = new SliderView();
    s_sleepSlider = new SliderView();
    s_timezoneSlider = new SliderView();
    s_languageMenu = new ListView();
    s_dateInput = new DateInputView();
    s_timeInput = new TimeInputView();
    s_pinChangeView = new PinEntryView();
    // ...
}
```

### Static View Instances in Modules
`components/mod_gpg/src/GpgModule.cpp:206-213`:
```cpp
static ui::ListView s_menuView;
static ui::ListView s_settingsView;
static ui::PinChangeView s_pinChangeView;
static ui::T9InputView s_t9Input;
static ui::ListView s_curveView;
static ui::InfoView s_infoView;
static ui::QRCodeView s_qrView;
static bool s_viewsInitialized = false;
```

### No View Registry or Factory
There is no interface like:
```cpp
// Missing - should exist:
class IViewFactory {
public:
    virtual ui::IView* createView(ViewType type) = 0;
    virtual ui::IView* createViewByName(const char* name) = 0;
};

class ViewRegistry {
public:
    static ViewRegistry& instance();
    void registerViewFactory(const char* name, IViewFactory* factory);
    ui::IView* createView(const char* name);
};
```

### View Creation Patterns in Code
`components/cdc_os_ui/src/BluetoothMenuUi.cpp:156`:
```cpp
s_bluetoothMenu = new ListView();
```

`components/mod_nvsedit/src/NvsEditModule.cpp:409`:
```cpp
s_namespaceListView = new ListView();
```

All these locations would need to be modified to support:
- Custom view implementations
- View pooling/reuse
- Dynamic view loading
- Theme switching

## Recommended Fix

### Create View Factory Interface
```cpp
// components/cdc_ui/include/cdc_ui/ViewFactory.h
#pragma once
#include "cdc_ui/IView.h"
#include <cstdint>

namespace cdc::ui {

/**
 * View type identifiers for factory creation
 */
enum class ViewType {
    LIST,
    INFO,
    PIN_ENTRY,
    SLIDER,
    DATE_INPUT,
    TIME_INPUT,
    T9_INPUT,
    CONFIRM,
    TOAST,
    QR_CODE,
    MENU,
    CUSTOM  // For module-specific views
};

/**
 * Abstract view factory interface
 */
class IViewFactory {
public:
    virtual ~IViewFactory() = default;
    virtual IView* create() const = 0;
    virtual const char* getName() const = 0;
    virtual ViewType getType() const = 0;
};

/**
 * View registry for dynamic view creation
 */
class ViewRegistry {
public:
    static ViewRegistry& instance();
    
    /**
     * Register a view factory
     * @param factory Factory instance (must remain valid)
     */
    void registerFactory(IViewFactory* factory);
    
    /**
     * Create a view by type
     * @param type View type
     * @return New view instance, or nullptr if not found
     */
    IView* createView(ViewType type);
    
    /**
     * Create a view by name
     * @param name View name (e.g., "ListView", "PinEntryView")
     * @return New view instance, or nullptr if not found
     */
    IView* createViewByName(const char* name);
    
    /**
     * Get all registered view names
     */
    void getViewNames(char** names, uint8_t* count, uint8_t maxCount);
};

// Convenience factory functions
IView* createListView();
IView* createInfoView();
IView* createPinEntryView();
IView* createSliderView();
// ... etc

// Template factory for any view type
template<typename T>
class ViewFactoryImpl : public IViewFactory {
public:
    ViewType getType() const override { return T::getViewType(); }
    const char* getName() const override { return T::getViewTypeName(); }
    IView* create() const override { return new T(); }
};

} // namespace cdc::ui
```

### Implement Factories for Built-in Views
```cpp
// components/cdc_views/src/ListViewFactory.cpp
namespace cdc::ui {

class ListViewFactory : public IViewFactory {
public:
    ViewType getType() const override { return ViewType::LIST; }
    const char* getName() const override { return "ListView"; }
    IView* create() const override { return new ListView(); }
};

static ListViewFactory s_listViewFactory;

void registerListViewFactory() {
    ViewRegistry::instance().registerFactory(&s_listViewFactory);
}

} // namespace cdc::ui
```

### Register Views at Startup
```cpp
// components/cdc_ui/src/ViewRegistry.cpp
#include "cdc_ui/ViewFactory.h"

namespace cdc::ui {

static IViewFactory* s_viewFactories[] = {
    new ListViewFactory(),
    new InfoViewFactory(),
    new PinEntryViewFactory(),
    new SliderViewFactory(),
    new DateInputViewFactory(),
    new TimeInputViewFactory(),
    new T9InputViewFactory(),
    new ConfirmViewFactory(),
    new ToastViewFactory(),
    new QRCodeViewFactory(),
};

ViewRegistry& ViewRegistry::instance() {
    static ViewRegistry inst;
    return inst;
}

ViewRegistry::ViewRegistry() {
    for (auto* factory : s_viewFactories) {
        registerFactory(factory);
    }
}

void ViewRegistry::registerFactory(IViewFactory* factory) {
    factories_[count_++] = factory;
}

IView* ViewRegistry::createView(ViewType type) {
    for (size_t i = 0; i < count_; i++) {
        if (factories_[i]->getType() == type) {
            return factories_[i]->create();
        }
    }
    return nullptr;
}

} // namespace cdc::ui
```

### Refactor Modules to Use Factory
```cpp
// components/cdc_os_ui/src/AppUi.cpp
void AppUi::initViews() {
    auto& registry = ViewRegistry::instance();
    
    s_pinEntry = static_cast<PinEntryView*>(registry.createView(ViewType::PIN_ENTRY));
    s_mainMenu = static_cast<ListView*>(registry.createView(ViewType::LIST));
    s_settingsMenu = static_cast<ListView*>(registry.createView(ViewType::LIST));
    s_brightnessSlider = static_cast<SliderView*>(registry.createView(ViewType::SLIDER));
    // ...
}

// Or use convenience functions:
s_mainMenu = static_cast<ListView*>(createListView());
```

### Enable Theme Support
```cpp
// components/mod_theme_dark/src/DarkViewFactory.cpp
class DarkListViewFactory : public IViewFactory {
public:
    ViewType getType() const override { return ViewType::LIST; }
    const char* getName() const override { return "DarkListView"; }
    IView* create() const override { return new DarkListView(); }
};

// Register at startup to override default
void mod_theme_dark_register() {
    auto& registry = ViewRegistry::instance();
    registry.registerFactory(new DarkListViewFactory());  // Replaces default
}
```

### Enable Module-Specific Views
```cpp
// components/mod_password/src/PasswordListView.h
class PasswordListView : public ListView {
public:
    static ViewType getViewType() { return ViewType::CUSTOM; }
    static const char* getViewTypeName() { return "PasswordListView"; }
    void init(const char* title, const PasswordEntry* entries, uint16_t count);
};

// components/mod_password/src/PasswordModule.cpp
static void showPasswordList() {
    auto& registry = ViewRegistry::instance();
    auto* view = static_cast<PasswordListView*>(registry.createViewByName("PasswordListView"));
    view->init("Passwords", s_entries, s_count);
    ViewStack::instance().push(view);
}
```

## References
- Factory Pattern: https://refactoring.guru/design-patterns/factory-method
- Abstract Factory: https://refactoring.guru/design-patterns/abstract-factory
- View Model Pattern: https://en.wikipedia.org/wiki/Model%E2%80%93view%E2%80%93viewmodel
- Plugin Architecture: https://martinfowler.com/articles/plugins.html

</content>