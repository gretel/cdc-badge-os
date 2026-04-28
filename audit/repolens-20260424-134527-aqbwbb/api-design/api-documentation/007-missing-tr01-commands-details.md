---
title: "[MEDIUM] TROPIC01 commands lack parameter format details"
severity: MEDIUM
domain: api-design/api-documentation
lens: serial-command-api
labels:
  - "audit:api-design/api-documentation"
---

## Summary
The `docs/SERIAL_COMMANDS.md` lists TROPIC01 commands but does not specify:
1. Valid slot number ranges for each command
2. The exact output format of `TR01_RMEM_READ`
3. What constitutes a "mismatched slot" for `TR01_CLEANUP`
4. Confirmation requirements for destructive operations

## Impact
- Users may attempt to access invalid slot ranges
- Hard to parse programmatic output from R-Memory read
- Unclear when to use CLEANUP vs CACHE_REBUILD vs RESYNC
- Risk of accidental data loss without clear warnings

## Evidence
**Documentation (docs/SERIAL_COMMANDS.md:68-82):**
```markdown
## TROPIC01 Secure Element

| Command | Description |
|---------|-------------|
| `TR01_STATUS` | Show TR01 connection status |
| `TR01_INFO` | Show TR01 chip info (ID, firmware) |
| `TR01_SESSION` | Start/restart TR01 session |
| `TR01_SLOTS` | Show slot usage summary |
| `TR01_RMEM_READ <slot>` | Read and dump R-Memory slot |
| `TR01_ECC_DEL <slot>` | Delete ECC key slot `[AUTH]` |
| `TR01_RMEM_DEL <slot>` | Delete R-Memory slot `[AUTH]` |
| `TR01_RESYNC` | Resync TR01 session and cache |
| `TR01_CACHE_REBUILD` | Rebuild TR01 cache from chip |
| `TR01_CLEANUP` | Cleanup mismatched slots + rebuild cache `[AUTH]` |
| `TR01_WIPE CONFIRM` | Factory reset all TR01 data `[AUTH]` |
```

**Missing details:**
- Slot ranges: ECC (0-31), R-Memory (0-511)
- Output format for TR01_RMEM_READ (hex dump)
- When to use RESYNC vs CACHE_REBUILD vs CLEANUP
- What "mismatched" means for CLEANUP

**Implementation (components/serial_cmd/src/SerialCmd.cpp:1207-1208, 1369-1380):**
```cpp
static constexpr size_t MAX_COMMANDS = 64;

// Slot validation:
if (slotVal >= maxSlot) {
    Console::printf("ERROR: Invalid %s (0-%d)\r\n", slotTypeName, maxSlot - 1);
}
```

## Recommended Fix
Enhance TROPIC01 documentation:

```markdown
## TROPIC01 Secure Element

### Slot Ranges
- **ECC Key Slots:** 0-31 (32 total)
  - Slot 0: Attestation key (system)
  - Slots 1-3: GPG keys
  - Slots 4: CA key
  - Slots 5-31: FIDO2 keys
- **R-Memory Slots:** 0-511 (512 total)
  - Slot 0: System PIN/lockout
  - Slots 1-31: Paired with ECC slots
  - Slots 32-131: TOTP accounts (100)
  - Slots 132-511: Password vault

### Commands

| Command | Description |
|---------|-------------|
| `TR01_STATUS` | Show TR01 connection status |
| `TR01_INFO` | Show TR01 chip info (ID, firmware) |
| `TR01_SESSION` | Start/restart TR01 session |
| `TR01_SLOTS` | Show slot usage summary |
| `TR01_RMEM_READ <slot>` | Read R-Memory slot (hex dump) |
| `TR01_ECC_DEL <slot>` | Delete ECC key slot `[AUTH]` |
| `TR01_RMEM_DEL <slot>` | Delete R-Memory slot `[AUTH]` |
| `TR01_RESYNC` | Resync session (invalidate cache) |
| `TR01_CACHE_REBUILD` | Rebuild cache from chip (debug) |
| `TR01_CLEANUP` | Fix mismatched slots + rebuild `[AUTH]` |
| `TR01_WIPE CONFIRM` | Factory reset all data `[AUTH]` |

### When to use each recovery command
- **RESYNC:** Session lost or stale (fastest)
- **CACHE_REBUILD:** Rebuild from chip after external changes
- **CLEANUP:** Slots have corrupted metadata or mismatched ECC/R-Memory
```

## References
- docs/SERIAL_COMMANDS.md:68-82 (current docs)
- components/serial_cmd/src/SerialCmd.cpp:1207-1380 (implementation)
- docs/plans/2026-01-28-tropic-storage-design.md (slot allocation design)
