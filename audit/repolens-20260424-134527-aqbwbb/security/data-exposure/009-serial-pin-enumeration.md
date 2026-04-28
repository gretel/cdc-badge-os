---
title: "[MEDIUM] Serial command PIN enumeration via retry messages"
severity: MEDIUM
domain: serial-cmd
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The serial command interface in `components/serial_cmd/src/SerialCmd.cpp` exposes detailed PIN status information through error messages. Lines 839-847 output specific retry counts and lockout status, enabling user enumeration and PIN brute-force guidance.

## Impact
- **User Enumeration**: Different messages for "locked" vs "permanently locked" vs "wrong PIN" allow attackers to distinguish between different states
- **Retry Count Leakage**: Exact retry count is revealed, helping attackers plan brute-force attempts
- **Lockout Timing**: Lockout duration is exposed, allowing attackers to time their attempts
- **PIN Status Disclosure**: The `PIN_STATUS` command at line 884 exposes full PIN state (retries, blocked, set)

## Evidence
File: `components/serial_cmd/src/SerialCmd.cpp:839-847`

Lines 839-846 (PIN authentication failure messages):
```cpp
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
```

Lines 884-887 (PIN status command):
```cpp
Console::printf("Badge PIN: retries=%d blocked=%s set=%s\r\n",
                pm.getBadgeRetries(),
                pm.isBadgeBlocked() ? "yes" : "no",
                pm.isPinSet() ? "yes" : "no");
```

## Recommended Fix
1. **Normalize error messages** - use the same generic message for all PIN failures
2. **Remove retry counts** from error messages
3. **Add delay** after each authentication attempt to slow brute-force
4. **Require authentication** for `PIN_STATUS` command

Example fix:
```cpp
// Generic message for all PIN failures
Console::printf("ERROR: Wrong PIN.\r\n");
// No retry count, no lockout timing
```

## References
- OWASP: [Authentication Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html)
- CTAP2 specification: PIN protocol should not reveal retry counts in responses

</content>