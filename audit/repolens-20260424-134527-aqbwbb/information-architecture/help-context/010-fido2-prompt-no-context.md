---
title: "[MEDIUM] FIDO2 user-presence prompt lacks contextual explanation for Relying Party"
severity: MEDIUM
domain: information-architecture
lens: help-context
labels:
  - "audit:information-architecture/help-context"
---

## Summary
The FIDO2 user-presence prompt (in `components/mod_fido2/src/Fido2Ui.cpp:540-565`) displays the Relying Party ID (`s_promptRpId`) but provides no explanation of what a Relying Party is, what the user is approving, or what the implications of approval are. The prompt text only shows:
- A headline (e.g., "Register Key" or "Sign In")
- The Relying Party ID (e.g., "github.com" or "example.com")
- A footer hint "[Y] Approve  [N] Deny"

The user is presented with a technical identifier without any context about what action they're confirming.

**Evidence** (`components/mod_fido2/src/Fido2Ui.cpp:540-554`):
```cpp
static char prompt_text[200];
if (action == FIDO2_ACTION_SELECT) {
    snprintf(prompt_text, sizeof(prompt_text),
             "%s\n\n%s",
             headline,
             ui::tr(ui::StringId::HINT_APPROVE_DENY));
} else {
    snprintf(prompt_text, sizeof(prompt_text),
             "%s\n\n%s\n\n%s",
             headline,
             s_promptRpId,  // Just the RP ID, no explanation
             ui::tr(ui::StrId::HINT_APPROVE_DENY));
}
```

The `s_promptRpId` is displayed as raw text with no additional context.

## Impact
Users may approve FIDO2 operations without understanding:
1. What a "Relying Party" is
2. Whether they're registering a new key or authenticating
3. What website/service they're connecting to
4. What permissions or access they're granting

This reduces the security value of the user-presence check because users cannot make informed decisions.

## Evidence
- File: `components/mod_fido2/src/Fido2Ui.cpp`
- Lines: 540-565 (prompt construction and display)
- The prompt shows RP ID but no explanatory text
- No info icon, helper text, or expandable details available

## Recommended Fix
Add a brief explanatory line to the FIDO2 prompt that clarifies what the user is approving:

**Option 1 - Inline explanation:**
```cpp
// For registration:
snprintf(prompt_text, sizeof(prompt_text),
         "%s for:\n%s\n\n(Relying Party = website/service)\n\n%s",
         headline,
         s_promptRpId,
         ui::tr(ui::StringId::HINT_APPROVE_DENY));

// For authentication:
snprintf(prompt_text, sizeof(prompt_text),
         "Sign in to:\n%s\n\n(Confirm you own this badge)\n\n%s",
         s_promptRpId,
         ui::tr(ui::StringId::HINT_APPROVE_DENY));
```

**Option 2 - Add a "Details" or "Info" button:**
Add a third key option (e.g., key '3' or 'I') that shows additional context about FIDO2 and Relying Parties when pressed.

## References
- FIDO2 WebAuthn specification: https://www.w3.org/TR/webauthn-2/
- User presence verification best practices: https://fidoalliance.org/overview/
