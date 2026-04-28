---
title: "[LOW] Missing Error Log Interpretation Guide"
severity: LOW
domain: operational-docs
lens: documentation/operational-docs
labels:
  - "audit:documentation/operational-docs"
---

## Summary
The error log system is documented as a command (`ERROR_LOG`) but there is no guide explaining what different error messages mean and how to interpret them for diagnosis. The error log stores WARNING and ERROR messages in a PSRAM-backed circular buffer (50 entries, 100 chars each), but users have no reference for common error patterns.

**Where it should be:** Section in `TROUBLESHOOTING.md` or new file `docs/ERROR_CODES.md`

**Current state:**
- `SERIAL_COMMANDS.md:24-25` - `ERROR_LOG` and `ERROR_LOG CLEAR` commands listed
- `components/cdc_log/include/cdc_log.h` - Error log implementation (50 entries, WARNING+ERROR levels)
- No documentation of common error patterns or their meanings
- No reference for module-specific error codes

## Impact
**Diagnostic Impact:**
- Error log output is opaque to users
- Cannot prioritize which errors to address first
- Missed opportunity for proactive maintenance (e.g., seeing repeated TROPIC01 warnings before failure)
- Debugging requires guessing rather than systematic analysis

**Evidence:**
- `components/cdc_log/include/cdc_log.h:100-110` - Error log structure defined
- `components/cdc_log/src/cdc_log.cpp` - Implementation with `error_log_get_entries()`, `error_log_dump()`
- `SERIAL_COMMANDS.md` - Only mentions command, no examples of output interpretation

## Recommended Fix
Create error log documentation with:

1. **Error Log Overview**
   - Capacity: 50 entries, 100 chars per entry
   - Storage: PSRAM-backed circular buffer
   - Log levels: WARNING, ERROR (not INFO, DEBUG)

2. **Common Error Messages by Category**
   
   **TROPIC01 Errors:**
   - `TR01_SESSION not initialized` → Run `TR01_SESSION`
   - `Slot X not found` → Check `TR01_SLOTS`, run `TR01_RESYNC`
   - `ECC key missing` → Module needs re-initialization
   
   **NVS Errors:**
   - `NVS not found` → NVS corruption, run `NVS_CLEAR`
   - `Key not found` → Module data missing, re-initialize module
   
   **Memory Errors:**
   - `ESP_ERR_NO_MEM` → PSRAM exhaustion, reduce modules or reboot
   
   **PIN Errors:**
   - `PIN retry count: 3` → Lockout imminent, verify PIN
   - `PIN locked` → Wait 60 seconds

3. **How to Use Error Log**
   - Command: `ERROR_LOG` to display
   - Command: `ERROR_LOG CLEAR` to reset
   - Command: `MEM` to check memory after NO_MEM errors

4. **Error Log Best Practices**
   - Check error log after any crash or unexpected behavior
   - Clear error log after resolving issues
   - Capture error log before factory reset (for debugging)

## References
- `components/cdc_log/include/cdc_log.h` - Error log API
- `docs/SERIAL_COMMANDS.md` - Command reference
- `docs/TROUBLESHOOTING.md` - Related diagnostic guide

---
**Related Issues:**
- Missing troubleshooting guide (#001)
