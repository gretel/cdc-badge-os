---
title: "[LOW] vCard serial commands exist in code but missing from SERIAL_COMMANDS.md"
severity: LOW
domain: api-design/api-documentation
lens: serial-command-api
labels:
  - "audit:api-design/api-documentation"
---

## Summary
The vCard module has 3 serial commands registered in code but they are not documented in the main command reference (`docs/SERIAL_COMMANDS.md`).

**Implemented commands (components/mod_vcard/src/VcardModule.cpp):**
- `VCARD_SET` - Set own vCard (multiline paste)
- `VCARD_GET` - Show own vCard
- `VCARD_DELETE` - Delete own vCard

**Documentation status:**
- Not listed in `docs/SERIAL_COMMANDS.md`
- Not mentioned in `docs/ble_vcard_protocol.md`

## Impact
- Users discovering the serial command interface will miss the vCard functionality
- Inconsistent documentation compared to other modules (TOTP, GPG, Password all documented)
- Developers adding new features may not know vCard commands exist

## Evidence
**Implementation (components/mod_vcard/src/VcardModule.cpp):**
```cpp
reg.registerCommand({"VCARD_SET",    "Set own vCard (multiline paste)", cmdVcardSet,    "vcard", false});
reg.registerCommand({"VCARD_GET",    "Show own vCard",                  cmdVcardGet,    "vcard", false});
reg.registerCommand({"VCARD_DELETE", "Delete own vCard",                cmdVcardDelete, "vcard", false});
```

**Missing from documentation:**
`docs/SERIAL_COMMANDS.md` contains sections for:
- TOTP Module
- Password Module
- GPG Module

But **no vCard Module section**.

## Recommended Fix
Add a vCard section to `docs/SERIAL_COMMANDS.md` after the GPG section:

```markdown
## vCard Module

| Command | Description |
|---------|-------------|
| `VCARD_SET` | Set own vCard (multiline paste mode) |
| `VCARD_GET` | Show own vCard |
| `VCARD_DELETE` | Delete own vCard |

**VCARD_SET Usage:**
1. Type `VCARD_SET` and press Enter
2. Paste vCard content line by line
3. Type `DONE` on a new line to finish
```

## References
- components/mod_vcard/src/VcardModule.cpp (command registration)
- docs/SERIAL_COMMANDS.md (main command reference)
- docs/ble_vcard_protocol.md (BLE protocol spec)
