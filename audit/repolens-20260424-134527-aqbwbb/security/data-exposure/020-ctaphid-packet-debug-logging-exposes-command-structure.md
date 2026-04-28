---
title: "[LOW] CTAPHID packet-level debug logging exposes command structure"
severity: LOW
domain: mod_fido2
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
In `components/mod_fido2/src/ctaphid.cpp`, the `CTAPHID_DEBUG_PACKETS` flag (which defaults to `DEBUG_MODE`) enables verbose logging of CTAPHID packet processing. This logs channel IDs, command types, and message lengths for all CTAPHID operations including CBOR commands.

## Impact
- **Command Structure Exposure**: Logs reveal which CTAP2 commands are being sent (MakeCredential, GetAssertion, GetInfo, etc.) via command type bytes
- **Channel ID Exposure**: Logs show channel identifiers which could help track multiple concurrent sessions
- **Message Length Exposure**: Response lengths can help infer the type of data being processed
- **Protocol Reconnaissance**: Attackers can map the CTAPHID protocol flow and timing

## Evidence
File: `components/mod_fido2/src/ctaphid.cpp`

Line 26 (definition):
```cpp
#ifndef CTAPHID_DEBUG_PACKETS
#define CTAPHID_DEBUG_PACKETS       DEBUG_MODE  // Controlled by feature_flags.h
#endif
```

Multiple logging statements throughout the file:
```cpp
Line 248: if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "INIT: allocated CID 0x%08lX", new_cid);
Line 260: if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "PING: echoing %d bytes", len);
Line 270: if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "WINK");
Line 280: if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "CANCEL");
Line 306: if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "CBOR: cmd=0x02X status=0x%02X len=%d", data[0], status, response_len);
Line 319: if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "Processing cmd=0x%02X len=%d", cmd, len);
Line 347: if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "U2F: response len=%d", u2f_response_len);
Line 440: if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "Init packet: CID=0x%08lX CMD=0x%02X BCNT=%d", cid, cmd, bcnt);
```

The CBOR command logging at line 306 is particularly notable as it reveals the CTAP2 command type (e.g., 0x01 = MakeCredential, 0x02 = GetAssertion, etc.).

## Recommended Fix
1. **Reduce logging verbosity**: Log only summary information without command types
2. **Add specific debug flag**: Use a more specific flag like `CTAPHID_VERBOSE` separate from `DEBUG_MODE`
3. **Log only errors**: Keep error-level logging but reduce debug-level packet logging

Example fix:
```cpp
// Before:
if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "CBOR: cmd=0x%02X status=0x%02X len=%d", data[0], status, response_len);

// After (less verbose):
if (CTAPHID_DEBUG_PACKETS) LOG_D("CTAPHID", "CBOR: status=0x%02X len=%d", status, response_len);

// Or remove entirely:
// Packet-level logging is verbose; consider removing or using trace-level
```

## References
- FIDO2 CTAP2 specification: Command types
- OWASP: [Logging Cheat Sheet - Sensitive Data](https://cheatsheetseries.owasp.org/cheatsheets/Logging_Cheat_Sheet.html)
- Related finding: #004 (FIDO2 makeCredential response dumped to debug logs)
- Related finding: #010 (ESP_LOG bypasses cdc_log)

</content>