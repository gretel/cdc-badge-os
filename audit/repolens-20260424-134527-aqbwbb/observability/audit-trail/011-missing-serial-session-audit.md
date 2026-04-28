---
title: "[HIGH] Missing audit trail for serial session authentication"
severity: HIGH
domain: administration
lens: audit-trail
labels:
  - audit:observability/audit-trail
---

## Summary

Serial session authentication (via `AUTH <pin>` command) lacks structured audit logging. When `SerialCmd::authenticate()` is called, basic log messages are produced but no audit record captures:
- Successful authentication (who, when, session start)
- Failed authentication attempts (for forensic analysis of brute-force attacks)
- Session logout (session end)
- Session timeout (automatic logout)

**Files affected:**
- `components/serial_cmd/src/SerialCmd.cpp:1367` - `SerialCmd::authenticate()` - Authenticates session
- `components/serial_cmd/src/SerialCmd.cpp:1399` - `SerialCmd::logout()` - Logs out session
- `components/serial_cmd/src/SerialCmd.cpp:1347` - `SerialCmd::isAuthenticated()` - Checks session with timeout

## Impact

1. **Security investigations**: Cannot reconstruct when admin sessions were authenticated/logged out.
2. **Brute-force detection**: Failed authentication attempts are logged but not structured for analysis.
3. **Session tracking**: No audit trail for session duration, allowing silent session hijacking.
4. **Compliance**: Admin sessions typically require audit trails for compliance (e.g., SOC2, ISO 27001).
5. **Accountability**: Cannot distinguish between user-initiated and admin-initiated changes without session context.

## Evidence

### Authentication success logged but not audited

In `SerialCmd.cpp:1367-1394`:
```cpp
bool SerialCmd::authenticate(const char* pin) {
    auto& pm = core::PinManager::instance();

    if (pm.isBadgeBlocked()) {
        if (pm.isLockoutActive()) {
            uint32_t remainingSec = pm.getLockoutRemainingMs() / 1000;
            LOG_W(TAG, "PIN locked, %lu seconds remaining", (unsigned long)remainingSec);
        } else {
            LOG_W(TAG, "PIN permanently blocked (retries exhausted)");
        }
        return false;  // No audit event for blocked PIN!
    }

    if (!pin || !*pin) {
        LOG_W(TAG, "Empty PIN provided");
        return false;  // No audit event for empty PIN!
    }

    if (!pm.verifyBadgePin(pin)) {
        LOG_W(TAG, "Authentication failed, %d retries remaining", pm.getBadgeRetries());
        return false;  // No audit event for failed auth!
    }

    s_authenticated = true;
    s_authTimestamp = esp_timer_get_time();
    LOG_I(TAG, "Authenticated via serial");  // Basic log only, no audit structure
    return true;
}
```

### Logout logged but not audited

In `SerialCmd.cpp:1396-1403`:
```cpp
void SerialCmd::logout() {
    s_authenticated = false;
    s_authTimestamp = 0;
    LOG_I(TAG, "Logged out");  // Basic log only, no audit structure
}
```

### Session timeout silent

In `SerialCmd.cpp:1347-1358`:
```cpp
bool SerialCmd::isAuthenticated() {
    if (!s_authenticated) return false;

    // Check timeout (5 minutes)
    uint64_t now = esp_timer_get_time();
    if (now - s_authTimestamp > 5 * 60 * 1000 * 1000) {
        s_authenticated = false;  // Session timed out silently
        return false;
    }
    return true;
    // No audit event for session timeout!
}
```

### cmdAuth command lacks audit

In `SerialCmd.cpp:815-849`:
```cpp
static void cmdAuth(const char* args) {
    auto& pm = core::PinManager::instance();

    if (pm.isBadgeBlocked()) {
        if (pm.isLockoutActive()) {
            uint32_t remainingSec = pm.getLockoutRemainingMs() / 1000;
            Console::printf("ERROR: PIN locked. Wait %lu seconds.\r\n", (unsigned long)remainingSec);
        } else {
            Console::printf("ERROR: PIN permanently locked.\r\n");
        }
        return;  // No audit event!
    }

    if (!args || !*args) {
        Console::printf("Usage: AUTH <pin>\r\n");
        Console::printf("Retries: %d\r\n", pm.getBadgeRetries());
        return;  // No audit event!
    }

    if (SerialCmd::authenticate(args)) {
        Console::printf("OK: Authenticated\r\n");  // No audit event!
    } else {
        uint8_t retries = pm.getBadgeRetries();
        if (retries == 0) {
            if (pm.isLockoutActive()) {
                uint32_t remainingSec = pm.getLockoutRemainingMs() / 1000;
                Console::printf("ERROR: Wrong PIN. Locked for %lu seconds.\r\n", (unsigned long)remainingSec);
            } else {
                Console::printf("ERROR: Wrong PIN. Permanently locked.\r\n");
            }
        } else {
            Console::printf("ERROR: Wrong PIN. %d retries remaining.\r\n", retries);
        }
        // No audit event for failure!
    }
}
```

