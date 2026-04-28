---
title: "[MEDIUM] Missing Troubleshooting Guide for Common Operational Issues"
severity: MEDIUM
domain: operational-docs
lens: documentation/operational-docs
labels:
  - "audit:documentation/operational-docs"
---

## Summary
The CDC Badge OS documentation lacks a dedicated troubleshooting guide. While individual components (e.g., GPG module) have small troubleshooting sections, there is no centralized, comprehensive guide for operators to diagnose and recover from common failure modes.

**Where it should be:** New file `docs/TROUBLESHOOTING.md`

**Current state:**
- GPG.md has a minimal troubleshooting section (lines 186-204)
- MODULE_DEVELOPMENT.md has a troubleshooting section for module developers (lines 683-700)
- No general troubleshooting for end users or operators

## Impact
**Operational Impact:**
- When the device fails, users must piece together solutions from scattered documentation
- Common issues (boot loops, USB disconnects, TROPIC01 errors, PIN lockouts) have no documented diagnostic procedures
- On-call engineers or users must reverse-engineer recovery steps
- Increases mean-time-to-recovery (MTTR) for common issues

**Evidence:**
- `docs/GPG.md:186-204` - Only covers GPG-specific issues
- `docs/MODULE_DEVELOPMENT.md:683-700` - Only for module developers
- `docs/SERIAL_COMMANDS.md` - Lists `ERROR_LOG` command but no guidance on interpreting results
- `tools/coredump.py` exists but no documentation on when/how to use it

## Recommended Fix
Create `docs/TROUBLESHOOTING.md` with the following sections:

1. **Device Won't Boot**
   - Symptoms: No USB enumeration, display blank
   - Steps: Check USB power, try BOOT+RESET sequence, connect serial monitor

2. **USB Connection Issues**
   - Symptoms: Device detected but no CDC serial, intermittent disconnects
   - Steps: Check `pio device monitor` connection, verify port permissions

3. **TROPIC01 Secure Element Errors**
   - Symptoms: `TR01_STATUS` fails, slot commands return errors
   - Steps: Run `TR01_SESSION`, `TR01_RESYNC`, `TR01_CACHE_REBUILD`

4. **PIN Lockout Recovery**
   - Symptoms: 3 failed PIN attempts, device locked for 60s
   - Steps: Wait for timeout, check `PIN_STATUS`, use `PIN_RESET` (debug only)

5. **Memory Issues**
   - Symptoms: `ESP_ERR_NO_MEM`, random crashes
   - Steps: Run `MEM` command, check PSRAM usage, reduce module count

6. **Core Dump Analysis**
   - When to use `tools/coredump.py`
   - How to interpret output
   - Common panic causes

7. **Factory Reset Procedures**
   - Soft reset: `NVS_CLEAR` + `TR01_CLEANUP`
   - Hard reset: `TR01_WIPE CONFIRM` (destructive)
   - Re-flash procedure with `flash_firmware.py`

8. **Common Error Log Messages**
   - Table of common ERROR_LOG entries and their meanings
   - Reference `ERROR_LOG` command usage

## References
- ESP32-S3 technical reference manual (reset procedures)
- `tools/coredump.py` - Core dump analysis tool
- `tools/flash_firmware.py` - Firmware flashing tool
- `docs/SERIAL_COMMANDS.md` - Available diagnostic commands
- `components/cdc_log/` - Error log implementation

---
**Related Issues:**
- Missing backup/restore procedures (separate finding)
- Missing incident response runbook (separate finding)
