---
title: "[MEDIUM] FIDO2 CTAP2 logs Relying Party (RP) IDs during credential creation and assertion"
severity: MEDIUM
domain: Privacy by Design
lens: PII-in-Logs
labels:
  - "audit:compliance/privacy-by-design"
---

## Summary
The FIDO2 CTAP2 module logs Relying Party (RP) IDs at multiple points during credential creation and assertion operations. These log statements reveal which services the user is authenticating to or registering credentials for.

**Locations:** `components/mod_fido2/src/ctap2.cpp`

Key log statements:
- Line 509: `LOG_I("CTAP2", "User presence required for %s at %s", ...)`
- Line 979: `LOG_I("CTAP2", "Browser probe request (%s) - waiting for user selection", p->rp_id);`
- Line 1153: `LOG_I("CTAP2", "Created credential for %s (slot %d)", p->rp_id, slot);`
- Line 1191: `LOG_I("CTAP2", "makeCredential rp_id=%s rk=%d ...", ...)`
- Line 1777: `LOG_I("CTAP2", "getAssertion rp_id=%s allowList=%d ...", ...)`

## Impact
- **Service Discovery:** Logs reveal all services where the user has (or is creating) FIDO2 credentials, providing a complete picture of the user's online presence.
- **Authentication Context:** Every authentication attempt is logged with the RP ID, creating a detailed authentication history.
- **Credential Creation Tracking:** When new credentials are created, the RP ID is logged, revealing which new services the user is adding.
- **Browser Probe Logging:** Browser discovery requests are logged, showing which sites are checking for FIDO2 support.

## Evidence
**File:** `components/mod_fido2/src/ctap2.cpp`

**Line 509:**
```cpp
LOG_I("CTAP2", "User presence required for %s at %s",
      action == FIDO2_ACTION_SIGN ? "sign" : "register", rp_id);
```

**Line 979:**
```cpp
LOG_I("CTAP2", "Browser probe request (%s) - waiting for user selection", p->rp_id);
```

**Line 1153:**
```cpp
LOG_I("CTAP2", "Created credential for %s (slot %d)", p->rp_id, slot);
```

**Line 1191:**
```cpp
LOG_I("CTAP2", "makeCredential rp_id=%s rk=%d uv=%d up=%d alg=%d pinProto=%d pinAuthLen=%zu",
      p->rp_id, p->rank, ...);
```

**Line 1777:**
```cpp
LOG_I("CTAP2", "getAssertion rp_id=%s allowList=%d count=%u uv=%d up=%d appid=%s",
      rp_id, ...);
```

## Recommended Fix
1. **Remove RP ID from most log statements:**
   ```cpp
   LOG_I("CTAP2", "User presence required for %s",
         action == FIDO2_ACTION_SIGN ? "sign" : "register");
   ```

2. **Use hash of RP ID for debugging:**
   ```cpp
   uint32_t rp_hash = fnv1a_hash(p->rp_id);
   LOG_I("CTAP2", "Created credential (slot %d, rp_hash=0x%08X)", slot, rp_hash);
   ```

3. **Add DEBUG_MODE guard for detailed RP logging:**
   ```cpp
   #ifdef DEBUG_MODE
   LOG_I("CTAP2", "makeCredential rp_id=%s rk=%d ...", p->rp_id, p->rank);
   #endif
   ```

4. **Consider reducing log level** for routine operations from INFO to DEBUG.

## References
- GDPR Article 5(1)(c) - Data minimization
- W3C WebAuthn Specification - Credential metadata handling
- FIDO Alliance - Privacy considerations for authentication
- Common logging best practices: Avoid logging authentication context
