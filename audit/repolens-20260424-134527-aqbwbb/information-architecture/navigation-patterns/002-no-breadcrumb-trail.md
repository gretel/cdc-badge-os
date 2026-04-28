---
title: "[MEDIUM] No breadcrumb trail for deep navigation hierarchy"
severity: MEDIUM
domain: information-architecture/navigation-patterns
lens: navigation-patterns
labels:
  - "audit:information-architecture/navigation-patterns"
---

## Summary
The application uses a deep hierarchical navigation structure (Lock Screen → Main Menu → Tools → Expert → Modules, or Main Menu → FIDO2 → Credential Details → User-Presence Prompt) but provides no breadcrumb trail or title context to show users their current location in the hierarchy.

**Files affected:**
- `components/cdc_os_ui/src/AppUi.cpp` - Main navigation flow (lines 391-470)
- `components/cdc_ui/include/cdc_ui/ViewStack.h` - View stack management (lines 16-60)
- `components/cdc_views/include/cdc_views/ListView.h` - Menu view component

## Impact
Users navigating deep menus lose context of:
1. How many levels they are from the root
2. What path they took to reach the current view
3. How to quickly return to a parent section

For example:
- From Lock Screen → Main Menu → Tools → Expert → Modules → [Module Name] = 5 levels deep
- From Main Menu → FIDO2 → Details → [Credential] = 3 levels deep

Without breadcrumbs, users must rely on back-button navigation (key 'N') to retrace their steps, which is inefficient for deep hierarchies.

## Evidence
The ViewStack manages navigation depth but exposes no breadcrumb API:

```cpp
// components/cdc_ui/include/cdc_ui/ViewStack.h (lines 48-58)
void popToRoot();  // Returns to root only
IView* current() const;
IView* at(uint8_t depth) const;
uint8_t depth() const { return depth_ = 0; }
```

View titles exist but are not hierarchical. For example, in FIDO2 UI:

```cpp
// components/mod_fido2/src/Fido2Ui.cpp (lines 200-220)
snprintf(detail_text, sizeof(detail_text),
         "Relying Party:\n%s\n\n"
         "Type: %s  Algo: %s\n"
         // ...
s_detailView->init(mstr(STR_FIDO2_KEY), detail_text);  // Title: "FIDO2 Key"
```

The title "FIDO2 Key" doesn't indicate this is a detail view within WebAuthn menu.

Navigation callbacks push views without context:

```cpp
// components/mod_fido2/src/Fido2Ui.cpp (line 223)
ui::ViewStack::instance().push(s_detailView);
```

No breadcrumb trail is shown in the header or footer of views.

## Recommended Fix
Implement a minimal breadcrumb system:

1. **Add breadcrumb tracking to ViewStack** (lines 158-172 area in ViewStack.h):
   ```cpp
   struct ViewEntry {
       IView* view;
       char title[32];
       uint8_t depth;
   };
   ViewEntry breadcrumbStack_[MAX_DEPTH];
   ```

2. **Update push/replace to maintain breadcrumbs**:
   ```cpp
   void push(IView* view, void* context = nullptr) {
       // Existing push logic...
       snprintf(breadcrumbStack_[depth_].title, sizeof(breadcrumbStack_[depth_].title), "%s", view->getName());
   }
   ```

3. **Add breadcrumb renderer** to IView interface:
   ```cpp
   virtual const char* getBreadcrumb() const { return getTitle(); }
   ```

4. **Display breadcrumbs in ListView header** - show path like "Main > Tools > Expert"

This can be implemented as a focused ~1 hour task by:
- Adding breadcrumb path to ViewStack (30 min)
- Updating menu views to display path (30 min)

## References
- Breadcrumb navigation: [MDN - Breadcrumbs](https://developer.mozilla.org/en-US/docs/Web/Accessibility/ARIA/Roles/breadcrumb_role)
- Current ViewStack: `components/cdc_ui/include/cdc_ui/ViewStack.h`
- Navigation depth tracking: `components/cdc_os_ui/src/AppUi.cpp` line 773
