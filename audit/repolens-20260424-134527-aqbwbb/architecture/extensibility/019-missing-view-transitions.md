---
title: "[LOW] Missing Extension Point for View Transitions"
severity: LOW
domain: architecture/extensibility
lens: view-navigation
labels:
  - "audit:architecture/extensibility"
---

## Summary
View transitions in the ViewStack use a simple push/pop mechanism with no hooks for animations, transitions, or custom navigation logic. Adding slide animations, fade effects, or navigation history requires modifying the ViewStack core.

**Files affected:**
- `components/cdc_ui/include/cdc_ui/ViewStack.h`
- `components/cdc_ui/src/ViewStack.cpp`

## Impact
- **UI polish**: Cannot add visual transitions (slide, fade, scale) without modifying ViewStack
- **Navigation history**: No built-in support for back-stack with custom navigation logic
- **Transition effects**: Modules cannot customize how their views appear/disappear
- **Animation timing**: No hooks for coordinating animations with data loading

## Evidence

### Simple Push/Pop Implementation
```cpp
// components/cdc_ui/include/cdc_ui/ViewStack.h
class ViewStack {
public:
    void push(IView* view);
    void pop();
    IView* current();
    uint8_t depth();
};

// components/cdc_ui/src/ViewStack.cpp
void ViewStack::push(IView* view) {
    if (depth_ < MAX_DEPTH) {
        stack_[depth_++] = view;
        view->onEnter();
    }
}

void ViewStack::pop() {
    if (depth_ > 0) {
        stack_[--depth_]->onExit();
    }
}
```

### No Transition Hooks
There is no interface for:
```cpp
// Missing - should exist:
class IViewTransition {
public:
    virtual void enter(IView* view) = 0;
    virtual void exit(IView* view) = 0;
    virtual bool isComplete() const = 0;
};
```

## Recommended Fix

### Create Transition Interface
```cpp
// components/cdc_ui/include/cdc_ui/ViewTransition.h
#pragma once
#include <cstdint>

namespace cdc::ui {

class IView {
    // Existing interface...
};

class IViewTransition {
public:
    virtual ~IViewTransition() = default;
    
    // Called when pushing a new view
    virtual void pushStart(IView* from, IView* to) = 0;
    
    // Called when popping a view
    virtual void popStart(IView* from, IView* to) = 0;
    
    // Check if transition is complete
    virtual bool isComplete() const = 0;
    
    // Update transition (call each frame)
    virtual void update(uint32_t deltaMs) = 0;
};

class TransitionRegistry {
public:
    static TransitionRegistry& instance();
    void registerTransition(const char* name, IViewTransition* transition);
    IViewTransition* getTransition(const char* name);
};

// Built-in transitions
class NoTransition : public IViewTransition {
    void pushStart(IView* from, IView* to) override {}
    void popStart(IView* from, IView* to) override {}
    bool isComplete() const override { return true; }
    void update(uint32_t deltaMs) override {}
};

class SlideTransition : public IViewTransition {
    uint8_t offset_;
    uint32_t duration_;
    uint32_t elapsed_;
    
public:
    SlideTransition(uint8_t offset, uint32_t duration = 200);
    void pushStart(IView* from, IView* to) override;
    void popStart(IView* from, IView* to) override;
    bool isComplete() const override { return elapsed_ >= duration_; }
    void update(uint32_t deltaMs) override { elapsed_ += deltaMs; }
};

class FadeTransition : public IViewTransition {
    uint8_t alpha_;
    uint32_t duration_;
    uint32_t elapsed_;
    
public:
    FadeTransition(uint32_t duration = 150);
    void pushStart(IView* from, IView* to) override;
    void popStart(IView* from, IView* to) override;
    bool isComplete() const override { return elapsed_ >= duration_; }
    void update(uint32_t deltaMs) override { elapsed_ += deltaMs; }
};

} // namespace cdc::ui
```

### Refactor ViewStack to Use Transitions
```cpp
// components/cdc_ui/include/cdc_ui/ViewStack.h
class ViewStack {
public:
    void push(IView* view, const char* transition = "slide");
    void pop(const char* transition = "slide");
    
    // Set default transition
    void setDefaultTransition(const char* name);
};

// components/cdc_ui/src/ViewStack.cpp
void ViewStack::push(IView* view, const char* transition) {
    if (depth_ >= MAX_DEPTH) return;
    
    auto* trans = TransitionRegistry::instance().getTransition(transition);
    if (!trans) trans = TransitionRegistry::instance().getTransition("no-transition");
    
    IView* from = current();
    stack_[depth_++] = view;
    trans->pushStart(from, view);
    
    // Start transition animation loop
    while (!trans->isComplete()) {
        trans->update(16);  // ~60fps
        if (from) from->render(false);
        view->render(false);
        display->update();
    }
    
    view->onEnter();
}
```

### Module-Registered Transitions
```cpp
// components/mod_ui_effects/src/EffectsModule.cpp
class WipeTransition : public IViewTransition {
    // Custom wipe animation
};

void mod_ui_effects_register() {
    auto& registry = TransitionRegistry::instance();
    registry.registerTransition("wipe", new WipeTransition());
}
```

## References
- State Pattern for transitions: https://refactoring.guru/design-patterns/state
- Observer Pattern for animation updates: https://refactoring.guru/design-patterns/observer
- View Controller transitions (iOS example): https://developer.apple.com/documentation/uikit/uiviewcontroller/1621390-transitioncoordinator

</content>