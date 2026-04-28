---
title: "[LOW] Footer hint strings use inconsistent action patterns across modules"
severity: LOW
domain: ui
lens: dark-patterns
labels:
  - "audit:ux-antipatterns/dark-patterns"
---

## Summary

Footer hint strings across different modules use inconsistent action patterns, making it harder for users to learn the navigation model. Some hints use Y/N for actions, others use Y/3/N, and the action verbs vary in formality.

**Evidence:**

File: `components/mod_password/src/PasswordModule.cpp`, lines 89, 91

```cpp
i18n.registerTranslation(s_strIdBase + STR_HINT_LIST, ui::Language::EN, "[Y] View  [3] Menu  [N] Back");
i18n.registerTranslation(s_strIdBase + STR_HINT_TYPE, ui::Language::EN, "[Y] Type  [2/8] Scroll  [N] Back");
```

File: `components/cdc_ui/src/I18n.cpp`, lines 352-363

```cpp
REG(HINT_BACK,          "[N] Back",             "[N] Zuruck");
REG(HINT_SELECT,        "[Y] Select",           "[Y] Auswahlen");
REG(HINT_OK_BACK,       "[Y] OK [N] Back",      "[Y] OK [N] Zuruck");
REG(HINT_SCROLL_BACK,   "[2/8] Scroll [N] Back","[2/8] Scrollen [N] Zuruck");
```

**Inconsistencies identified:**

1. **Y/N vs Y/3/N patterns**:
   - Password module: `[Y] View  [3] Menu  [N] Back`
   - Core HINT_OK_BACK: `[Y] OK [N] Back`
   - Some views only use `[N] Back` without Y

2. **Action verb inconsistency**:
   - "View" (Password module) vs "Select" (core) vs "OK" (core)
   - "Type" (Password module) vs "Adjust" (brightness) vs "Scroll" (core)

3. **Missing standardization**:
   - No consistent pattern for three-action hints (Y/3/N vs Y/2/N)
   - Some hints include key numbers, others don't

4. **German translation inconsistency**:
   - "Ansehen" (View) vs "Auswahlen" (Select) - different formality levels
   - "Zuruck" vs "Back" - English/German asymmetry in some hints

## Impact

1. **Learning curve**: Users must relearn navigation for each module
2. **Memory load**: Inconsistent patterns increase cognitive burden
3. **Discoverability**: Users may not find the "3" key for context menus if they expect different patterns
4. **Professional appearance**: Inconsistency feels less polished

## Recommended Fix

Standardize on a consistent pattern across all modules:

**Option 1: Universal 3-action pattern**

```cpp
// Core hints (components/cdc_ui/src/I18n.cpp)
REG(HINT_SELECT_MENU,   "[Y] Select  [3] Menu  [N] Back",  "[Y] Auswahlen  [3] Menu  [N] Zuruck");
REG(HINT_VIEW_MENU,     "[Y] View    [3] Menu  [N] Back",  "[Y] Ansehen    [3] Menu  [N] Zuruck");
REG(HINT_SCROLL_MENU,   "[Y] Select  [3] Menu  [N] Back",  "[Y] Auswahlen  [3] Menu  [N] Zuruck");

// Update password module to use consistent pattern
i18n.registerTranslation(s_strIdBase + STR_HINT_LIST, ui::Language::EN, "[Y] View  [3] Menu  [N] Back");
i18n.registerTranslation(s_strIdBase + STR_HINT_TYPE, ui::Language::EN, "[Y] Select  [3] Menu  [N] Back");
```

**Option 2: Simplify to 2-action pattern**

```cpp
// Most common: Y for primary action, N for back
REG(HINT_SELECT_BACK,   "[Y] Select  [N] Back",  "[Y] Auswahlen  [N] Zuruck");
REG(HINT_VIEW_BACK,     "[Y] View    [N] Back",  "[Y] Ansehen    [N] Zuruck");

// Context menu on separate key (always 3)
// Don't mention in hint, discover through exploration
```

**Option 3: Use consistent action verbs**

```cpp
// Use "Select" for all selection actions
// Use "View" for all detail-view actions
// Use "Menu" for all context-menu actions
```

## References

- Nielsen Norman Group: [Consistency](https://www.nngroup.com/articles/consistency-and-familiarity/) - consistent patterns reduce learning time
- Material Design: [Navigation patterns](https://material.io/design/navigation/understanding-navigation.html) - predictable interaction models
- HCI fundamentals: [Mental models](https://www.nngroup.com/articles/mental-models/) - users build expectations from patterns

</content>