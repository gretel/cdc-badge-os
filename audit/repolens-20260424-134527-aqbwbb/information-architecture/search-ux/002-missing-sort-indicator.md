---
title: "[MEDIUM] No visible sort order indicator or user-selectable sort options for lists"
severity: MEDIUM
domain: search-ux
lens: information-architecture
labels:
  - "audit:information-architecture/search-ux"
---

## Summary

While the ListView components in the password and FIDO2 modules display items in sorted order (alphabetically by title/RP ID), there is:
1. **No visual indicator** showing the current sort order to the user
2. **No way for users to change** the sort order (e.g., by date, most recently used, etc.)
3. **No sort direction toggle** (ascending/descending)

This affects:
- Password module list (`components/mod_password/src/PasswordModule.cpp:408-434`) - sorted alphabetically by title
- FIDO2 credentials list (`components/mod_fido2/src/Fido2Ui.cpp:120-167`) - sorted alphabetically by RP ID
- vCard peer list (`components/mod_vcard/src/VcardModule.cpp:264-283`) - sorted by RSSI (signal strength)

## Impact

**Usability Impact:**
- Users cannot tell at a glance how the list is sorted (especially important when lists are large)
- No way to view passwords by most recently added/modified
- No way to view FIDO2 credentials by last used (sign count)
- Users may assume wrong sort order and waste time searching

**Discoverability:**
- The sort functionality (if added later) would be hard to discover without a visible sort control
- No indication that sorting is even a feature

**Comparison with expectations:**
- Most list-based apps show sort indicator (e.g., "A-Z" or "Newest") in the header or footer
- Users expect at least 2-3 sort options for meaningful lists

## Evidence

**Password Store Sort Implementation** (`components/mod_password/src/PasswordStore.cpp:376-378`):
```cpp
std::sort(entries, entries + *countOut, [](const EntryIndex& a, const EntryIndex& b) {
    return PasswordStore::compareTitles(a.title, b.title) < 0;
});
```
- Only alphabetical by title
- No other sort options exposed
- No indication shown to user

**FIDO2 Sort Implementation** (`components/mod_fido2/src/Fido2Ui.cpp:154-157`):
```cpp
std::sort(s_sortMap, s_sortMap + count, [](uint8_t a, uint8_t b) {
    return strcasecmp_safe(s_labels[a], s_labels[b]) < 0;
});
```
- Only alphabetical by RP ID
- No sort by sign count (last used) option
- No visual sort indicator in list header/footer

**ListView Footer** (`components/cdc_views/src/ListView.cpp:264-270`):
```cpp
// Footer with position counter
char positionStr[16];
const char* prefix = nullptr;
if (itemCount_ > 0) {
    snprintf(positionStr, sizeof(positionStr), "%u/%u  ", selection_ + 1, itemCount_);
    prefix = positionStr;
}
const char* hint = getFooterHint();
render::drawFooterBar(gfx, width, height, prefix, hint, true);
```
- Footer only shows position (e.g., "5/23")
- No sort order displayed (e.g., "A-Z" or "Newest")

## Recommended Fix

**Short-term (30 min):**
Add a sort indicator to the list footer showing current sort order:
1. Add `sortIndicator_` member to `ListView` class
2. Add `setSortIndicator(const char*)` method
3. Modify modules to set indicator:
   - Password: "A-Z" in footer
   - FIDO2: "A-Z" in footer
   - vCard: "Signal" in footer

**Example footer display:**
```
5/23  A-Z  [Y] View  [3] Menu  [N] Back
```

**Medium-term (1 hour):**
Add sort order selection via context menu:
1. Add "Sort by..." option to list context menu (key '3')
2. Show submenu with sort options:
   - Passwords: Title (A-Z), Date Added, Last Used
   - FIDO2: RP ID (A-Z), Sign Count (Newest), Sign Count (Oldest)
3. Rebuild list with new sort order

**Implementation scope:**
- `components/cdc_views/include/cdc_views/ListView.h` - Add sort indicator API
- `components/cdc_views/src/ListView.cpp` - Render sort indicator in footer
- `components/mod_password/src/PasswordModule.cpp` - Add sort options to context menu
- `components/mod_fido2/src/Fido2Ui.cpp` - Add sort options to context menu

## References

- Sort indicator patterns: https://www.nngroup.com/articles/sorting-filtering/
- Mobile list UX: https://www.nngroup.com/articles/list-mobile/