## Recommended Fix

### Step 1: Create audit helper for serial session (15 min)

Create `components/serial_cmd/include/serial_cmd/SerialCmdAudit.h`:

```cpp
#ifndef SERIAL_CMD_SERIAL_CMD_AUDIT_H
#define SERIAL_CMD_SERIAL_CMD_AUDIT_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Audit record for serial session authentication success.
 * \param source Source interface ("SERIAL", "USB", etc).
 */
void serial_audit_auth_success(const char* source);

/**
 * \brief Audit record for serial session authentication failure.
 * \param source Source interface.
 * \param reason Reason code (0=wrong PIN, 1=blocked, 2=empty PIN, 3=lockout).
 */
void serial_audit_auth_failure(const char* source, uint8_t reason);

/**
 * \brief Audit record for serial session logout.
 * \param source Source interface.
 * \param reason Reason (0=user logout, 1=timeout).
 */
void serial_audit_logout(const char* source, uint8_t reason);

#ifdef __cplusplus
}
#endif

#endif // SERIAL_CMD_SERIAL_CMD_AUDIT_H
```

### Step 2: Implement audit functions in `SerialCmd.cpp` (15 min)

Add to `components/serial_cmd/src/SerialCmd.cpp`:

```cpp
#include "serial_cmd/SerialCmdAudit.h"

static void serial_audit_auth_success(const char* source) {
    uint64_t now = esp_timer_get_time();
    LOG_I(TAG, "AUDIT: AUTH OK | source=%s | time=%lu | session_started",
          source, (unsigned long)(now / 1000));
}

static void serial_audit_auth_failure(const char* source, uint8_t reason) {
    uint64_t now = esp_timer_get_time();
    const char* reasons[] = {"wrong_pin", "blocked", "empty_pin", "lockout"};
    LOG_I(TAG, "AUDIT: AUTH FAIL | source=%s | time=%lu | reason=%s",
          source, (unsigned long)(now / 1000), reasons[reason < 4 ? reason : 0]);
}

static void serial_audit_logout(const char* source, uint8_t reason) {
    const char* reasons[] = {"user_logout", "timeout"};
    LOG_I(TAG, "AUDIT: LOGOUT | source=%s | reason=%s",
          source, reasons[reason < 2 ? reason : 0]);
}
```

### Step 3: Integrate audit calls into authenticate() (10 min)

Update `SerialCmd::authenticate()`:

```cpp
bool SerialCmd::authenticate(const char* pin) {
    auto& pm = core::PinManager::instance();

    if (pm.isBadgeBlocked()) {
        if (pm.isLockoutActive()) {
            uint32_t remainingSec = pm.getLockoutRemainingMs() / 1000;
            LOG_W(TAG, "PIN locked, %lu seconds remaining", (unsigned long)remainingSec);
            serial_audit_auth_failure("SERIAL", 3);  // lockout
        } else {
            LOG_W(TAG, "PIN permanently blocked (retries exhausted)");
            serial_audit_auth_failure("SERIAL", 1);  // blocked
        }
        return false;
    }

    if (!pin || !*pin) {
        LOG_W(TAG, "Empty PIN provided");
        serial_audit_auth_failure("SERIAL", 2);  // empty_pin
        return false;
    }

    if (!pm.verifyBadgePin(pin)) {
        LOG_W(TAG, "Authentication failed, %d retries remaining", pm.getBadgeRetries());
        serial_audit_auth_failure("SERIAL", 0);  // wrong_pin
        return false;
    }

    s_authenticated = true;
    s_authTimestamp = esp_timer_get_time();
    serial_audit_auth_success("SERIAL");  // Add audit event
    LOG_I(TAG, "Authenticated via serial");
    return true;
}
```

### Step 4: Add audit to logout() (5 min)

Update `SerialCmd::logout()`:

```cpp
void SerialCmd::logout() {
    serial_audit_logout("SERIAL", 0);  // user_logout
    s_authenticated = false;
    s_authTimestamp = 0;
    LOG_I(TAG, "Logged out");
}
```

### Step 5: Add audit to isAuthenticated() timeout (5 min)

Update `SerialCmd::isAuthenticated()`:

```cpp
bool SerialCmd::isAuthenticated() {
    if (!s_authenticated) return false;

    // Check timeout (5 minutes)
    uint64_t now = esp_timer_get_time();
    if (now - s_authTimestamp > 5 * 60 * 1000 * 1000) {
        serial_audit_logout("SERIAL", 1);  // timeout
        s_authenticated = false;
        return false;
    }
    return true;
}
```

### Step 6: Add audit to cmdAuth command (5 min)

Update `cmdAuth()` to call audit functions (already done via authenticate() integration).

## References

- NIST SP 800-53 Rev 5: AU-2 (Audit Events) - requires tracking authentication events
- OWASP Authentication Cheat Sheet - recommends audit trails for session management
- Common Criteria: FIA_UAU.4 (Single User Authentication)
- Related finding: `004-missing-admin-serial-audit.md` covers admin commands, this covers session authentication

</content>