---
title: "[LOW] Ambiguous STATUS command without resource qualifier"
severity: LOW
domain: api-design
lens: serial-command-api
labels:
  - "audit:api-design/rest-conventions"
---

## Summary
The `STATUS` command exists at multiple levels without clear differentiation:
- `STATUS` - System-wide status (in `system` group)
- `TR01_STATUS` - TROPIC01 secure element status (in `tr01` group)
- `GPG_STATUS` - GPG module status (in `gpg` group)

While the grouped output in `HELP` shows them under different modules, the naming pattern is inconsistent:
- Generic `STATUS` vs qualified `TR01_STATUS`, `GPG_STATUS`

**Files**:
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/serial_cmd/src/SerialCmd.cpp:1441`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_gpg/src/GpgModule.cpp:114`

## Impact
**Discoverability**: Users might expect `STATUS` to show all statuses (system + modules), but it only shows system-level info.

**Predictability**: Should there be a `PIN_STATUS`? `TOTP_STATUS`? `PASSWORD_STATUS`? Currently only `PIN_STATUS` exists (under `pin` group).

**Evidence**:
```cpp
// System-level STATUS (generic)
reg.registerCommand({"STATUS", "Show system status", cmdStatus, "system", false});

// Module-specific STATUS (qualified)
reg.registerCommand({"TR01_STATUS", "Show TR01 status", cmdTr01Status, "tr01", false});
registry.registerCommand({"GPG_STATUS", "Show GPG status", cmd_gpg_status, CMD_MODULE, true});
reg.registerCommand({"PIN_STATUS", "Show PIN status", cmdPinStatus, "pin", false});
```

Output of `STATUS`:
```
=== System Status ===
Free heap: xxx bytes
Min free heap: xxx bytes
Uptime: xxx ms
```

Output of `TR01_STATUS`:
```
TR01 Status:
  Session: active/inactive
```

## Recommended Fix
**Option 1: Make generic STATUS aggregate all statuses**
```cpp
// STATUS shows summary of all subsystems
// Add optional argument for specific status: STATUS [system|tr01|gpg|pin]
```

**Option 2: Rename generic STATUS to SYSTEM_STATUS for consistency**
```cpp
reg.registerCommand({"SYSTEM_STATUS", "Show system status", cmdStatus, "system", false});
```

**Option 3: Keep as-is but document the distinction**
- `STATUS` = system resources (heap, uptime)
- `*_STATUS` = module-specific state

Currently Option 3 is in use. Document this pattern in API documentation.

## References
- Unix `status` vs `systemctl status`
- REST: `/status` (health check) vs `/resources/{id}/status`
