---
title: "[LOW] RP ID buffer size may be tight for long domain names"
severity: LOW
domain: security
lens: session-nuclei
labels:
  - audit:toolgate/session-nuclei
---

## Summary

The FIDO2 Relying Party ID buffer is defined as 64 characters in `components/mod_fido2/include/mod_fido2/fido2.h`:

```cpp
#define FIDO2_RP_ID_MAX_LEN     64
```

This may be tight for some valid RP IDs, particularly with subdomains or longer domain names.

## Impact

- **Edge case truncation**: Very long RP IDs (e.g., `very-long-subdomain.example.com`) could be truncated, potentially causing issues.
- **FIDO2 spec compliance**: The WebAuthn spec allows RP IDs up to 253 characters (full DNS name).
- **Buffer overflow risk**: If `cbor_read_text` receives a longer string than expected, it could overflow the buffer.

## Evidence

**File**: `components/mod_fido2/include/mod_fido2/fido2.h` (line 19)

```cpp
#define FIDO2_RP_ID_MAX_LEN     64
```

**File**: `components/mod_fido2/include/mod_fido2/fido2.h` (line 52)

```cpp
char rp_id[FIDO2_RP_ID_MAX_LEN];        // Relying Party ID (e.g., "github.com" or "ssh:server")
```

**File**: `components/mod_fido2/src/ctap2.cpp` (line 700)

```cpp
cbor_read_text(r, p->rp_id, sizeof(p->rp_id), &id_len);
```

The `cbor_read_text` function is called with `sizeof(p->rp_id)` which is 64 bytes. Need to verify that `cbor_read_text` properly null-terminates and respects the size limit.

## Recommended Fix

**Option 1: Increase buffer size**
Change `FIDO2_RP_ID_MAX_LEN` to 256 to accommodate the maximum DNS name length:

```cpp
#define FIDO2_RP_ID_MAX_LEN     256
```

**Option 2: Add validation**
Add explicit validation after reading the RP ID to ensure it fits:

```cpp
cbor_read_text(r, p->rp_id, sizeof(p->rp_id), &id_len);
if (id_len >= sizeof(p->rp_id)) {
    return CTAP2_ERR_INVALID_CBOR;  // RP ID too long
}
```

**Option 3: Verify cbor_read_text behavior**
Confirm that `cbor_read_text` properly handles the size limit and null-termination. If it already does, add a comment explaining this.

## References

- WebAuthn Specification: RP ID definition
- DNS maximum label length: 63 characters, full name up to 253 characters
- FIDO2 CTAP2 specification
