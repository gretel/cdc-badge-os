---
title: "[MEDIUM] Zero-results state lacks actionable suggestions for users"
severity: MEDIUM
domain: search-ux
lens: information-architecture
labels:
  - "audit:information-architecture/search-ux"
---

## Summary

When lists are empty (e.g., no FIDO2 credentials, no passwords), the UI shows a basic "No entries" placeholder but provides no guidance on what users should do next. This affects:

1. **FIDO2 Module** (`components/mod_fido2/src/Fido2Ui.cpp:125-135`) - Shows "No entries" with only a BACK hint
2. **Password Module** - Shows "New Entry" as first item but no contextual help

For a future search feature, when a filter returns zero results, there is no:
- Helpful message explaining why results might be empty
- Suggestions to broaden the search (e.g., "try fewer letters", "check spelling")
- Quick action to clear filters and show all items
- Link to add new entries if the list is empty

## Impact

**User Experience Impact:**
- Users seeing "No entries" may think something is broken rather than understanding they need to add data
- No clear call-to-action on how to populate the list
- When search is added, zero-results will feel like a dead end without suggestions

**Onboarding Gap:**
- First-time users may not discover how to add credentials/passwords
- No contextual help explaining what the list is for

**Future Search UX:**
- When search/filter is implemented (see finding #001), zero-results will need proper handling
- Without suggestions, users may abandon the search flow

## Evidence

**FIDO2 Zero-State** (`components/mod_fido2/src/Fido2Ui.cpp:125-135`):
```cpp
if (count == 0) {
    // Show "No entries" placeholder
    s_listItems[0].label = mstr(STR_NO_ENTRIES);
    s_listItems[0].userData = nullptr;
    s_listItems[0].icon = 0;
    s_listItems[0].iconDisabled = true;
    if (s_listView) {
        s_listView->init(mstr(STR_WEB_AUTHN), s_listItems, 1);
        s_listView->setHint(ui::tr(ui::StringId::HINT_BACK));
    }
    return;
}
```

**Current i18n String** (`components/mod_fido2/src/Fido2Ui.cpp:71`):
```cpp
i18n.registerTranslation(s_strIdBase + STR_NO_ENTRIES, ui::Language::EN, "No entries");
i18n.registerTranslation(s_strIdBase + STR_NO_ENTRIES, ui::Language::DE, "Keine Eintraege");
```

**Password Module** (`components/mod_password/src/PasswordModule.cpp:408-434`):
- Shows "New Entry" as first item (better than FIDO2)
- But no additional context or help text

## Recommended Fix

**Short-term (30 min):** Add actionable zero-state messaging:

1. **FIDO2 Module** - Enhance zero-state with helper text:
   ```cpp
   // Instead of just "No entries"
   s_listItems[0].label = mstr(STR_NO_ENTRIES);
   s_listItems[1].label = "-> Press [Y] to add first credential";
   s_listItems[1].iconDisabled = true;
   s_listView->init(mstr(STR_WEB_AUTHN), s_listItems, 2);
   ```

2. **Add helper string** to i18n:
   ```cpp
   i18n.registerTranslation(s_strIdBase + STR_ADD_FIRST, ui::Language::EN, "Press [Y] to add first credential");
   i18n.registerTranslation(s_strIdBase + STR_ADD_FIRST, ui::Language::DE, "[Y] druecken fuer ersten Eintrag");
   ```

**For future search implementation** (when search is added per finding #001):

3. **Zero-results with search active** should show:
   - "No matches for 'xyz'"
   - "Try fewer letters or check spelling"
   - "Press [N] to clear search"
   - "Press [Y] to add new entry"

4. **Add i18n strings for search zero-state**:
   ```cpp
   STR_NO_MATCHES      // "No matches for '%s'"
   STR_CLEAR_SEARCH    // "Clear search"
   STR_SEARCH_HINT     // "Try fewer letters"
   ```

**Files to modify:**
- `components/mod_fido2/src/Fido2Ui.cpp` - Enhance zero-state display
- `components/mod_fido2/src/Fido2Ui.cpp` - Add STR_NO_ENTRIES helper
- `components/cdc_ui/include/cdc_ui/I18n.h` - Add zero-state string IDs (for future search)

## References

- Empty state UX patterns: https://www.nngroup.com/articles/empty-states/
- Zero-results search design: https://www.nngroup.com/articles/search-zero-results/
- Call-to-action in empty lists: https://mobiforge.com/design-patterns/empty-states

</content>