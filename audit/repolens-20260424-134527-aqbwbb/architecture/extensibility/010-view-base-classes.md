---
title: "[LOW] View Classes Use Static View Instances with Manual Initialization"
severity: LOW
domain: extensibility
lens: ui-architecture
labels:
  - "audit:architecture/extensibility"
---

## Summary
Module views use static instances with manual `s_viewsInitialized` flags (e.g., `TotpModule.cpp:545-550`). This pattern requires tracking initialization state and causes issues if views need to be recreated.

**Evidence:**
- `components/mod_totp/src/TotpModule.cpp:545-560`:
  ```cpp
  static ui::ListView s_listView;
  static ui::T9InputView s_t9Input;
  static ui::ListView s_digitsMenu;
  static ui::ListView s_algoMenu;
  static ui::ListView s_periodMenu;
  static TotpCodeView s_codeView;
  static bool s_viewsInitialized = false;
  
  static void rebuildList() {
      // ...
      if (!s_viewsInitialized) {
          s_listView.setOnSelect(onListSelect);
          s_viewsInitialized = true;
      }
      // ...
  }
  ```

- `components/mod_gpg/src/GpgModule.cpp`: Similar pattern
- `components/grove_led/src/GroveLedModule.cpp`: Similar pattern

## Impact
**Fragile View Management:**
1. Views can only be initialized once
2. No way to reset/recreate views
3. Static instances tied to module lifetime
4. Hard to test views in isolation

## Evidence
Files affected:
- `components/mod_totp/src/TotpModule.cpp:545-560` (views)
- `components/mod_gpg/src/GpgModule.cpp` (views)
- `components/grove_led/src/GroveLedModule.cpp` (views)
- `components/cdc_ui/include/cdc_ui/ViewStack.h` (view stack)

## Recommended Fix
Use factory pattern:

1. **View factory:**
   ```cpp
   class ViewFactory {
   public:
       using ViewCreator = std::function<ui::IView*>();
       void registerView(const char* name, ViewCreator creator);
       ui::IView* createView(const char* name);
   };
   ```

2. **Module registers views:**
   ```cpp
   static void registerViews() {
       ViewFactory::instance().registerView("totp_list", []() {
           auto* view = new ui::ListView();
           view->setOnSelect(onListSelect);
           return view;
       });
   }
   ```

3. **Or lazy initialization:**
   ```cpp
   static ui::ListView* getListview() {
       static ui::ListView* s_listView = nullptr;
       if (!s_listView) {
           s_listView = new ui::ListView();
           s_listView->setOnSelect(onListSelect);
       }
       return s_listView;
   }
   ```

## References
- Factory Pattern
- Lazy initialization
- View model pattern
