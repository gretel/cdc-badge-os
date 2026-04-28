---
title: "[HIGH] Missing search/filter functionality for list-based UI components"
severity: HIGH
domain: search-ux
lens: information-architecture
labels:
  - "audit:information-architecture/search-ux"
---

## Summary

The CDC Badge OS firmware lacks search and filter functionality for its list-based UI components. Three key modules use `ListView` to display potentially large datasets without any way to quickly find items:

1. **Password Module** (`components/mod_password/src/PasswordModule.cpp:408-434`) - Can store up to 162 password entries (slots 150-511), currently displayed in a simple scrollable list
2. **FIDO2 Module** (`components/mod_fido2/src/Fido2Ui.cpp:120-167`) - Supports up to 27 credentials, displayed in alphabetically sorted list
3. **vCard Module** (`components/mod_vcard/src/VcardModule.cpp:264-283`) - Displays nearby BLE peers (up to 16)

The `T9InputView` component (`components/cdc_views/src/T9InputView.cpp`) exists for text input but is not integrated with the ListView for search/filter purposes.

## Impact

**Usability Impact:**
- Users with 20+ password entries must scroll through the entire list to find a specific entry
- No quick way to locate a password by title, username, or URL
- FIDO2 credential lookup becomes tedious with multiple credentials for different services
- vCard peer discovery lacks filtering by device name

**Scalability:**
- Password module supports 162 entries but becomes impractical to use beyond ~10 entries without search
- As the badge accumulates more credentials/entries, user experience degrades linearly with list size

**Competitive Gap:**
- Modern password managers and FIDO2 implementations all provide search functionality
- Users expect at minimum 1-2 keystroke filtering for lists larger than 10 items

## Evidence

**Password Module List Display** (`components/mod_password/src/PasswordModule.cpp:408-434`):
```cpp
static void rebuildList() {
    // ...
    for (uint16_t i = 0; i < s_entryCount; i++) {
        uint16_t idx = static_cast<uint16_t>(i + 1);
        s_listItems[idx].label = s_entries[i].title;
        // No filtering by search query
    }
    s_listView.init(mstr(STR_PASSWORDS), s_listItems, static_cast<uint16_t>(s_entryCount + 1));
}
```

**FIDO2 Credential List** (`components/mod_fido2/src/Fido2Ui.cpp:120-167`):
```cpp
static void rebuildList() {
    // ...
    for (uint8_t i = 0; i < count && i < FIDO2_MAX_CREDENTIALS; i++) {
        // ...
        s_listItems[display].label = s_labels[store_idx];
        // Only alphabetical sort, no search filtering
    }
}
```

**T9InputView Available but Unused for Search** (`components/cdc_views/include/cdc_views/T9InputView.h:8-19`):
```cpp
/**
 * T9InputView - Multi-tap text input
 *
 * Classic phone-style T9 text entry.
 * Each number key cycles through characters on repeated press.
 *
 * Keys:
 *   0-9 = Character input (multi-tap)
 *   Long-press 0-9 = Insert digit directly
 *   N = Backspace (short) / Clear (long)
 *   Y = Confirm (triggers callback)
 */
```

The T9InputView is currently only used for entering password metadata (title, username, URL, notes) in a wizard flow, not for filtering lists.

## Recommended Fix

Implement a search/filter mechanism that works with the T9 keypad input:

**Option 1: T9-based incremental filter (Recommended for embedded)**
1. Add a "Search" action to the ListView context menu (key '3')
2. When activated, push a `T9InputView` with placeholder "Search..."
3. As user types, filter the list items in real-time (case-insensitive substring match on title/username)
4. Show "No matches" message if filter returns zero results
5. Press 'N' to clear filter and show all items again

**Option 2: Quick-nav character filter**
1. Intercept letter input (via T9) on the ListView itself
2. Jump to first item starting with that letter (like macOS Finder)
3. Accumulate letters for more precise jumps (e.g., "go" jumps to items starting with "go")

**Implementation scope (~1 hour):**
1. Add `setSearchCallback()` method to `ListView` (`components/cdc_views/include/cdc_views/ListView.h`)
2. Modify `rebuildList()` in each module to filter based on current search query
3. Add T9InputView integration with callback to update filter
4. Add visual indicator when filter is active (e.g., "(searching)" in footer)

**Files to modify:**
- `components/cdc_views/include/cdc_views/ListView.h` - Add search API
- `components/cdc_views/src/ListView.cpp` - Implement search filtering
- `components/mod_password/src/PasswordModule.cpp` - Integrate search for passwords
- `components/mod_fido2/src/Fido2Ui.cpp` - Integrate search for FIDO2 credentials

## References

- ESP32-S3 T9 keypad input patterns: Classic phone-style text entry is well-suited for 12-button hardware keypads
- Embedded UI search patterns: https://www.nngroup.com/articles/search-mobile/
- T9 text entry: https://en.wikipedia.org/wiki/T9_(text_input)
