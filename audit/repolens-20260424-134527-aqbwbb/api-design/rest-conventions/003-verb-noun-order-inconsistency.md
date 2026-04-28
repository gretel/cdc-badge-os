---
title: "[MEDIUM] Inconsistent verb-noun ordering in command names"
severity: MEDIUM
domain: api-design
lens: serial-command-api
labels:
  - "audit:api-design/rest-conventions"
---

## Summary
The command naming convention mixes two patterns for verb-noun ordering:

**Pattern A: VERB_NOUN (action first)**
- `GET_TIME`, `SET_TIME`, `GET_DATE`, `SET_DATE`
- `SET_NAME`, `SET_INFO`, `SET_INFO2`
- `NVS_LIST`, `NVS_READ`, `NVS_DEL`, `NVS_CLEAR`
- `TR01_STATUS`, `TR01_INFO`, `TR01_SESSION`

**Pattern B: NOUN_VERB (resource first)**
- `PIN_STATUS`, `PIN_RESET`
- `PASSWORD_LIST`, `PASSWORD_GET`, `PASSWORD_ADD`, `PASSWORD_DEL`
- `TOTP_LIST`, `TOTP_ADD`, `TOTP_DEL`, `TOTP_GET`
- `GPG_STATUS`, `GPG_GENERATE`, `GPG_EXPORT`, `GPG_RESET`
- `VCARD_SET`, `VCARD_GET`, `VCARD_DELETE`

**Files**:
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/serial_cmd/src/SerialCmd.cpp` (built-in commands)
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_*/src/*Module.cpp` (module commands)

## Impact
**Cognitive load**: Users must remember which pattern each command group uses:
- Time commands: `SET_TIME` (verb first)
- PIN commands: `PIN_STATUS` (noun first) - inconsistent with time!
- Module commands: `PASSWORD_ADD` (noun first)
- Display commands: `SET_NAME` (verb first)

**Searchability**: When typing, users might try `TIME_GET` instead of `GET_TIME` or vice versa.

**REST parallel**: REST API endpoints use resource-first naming (`/users`, `/orders`) with HTTP verbs providing the action. This API uses both approaches inconsistently.

## Evidence
```cpp
// Pattern A: VERB_NOUN (time, NVS, TROPIC01)
reg.registerCommand({"GET_TIME", "Show current time", cmdGetTime, "time", false});
reg.registerCommand({"SET_TIME", "Set time (HH:MM:SS)", cmdSetTime, "time", false});
reg.registerCommand({"NVS_LIST", "List NVS entries [namespace]", cmdNvsList, "nvs", false});
reg.registerCommand({"TR01_STATUS", "Show TR01 status", cmdTr01Status, "tr01", false});

// Pattern B: NOUN_VERB (PIN, modules)
reg.registerCommand({"PIN_STATUS", "Show PIN status", cmdPinStatus, "pin", false});
reg.registerCommand({"PASSWORD_LIST", "List password entries", cmd_password_list, CMD_MODULE, true});
reg.registerCommand({"GPG_STATUS", "Show GPG status", cmd_gpg_status, CMD_MODULE, true});
```

Notice the inconsistency:
- `GET_TIME` vs `PIN_STATUS` (both show status, different order)
- `TR01_STATUS` vs `GPG_STATUS` (both show status, different order)

## Recommended Fix
**Recommendation: Adopt NOUN_VERB (resource-first) pattern**

This aligns better with:
1. REST conventions (`/users/list`, `/users/get`)
2. Module command consistency (all modules use NOUN_VERB)
3. Scalability (easier to group by resource)

**Migration plan:**
```cpp
// Change time commands
GET_TIME → TIME_GET
SET_TIME → TIME_SET
GET_DATE → DATE_GET
SET_DATE → DATE_SET

// Change display commands
SET_NAME → NAME_SET
SET_INFO → INFO_SET
SET_INFO2 → INFO2_SET

// Change NVS commands (optional, more disruptive)
NVS_LIST → NVS_LIST (already noun-first in a way)
NVS_READ → NVS_READ
NVS_DEL → NVS_DELETE

// Keep PIN and module commands as-is (already noun-first)
```

**Alternative: Document both patterns with rationale**
- Built-in system commands: VERB_NOUN (action-oriented)
- Module commands: NOUN_VERB (resource-oriented)
- Document this distinction clearly

## References
- RESTful API design: Resource-oriented naming
- HTTP methods as verbs acting on resource nouns
- Command-line tool conventions (git uses noun-first: `git status`, `git add`)
