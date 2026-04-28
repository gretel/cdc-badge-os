---
title: "[MEDIUM] FIDO2 credential list supports 32 items without search/filter"
severity: MEDIUM
domain: UI/UX
lens: cognitive-overload
labels:
  - choice-paralysis
  - flat-list
---

## Summary

The FIDO2 credential list (`Fido2Ui.cpp:138-169`) supports up to 32 credentials (`FIDO2_MAX_CREDENTIALS = 32`) displayed in a flat, sorted list without any search, filtering, or pagination functionality. Users must scroll through all entries to find a specific credential.

**Files:**
- `components/mod_fido2/src/Fido2Ui.cpp:138-169` (credential list rendering)
- `components/mod_fido2/include/mod_fido2/fido2.h:18` (FIDO2_MAX_CREDENTIALS definition)

## Impact

**User Experience:** With up to 32 credentials, users face significant cognitive load when searching for a specific relying party. The list is alphabetically sorted but lacks any search or filtering mechanism.

**Evidence:**
From `Fido2Ui.cpp:138-169`:
```cpp
for (uint8_t i = 0; i < count && i < FIDO2_MAX_CREDENTIALS; i++) {
    s_sortMap[i] = i;
    fido2_credential_info_t info = {};
    if (fido2_get_credential_info(i, &info)) {
        if (strlen(info.user_name) > 0) {
            snprintf(s_labels[i], sizeof(s_labels[i]),
                     "%.45s (%.45s)", info.rp_id, info.user_name);
        } else {
            snprintf(s_labels[i], sizeof(s_labels[i]),
                     "%.45s", info.rp_id);
        }
    }
}

std::sort(s_sortMap, s_sortMap + count, [](uint8_t a, uint8_t b) {
    return strcasecmp_safe(s_labels[a], s_labels[b]) < 0;
});
```

The list is sorted alphabetically but has:
- No search input
- No filter by RP domain
- No grouping by domain
- No pagination

With 32 items and ~4 visible per screen, users may need 6-8 scrolls to reach the end.

## Recommended Fix

Implement one of these approaches:

1. **Add T9 search** - Reuse `T9InputView` to filter list as user types (most efficient for small display)
2. **Add quick-scroll index** - Show A-Z sidebar for faster navigation
3. **Group by domain** - Display domain groups with expand/collapse
4. **Add "Recent" section** - Show most recently used credentials at top

Quick fix (Option 1):
```cpp
// Add search mode toggle (e.g., long-press Y or menu key)
// When in search mode, show T9InputView with filtered results
static void showSearchMode() {
    s_t9Input.init("Search...", nullptr, 32);
    s_t9Input.setOnSave([](const char* text) {
        filterListByText(text);
        rebuildList();
    });
    ViewStack::instance().push(&s_t9Input);
}
```

## References

- Nielsen Norman Group: "Search vs. Browse"
- Material Design: "Lists - Searchable lists"
- Cognitive Load Theory: Reduce working memory demands for large datasets
