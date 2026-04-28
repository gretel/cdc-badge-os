---
title: "[MEDIUM] Authentication flow and session timeout not clearly documented"
severity: MEDIUM
domain: api-design/api-documentation
lens: serial-command-api
labels:
  - "audit:api-design/api-documentation"
---

## Summary
The `docs/SERIAL_COMMANDS.md` mentions `[AUTH]` markers but does not explain:
1. The authentication session timeout behavior
2. What happens when authentication expires mid-operation
3. How to check current authentication status
4. The difference between PIN retries and session timeout

## Impact
- Users may be confused when commands suddenly require re-authentication
- Scripts may fail silently if session times out during batch operations
- No guidance on how to maintain authentication for long operations
- PIN lockout behavior not clearly distinguished from session timeout

## Evidence
**Current documentation (docs/SERIAL_COMMANDS.md:7-14):**
```markdown
Commands marked with `[AUTH]` require authentication when `FEATURE_SECURE_SERIAL` is enabled.

## Authentication

| Command | Description |
|---------|-------------|
| `AUTH <pin>` | Authenticate with PIN |
| `LOGOUT` | End authenticated session |
```

**Implementation details (components/serial_cmd/src/SerialCmd.cpp:25-26, 1344-1360):**
```cpp
static constexpr uint32_t AUTH_TIMEOUT_MS = 5 * 60 * 1000;  // 5 minutes

// In isAuthenticated():
if ((now - s_authTimestamp) > (AUTH_TIMEOUT_MS * 1000ULL)) {
    s_authenticated = false;
    LOG_I(TAG, "Session timed out");
    return false;
}
```

**Missing from docs:**
- Session timeout duration (5 minutes)
- Timeout resets on each command execution
- How to check if currently authenticated (no STATUS command shows this)
- What triggers session timeout vs PIN lockout

## Recommended Fix
Expand the Authentication section in `docs/SERIAL_COMMANDS.md`:

```markdown
## Authentication

Commands marked with `[AUTH]` require authentication when `FEATURE_SECURE_SERIAL` is enabled.

### Session Management

| Command | Description |
|---------|-------------|
| `AUTH <pin>` | Authenticate with PIN (starts session) |
| `LOGOUT` | End authenticated session |

### Session Timeout
- **Timeout duration:** 5 minutes of inactivity
- **Auto-reset:** Session resets on each successful command
- **Expiration:** After 5 minutes without commands, re-authenticate with `AUTH <pin>`

### PIN vs Session
- **PIN lockout:** After N wrong AUTH attempts, PIN locks (temporary or permanent)
- **Session timeout:** After 5 minutes of inactivity, session expires (just re-authenticate)

### Check Authentication Status
Run any authenticated command to verify session is active. If you get:
```
ERROR: Not authenticated. Use AUTH <pin> to login.
```
Simply run `AUTH <pin>` again.
```

## References
- components/serial_cmd/include/serial_cmd/SerialCmd.h:26 (AUTH_TIMEOUT_MS)
- components/serial_cmd/src/SerialCmd.cpp:1344-1360 (isAuthenticated implementation)
- docs/SERIAL_COMMANDS.md:7-14 (current auth docs)
