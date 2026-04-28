---
title: "[LOW] TR01_SESSION command has inconsistent idempotency behavior"
severity: LOW
domain: api-design/api-idempotency
lens: api-idempotency
labels:
  - audit:api-design/api-idempotency
---

## Summary
The `TR01_SESSION` command restarts the TROPIC01 session, but its behavior differs based on current state. When called with an active session, it prints "Session already active, reconnecting..." and restarts. When called with no session, it just starts. This inconsistency can confuse users about whether the operation was a no-op or performed work.

**Location:** `components/serial_cmd/src/SerialCmd.cpp:953-962` (cmdTr01Session)

## Impact
- **Inconsistent feedback:** Different messages for different initial states
- **Unclear idempotency:** User doesn't know if restart happened or was skipped
- **Race condition potential:** If session expires between check and action, behavior is undefined

## Evidence
```cpp
// components/serial_cmd/src/SerialCmd.cpp:953-962
static void cmdTr01Session(const char* args) {
    (void)args;
    auto* se = getSecureElementWithCheck();
    if (!se) return;

    if (se->isSessionActive()) {
        Console::printf("Session already active, reconnecting...\r\n");
        se->sessionEnd();
    }

    if (se->sessionStart()) {
        Console::printf("OK: Session started\r\n");
    } else {
        Console::printf("ERROR: Session start failed\r\n");
    }
}
```

Behavior:
- **No session:** "OK: Session started"
- **Session active:** "Session already active, reconnecting..." then "OK: Session started"
- **Session expired between calls:** "OK: Session started" (same as no session case)

## Recommended Fix
Standardize the response to clearly indicate what happened:

```cpp
static void cmdTr01Session(const char* args) {
    (void)args;
    auto* se = getSecureElementWithCheck();
    if (!se) return;

    bool was_active = se->isSessionActive();

    if (was_active) {
        se->sessionEnd();
    }

    if (se->sessionStart()) {
        if (was_active) {
            Console::printf("OK: Session restarted (was active)\r\n");
        } else {
            Console::printf("OK: Session started\r\n");
        }
    } else {
        Console::printf("ERROR: Session start failed\r\n");
    }
}
```

Alternatively, for true idempotency (always same response):
```cpp
static void cmdTr01Session(const char* args) {
    (void)args;
    auto* se = getSecureElementWithCheck();
    if (!se) return;

    // Always restart for consistency
    if (se->isSessionActive()) {
        se->sessionEnd();
    }

    if (se->sessionStart()) {
        Console::printf("OK: Session active\r\n");
    } else {
        Console::printf("ERROR: Session start failed\r\n");
    }
}
```

## References
- Idempotency: Same input should produce same observable output
- Similar pattern: `cmdTr01Resync` has clearer messaging about cache invalidation
