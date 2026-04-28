---
title: "[LOW] No error boundary for unhandled exceptions in view callbacks"
severity: LOW
domain: interaction-design/error-states
lens: error-boundaries
labels:
  - "audit:interaction-design/error-states"
---

## Summary
No global error handling for uncaught exceptions or null pointer dereferences in view callbacks. A single failed callback can crash the entire UI.

**Risk areas:**
- ViewStack dispatching to null views
- Callbacks invoked on stale view pointers
- Module getView() returning null

## Impact
**UI Crash:** Unhandled exceptions in callbacks can freeze the entire interface.

**No graceful degradation:** User loses all context, not just the failing component.

## Evidence
```cpp
// components/cdc_ui/src/ViewStack.cpp
void ViewStack::dispatchKey(char key) {
    IView* view = current();
    if (view) {
        view->onKey(key);  // What if onKey throws or dereferences null member?
    }
}

// components/cdc_os_ui/src/AppUi.cpp
static void onMainMenuSelect(uint16_t index, void* userData) {
    (void)userData;
    if (index < s_mainMenuPluginCount) {
        auto& item = s_mainMenuModuleItems[index];
        if (item.getView) {
            IView* view = item.getView();  // getView() could return null!
            if (view) ViewStack::instance().push(view);  // Check exists but no try-catch
        }
        return;
    }
}

// components/mod_totp/src/TotpModule.cpp
static void onListSelect(uint16_t index, void* userData) {
    (void)userData;
    if (index == 0) {
        wizardStart();
        return;
    }
    if (index - 1 >= s_accountCount) return;  // Bounds check

    uint16_t slot = s_listSlots[index - 1];
    const char* name = s_listLabels[index - 1];  // Could be null!
    s_codeView.init(slot, name);
    ui::ViewStack::instance().push(&s_codeView);
}

// Components/cdc_views/src/ListView.cpp
InputResult ListView::onKey(char key) {
    switch (key) {
        case 'Y': // Select
            if (onSelect_ && items_ && selection_ < itemCount_) {
                onSelect_(selection_, items_[selection_].userData);  // Callback could crash
            }
            return InputResult::CONSUMED;
```

**Null pointer risks in ToastView:**
```cpp
// components/cdc_views/src/ToastView.cpp
void ToastView::render(bool partial) {
    hal::IDisplay* display = hal::getDisplayInstance();
    if (!display) return;  // Good check

    auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());
    if (!gfx) return;  // Good check

    // But what if message_ is accessed after it's been freed?
    gfx->print(message_);
}
```

## Recommended Fix
**1. Add try-catch wrappers for callbacks:**

```cpp
// ViewStack.cpp
void ViewStack::dispatchKey(char key) {
    IView* view = current();
    if (view) {
        try {
            view->onKey(key);
        } catch (const std::exception& e) {
            LOG_E("ViewStack", "Exception in view %s: %s", view->getName(), e.what());
            // Show error toast
            showToastError("UI error, please restart");
        }
    }
}

static void onMainMenuSelect(uint16_t index, void* userData) {
    try {
        if (index < s_mainMenuPluginCount) {
            auto& item = s_mainMenuModuleItems[index];
            if (item.getView) {
                IView* view = item.getView();
                if (view) ViewStack::instance().push(view);
            }
            return;
        }
        // ... rest of switch
    } catch (const std::exception& e) {
        LOG_E("AppUi", "Exception in main menu: %s", e.what());
        showToastError("Menu error");
    }
}
```

**2. Add null checks for all callback data:**

```cpp
// ListView.cpp
InputResult ListView::onKey(char key) {
    switch (key) {
        case 'Y': // Select
            if (onSelect_ && items_ && selection_ < itemCount_) {
                const ListItem& item = items_[selection_];
                // Ensure label and userData are valid
                if (item.label || item.userData) {
                    try {
                        onSelect_(selection_, item.userData);
                    } catch (...) {
                        LOG_E("ListView", "Exception in onSelect");
                    }
                }
            }
            return InputResult::CONSUMED;
```

**3. Add ViewBase error handler hook:**

```cpp
// IView.h
class IView {
public:
    virtual void onError(const char* error) {
        // Default: log error
        LOG_E("IView", "Error in %s: %s", getName(), error);
    }
};

// ViewBase implementation
class ViewBase : public IView {
protected:
    void pushError(const char* error) {
        onError(error);
        showToastError(error);
    }
};
```

**4. Validate getView() return values:**

```cpp
// AppUi.cpp
static void onMainMenuSelect(uint16_t index, void* userData) {
    if (index < s_mainMenuPluginCount) {
        auto& item = s_mainMenuModuleItems[index];
        if (item.getView) {
            IView* view = item.getView();
            if (view) {
                // Validate view is usable
                if (view->getName()) {
                    ViewStack::instance().push(view);
                } else {
                    LOG_E("AppUi", "Module %s returned view with null name", item.moduleName);
                    showToastError("Module error");
                }
            } else {
                LOG_W("AppUi", "Module %s getView returned null", item.moduleName);
                showToastError("Module not ready");
            }
        }
        return;
    }
    // ...
}
```

**Estimated effort:** 1 hour for ViewStack try-catch wrappers, 30 min for callback null checks

## References
- [React Error Boundaries](https://reactjs.org/docs/error-boundaries.html)
- [C++ Exception Safety](https://en.cppreference.com/w/cpp/language/exception)
