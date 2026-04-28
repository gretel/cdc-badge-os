---
title: "[MEDIUM] Data access logic mixed with UI in Password module"
severity: MEDIUM
domain: architecture
lens: separation-of-concerns
labels:
  - "audit:architecture/separation-of-concerns"
---

## Summary
The Password module (`components/mod_password/src/PasswordModule.cpp`) embeds data access patterns (reading entries from `PasswordStore`) directly within UI view callbacks and rendering logic. The `showDetails()` function (line 455) mixes data retrieval, formatting, and UI presentation in a single function.

**Location**: `components/mod_password/src/PasswordModule.cpp:455-512`

## Impact
- **Tight coupling**: UI cannot be changed without touching data access code
- **Hard to test**: Data retrieval logic requires UI stack initialization
- **Code duplication**: Similar patterns appear in list rebuilding, detail display, and context menus
- **No clear boundary**: Data layer, business layer, and presentation layer are intermingled

## Evidence
```cpp
// Line 455-512: showDetails() mixes data access with UI
static void showDetails(uint16_t slot) {
    PasswordEntry entry = {};
    if (!PasswordStore::instance().readEntry(slot, &entry)) {  // Data access
        ui::showToastError(ui::tr(ui::StringId::FAILED));
        return;
    }
    
    // Data formatting for UI
    strncpy(s_passwordToType, entry.password, sizeof(s_passwordToType) - 1);
    snprintf(detailText, sizeof(detailText),
        "Title: %s\nUsername: %s\nPassword: %s\n...",  // Presentation
        entry.title, entry.username, entry.password);
    
    s_infoView.init(mstr(STR_DETAILS), detailText);  // UI
    ui::ViewStack::instance().push(&s_infoView);  // UI
}
```

Similar mixing occurs in:
- `rebuildList()` (line 403): Data access + UI list construction
- `onListMenu()` (line 707): Data retrieval + context menu UI

## Recommended Fix
1. **Create a PasswordViewModel** class:
   ```cpp
   class PasswordViewModel {
       PasswordStore& store_;
   public:
       PasswordEntry loadEntry(uint16_t slot);
       std::string formatDetails(const PasswordEntry& entry);
       std::vector<PasswordEntry> listAllEntries();
   };
   ```

2. **Refactor `showDetails()`** to use view model:
   ```cpp
   static void showDetails(uint16_t slot, PasswordViewModel& vm) {
       auto entry = vm.loadEntry(slot);  // Pure data access
       auto text = vm.formatDetails(entry);  // Pure formatting
       s_infoView.init(mstr(STR_DETAILS), text.c_str());  // Pure UI
   }
   ```

3. **Separate data access from UI callbacks**:
   - Data layer: `PasswordStore::readEntry()`, `PasswordStore::listEntriesSorted()`
   - ViewModel: formats and prepares data for display
   - View: renders formatted data

## References
- [Repository Pattern](https://en.wikipedia.org/wiki/Repository_pattern)
- [Separation of Concerns](https://en.wikipedia.org/wiki/Separation_of_concerns)
- Related issue: TOTP module has similar pattern (`components/mod_totp/src/TotpModule.cpp`)
