---
title: "[MEDIUM] APDU parser doesn't handle Lc=0 with data field edge case"
severity: MEDIUM
domain: mod_gpg/openpgp/apdu
lens: edge-case-testing
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `apdu_parse()` (file: `components/mod_gpg/src/openpgp/apdu.cpp:17-88`), when parsing APDU commands with Lc=0 (zero-length data), the parser correctly sets `apdu->data = nullptr`, but the check at lines 64-68 doesn't handle the edge case where Lc=0 but there's still data in the buffer (which could indicate a malformed APDU).

Lines 64-68:
```cpp
if (apdu->lc > 0) {
    if (pos + apdu->lc > raw_len) {
        return false;  // Not enough data
    }
    apdu->data = raw + pos;
    pos += apdu->lc;
}
```

When `apdu->lc == 0`, the data pointer is set earlier to `nullptr`, but if the APDU has extra bytes after the header that don't match the expected format, they are silently ignored.

## Impact
- **Silent data loss**: Malformed APDUs with extra bytes may be accepted
- **Protocol compliance**: ISO 7816-4 specifies exact APDU formats; extra bytes should be rejected
- **Security**: Could allow injection of unexpected data that gets ignored

## Evidence
File: `components/mod_gpg/src/openpgp/apdu.cpp`, lines 17-88

```cpp
bool apdu_parse(const uint8_t *raw, size_t raw_len, apdu_t *apdu) {
    if (!raw || !apdu || raw_len < 4) {
        return false;
    }

    memset(apdu, 0, sizeof(apdu_t));

    apdu->cla = raw[0];
    apdu->ins = raw[1];
    apdu->p1  = raw[2];
    apdu->p2  = raw[3];

    // Case 1: No Lc, no Le (just CLA INS P1 P2)
    if (raw_len == 4) {
        apdu->lc = 0;
        apdu->le = 0;
        apdu->data = nullptr;
        return true;  // Line 35
    }

    size_t pos = 4;

    // Check for extended APDU (first length byte is 0x00)
    if (raw[pos] == 0x00 && raw_len > 7) {
        apdu->extended = true;

        // Extended Lc (3 bytes: 0x00 + 2 bytes)
        if (pos + 3 <= raw_len) {
            apdu->lc = (raw[pos + 1] << 8) | raw[pos + 2];
            pos += 3;
        }
    } else {
        apdu->extended = false;

        // Short Lc (1 byte) or Le
        if (raw_len == 5) {
            // Case 2: Le only
            apdu->lc = 0;
            apdu->le = raw[pos] == 0 ? 256 : raw[pos];
            apdu->data = nullptr;
            return true;
        }

        apdu->lc = raw[pos];
        pos++;
    }

    // Command data (lines 64-71)
    if (apdu->lc > 0) {
        if (pos + apdu->lc > raw_len) {
            return false;  // Not enough data
        }
        apdu->data = raw + pos;
        pos += apdu->lc;
    }

    // Le field (optional)
    if (pos < raw_len) {
        // ...
    }

    return true;
}
```

Edge cases:
1. `CLA INS P1 P2 00` (5 bytes, Lc=0) → Treated as Case 2 (Le only), Lc=0, Le=256
2. `CLA INS P1 P2 00 00 00` (7 bytes, extended Lc=0) → Lc=0, data=nullptr, but 2 extra bytes
3. `CLA INS P1 P2 05 00 00 00 01 02 03 04 05` → Lc=5, but short Lc format

## Recommended Fix
Add validation to ensure APDU length matches expected format:

```cpp
bool apdu_parse(const uint8_t *raw, size_t raw_len, apdu_t *apdu) {
    if (!raw || !apdu || raw_len < 4) {
        return false;
    }

    memset(apdu, 0, sizeof(apdu_t));

    apdu->cla = raw[0];
    apdu->ins = raw[1];
    apdu->p1  = raw[2];
    apdu->p2  = raw[3];

    // Case 1: No Lc, no Le (just CLA INS P1 P2)
    if (raw_len == 4) {
        apdu->lc = 0;
        apdu->le = 0;
        apdu->data = nullptr;
        return true;
    }

    size_t pos = 4;

    // Check for extended APDU (first length byte is 0x00)
    if (raw[pos] == 0x00 && raw_len > 7) {
        apdu->extended = true;

        // Extended Lc (3 bytes: 0x00 + 2 bytes)
        if (pos + 3 <= raw_len) {
            apdu->lc = (raw[pos + 1] << 8) | raw[pos + 2];
            pos += 3;
        }
    } else {
        apdu->extended = false;

        // Short Lc (1 byte) or Le
        if (raw_len == 5) {
            // Case 2: Le only
            apdu->lc = 0;
            apdu->le = raw[pos] == 0 ? 256 : raw[pos];
            apdu->data = nullptr;
            return true;
        }

        apdu->lc = raw[pos];
        pos++;
    }

    // Command data
    if (apdu->lc > 0) {
        if (pos + apdu->lc > raw_len) {
            return false;  // Not enough data
        }
        apdu->data = raw + pos;
        pos += apdu->lc;
    } else if (apdu->lc == 0 && pos < raw_len) {
        // Lc=0 but there's more data - could be Le or malformed
        // For now, treat remaining as Le field
    }

    // Le field (optional)
    if (pos < raw_len) {
        if (apdu->extended) {
            // Extended Le (2 bytes)
            if (pos + 2 <= raw_len) {
                uint16_t le_val = (raw[pos] << 8) | raw[pos + 1];
                apdu->le = (le_val == 0) ? 65536 : le_val;
            }
        } else {
            // Short Le (1 byte)
            apdu->le = (raw[pos] == 0) ? 256 : raw[pos];
        }
    }

    // Validate total length
    if (pos != raw_len) {
        // Extra bytes not accounted for - could indicate malformed APDU
        // For now, allow it but this could be stricter
    }

    return true;
}
```

## References
- ISO/IEC 7816-4:2005 (APDU structure)
- CWE-20: Improper input validation
