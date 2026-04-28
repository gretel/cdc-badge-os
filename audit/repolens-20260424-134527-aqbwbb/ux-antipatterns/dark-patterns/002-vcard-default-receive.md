---
title: "[MEDIUM] vCard module enables receiving by default without explicit user consent"
severity: MEDIUM
domain: ui
lens: dark-patterns
labels:
  - "audit:ux-antipatterns/dark-patterns"
---

## Summary
The vCard module automatically enables incoming vCard reception when the module initializes, without requiring explicit user consent. This is a **Pre-Checked Opt-In Box** pattern where the default state favors the business/feature over user choice.

**Evidence:**
- File: `components/mod_vcard/src/VcardModule.cpp`, line 472-473
- Code:
```cpp
// Enable receiving by default
ble_vcard_set_receive_enabled(true);
```

- File: `components/mod_vcard/src/ble_vcard.cpp`, line 97
- Initial state:
```cpp
static bool s_receive_enabled = false;
```

The variable is initialized to `false` but then immediately set to `true` in the module's `init()` function without any user interaction.

## Impact
1. **User autonomy**: Users may not know that their badge will automatically accept incoming vCards until they discover the setting in the menu
2. **Privacy implications**: vCard exchange can reveal contact information to nearby devices; default-on means users are "opted in" to this behavior without explicit consent
3. **Discovery friction**: Users who want to disable receiving must navigate to Tools → Modules → vCard and toggle it off, rather than being asked during initial setup

## Recommended Fix
Change the default to disabled (`false`) and let users explicitly enable receiving:

```cpp
// In components/mod_vcard/src/VcardModule.cpp, line 472-473
// Remove or change to false:
ble_vcard_set_receive_enabled(false);  // User must explicitly enable
```

Alternatively, show a one-time consent prompt on first module initialization:
```cpp
// Show consent dialog before enabling
showConfirm("Enable vCard receiving? Accept incoming vCard exchanges.",
    [](void*){ ble_vcard_set_receive_enabled(true); },
    [](void*){ ble_vcard_set_receive_enabled(false); },
    ConfirmView::Icon::QUESTION);
```

## References
- Dark Patterns Quick Guide: [Pre-Checked Opt-In Boxes](https://www.darkpatterns.org/types-of-dark-pattern#pre-checked-opt-in-boxes)
- GDPR Article 7: Conditions for consent (requires clear affirmative action)
- Material Design: [Preferences and settings](https://material.io/design/usability/preferences-and-settings.html) - suggests让用户 choose defaults

</content>