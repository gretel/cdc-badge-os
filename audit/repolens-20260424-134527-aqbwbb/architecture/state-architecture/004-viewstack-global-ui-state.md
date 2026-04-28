---
title: "[MEDIUM] ViewStack singleton maintains global UI state without clear ownership"
severity: MEDIUM
domain: State Management Architecture
lens: state-architecture
labels:
  - "audit:architecture/state-architecture"
---

## Summary
The `ViewStack` singleton (`components/cdc_ui/include/cdc_ui/ViewStack.h`) maintains global UI navigation state including the view stack, modal overlay, and inactivity timer. The state is accessible from anywhere via `ViewStack::instance()`, creating implicit dependencies and making it hard to track which modules are modifying the UI state.

### Evidence:
- `ViewStack.h:16-173`: Single struct with `stack_[MAX_DEPTH]`, `modal_`, `inactivityCallback_`, `needsFullRefresh_`
- `ViewStack.h:73-88`: `dispatchKey()`, `dispatchTick()` methods modify stack state
- `ViewStack.h:107-120`: `showModal()`, `hideModal()` modify `modal_` state
- `ViewStack.h:132-150`: Inactivity timer state (`inactivityCallback_`, `lastActivityMs_`) is global

### State Access Pattern:
```cpp
// Anywhere in code can push/pop views
ViewStack::instance().push(newView);
ViewStack::instance().pop();
ViewStack::instance().showModal(toastView);
```

## Impact
1. **Implicit dependencies**: Modules can push views without explicit ownership (e.g., FIDO2 UI pushes views from callback)
2. **State race conditions**: Multiple modules can call `push()`/`pop()` simultaneously (no mutex)
3. **Debugging difficulty**: Hard to trace which module pushed which view
4. **Inactivity timer conflicts**: Multiple modules can set different timeout callbacks

## Recommended Fix
1. Add state ownership tracking:
   ```cpp
   struct ViewState {
       IView* view;
       const char* ownerModule;  // Track which module pushed this view
       void* context;
   };
   
   void push(IView* view, const char* ownerModule, void* context = nullptr);
   ```

2. Add state query API:
   ```cpp
   typedef struct {
       uint8_t depth;
       ViewState stack[MAX_DEPTH];
       IView* modal;
       bool needsFullRefresh;
   } ViewStackState;
   
   ViewStackState getState() const;
   ```

3. Add owner validation:
   ```cpp
   void pop(const char* expectedOwner = nullptr);  // Only pop if matches owner
   ```

4. Add inactivity timer guard:
   ```cpp
   bool setInactivityTimeout(InactivityCallback callback, uint32_t timeoutMs, const char* owner);
   // Returns false if another module already set timer
   ```

## References
- `components/cdc_ui/include/cdc_ui/ViewStack.h` - Full header
- `components/cdc_ui/src/ViewStack.cpp` - Implementation
- `components/cdc_os_ui/src/AppUi.cpp` - Usage example
