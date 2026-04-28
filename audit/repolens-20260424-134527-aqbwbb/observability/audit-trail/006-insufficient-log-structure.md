---
title: "[MEDIUM] Missing timestamp and context in audit log entries"
severity: MEDIUM
domain: logging-infrastructure
lens: audit-trail
labels:
  - audit:observability/audit-trail
---

## Summary
The existing logging system (`components/cdc_log/include/cdc_log.h`) provides basic log output but lacks structured audit fields. Current logs do not include:
- Timestamp with timezone/uptime reference
- Source IP or interface (for networked operations)
- Session identifier
- Actor identification (which user/session performed the action)
- Machine-readable event type

**Evidence:**
```cpp
// cdc_log.h:129-133 - Log macros
#define LOG_E(tag, fmt, ...) log_write(CDC_LOG_LEVEL_ERROR,   tag, fmt, ##__VA_ARGS__)
#define LOG_W(tag, fmt, ...) log_write(CDC_LOG_LEVEL_WARN,    tag, fmt, ##__VA_ARGS__)
#define LOG_I(tag, fmt, ...) log_write(CDC_LOG_LEVEL_INFO,    tag, fmt, ##__VA_ARGS__)
#define LOG_D(tag, fmt, ...) log_write(CDC_LOG_LEVEL_DEBUG,   tag, fmt, ##__VA_ARGS__)

// cdc_log.cpp - Log output format (simplified)
void log_write(log_level_t level, const char* tag, const char* fmt, ...) {
    // Output: "[LEVEL] [TAG] message"
    // No timestamp, no session ID, no actor, no event type!
}
```

**Example log output:**
```
[INFO] [PinManager] Badge PIN verified
[INFO] [FIDO2] Created P-256 credential in slot 5
[INFO] [PASSWORD] Failed to write slot 150
```

These logs lack:
- When (exact timestamp)
- Who (which session/user)
- Where (serial, USB, BLE?)
- What event type (machine-readable)

## Impact
- **Forensic analysis**: Cannot correlate events across time without timestamps
- **Querying**: Free-text logs are hard to query programmatically
- **Compliance**: Audit logs typically require structured fields (timestamp, actor, action, target)
- **Correlation**: Cannot link related events (e.g., auth attempt followed by command execution)

## Evidence
Current log output format from `cdc_log.cpp`:
```
// Output format: [LEVEL] [TAG] message
// Example: [INFO] [PinManager] Badge PIN verified
// Missing: timestamp, session_id, actor, source, event_type
```

## Recommended Fix
1. Extend log format in `components/cdc_log/include/cdc_log.h`:
   ```cpp
   // Add timestamp to log entry
   typedef struct {
       uint32_t timestamp_ms;      // Boot time or RTC time
       log_level_t level;
       const char* tag;
       const char* event_type;     // Machine-readable (e.g., "PIN_VERIFY", "CRED_CREATE")
       const char* actor;          // Session ID or "SYSTEM"
       const char* source;         // "SERIAL", "USB", "BLE", "UI"
       char message[128];          // Human-readable
   } audit_entry_t;
   ```

2. Add audit-specific macros:
   ```cpp
   #define AUDIT_E(event_type, actor, source, fmt, ...) \
       audit_write(CDC_LOG_LEVEL_ERROR, event_type, actor, source, fmt, ##__VA_ARGS__)
   #define AUDIT_I(event_type, actor, source, fmt, ...) \
       audit_write(CDC_LOG_LEVEL_INFO, event_type, actor, source, fmt, ##__VA_ARGS__)
   ```

3. Add session tracking:
   ```cpp
   // components/serial_cmd/include/serial_cmd/Session.h
   struct Session {
       char id[16];          // Unique session ID
       const char* source;   // "SERIAL", "USB", "BLE"
       uint64_t start_time;
       bool authenticated;
   };
   ```

4. Update log output to include structured fields:
   ```
   // New format:
   // [TIMESTAMP=1234567890] [LEVEL=INFO] [EVENT=PIN_VERIFY] [ACTOR=session_abc] [SRC=SERIAL] Badge PIN verified
   ```

5. Create audit log storage:
   - Append-only ring buffer in NVS (last 100 entries)
   - Export command: `AUDIT_DUMP` to output all entries

## References
- RFC 5424: The Syslog Protocol (structured data)
- NIST SP 800-92: Guide to Computer Security Log Management
- OWASP Logging Cheat Sheet
