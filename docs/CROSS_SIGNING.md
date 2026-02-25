# GPG Cross-Signing Protocol (Badge2Badge)

Protocol specification for exchanging and cross-signing GPG public keys between CDC Badges via BLE.

> **Related:** [GPG Module](GPG.md) | [BLE vCard Protocol](ble_vcard_protocol.md)

## Overview

Cross-signing enables:
- Exchange of GPG public keys between badges
- Signing received keys (Web of Trust)
- Personal verification at conferences/meetups

## Concept

```
Badge A                             Badge B
   │                                   │
   │  ─────── Send GPG Key ─────────►  │
   │                                   │
   │  ◄────── Receive GPG Key ───────  │
   │                                   │
   │      ┌─────────────────────┐      │
   │      │  In-Person Check:   │      │
   │      │  Compare            │      │
   │      │  Fingerprints       │      │
   │      └─────────────────────┘      │
   │                                   │
   │  ─────── Cross-Signature ───────► │
   │                                   │
   │  ◄────── Cross-Signature ──────── │
   │                                   │
```

## BLE Protocol

### UUIDs

Uses the same GATT service as vCard Exchange:
- Service: `8E2F1F20-8B5D-4D7A-9A6E-4C9D6A8B1A01`

### GPG-Specific Opcodes

| Opcode | Name | Direction | Description |
|--------|------|-----------|-------------|
| `0x11` | GPG_WRITE_START | Client→Server | Start GPG key transfer, +2 byte length |
| `0x12` | GPG_WRITE_CONT | Client→Server | Continue |
| `0x13` | GPG_WRITE_END | Client→Server | Complete |
| `0x91` | GPG_DATA_START | Server→Client | Start GPG key response, +2 byte length |
| `0x92` | GPG_DATA_CONT | Server→Client | Continue |
| `0x93` | GPG_DATA_END | Server→Client | Complete |

### Payload Format

```
┌─────────┬────────────┬─────────────┬─────────────┬─────────────┬────────────┐
│ Curve   │ PubKey Len │ Public Key  │ Fingerprint │ UserID Len  │ User ID    │
│ 1 Byte  │ 1 Byte     │ 32/64 Bytes │ 20 Bytes    │ 1 Byte      │ max 63 B   │
└─────────┴────────────┴─────────────┴─────────────┴─────────────┴────────────┘
```

| Field | Size | Description |
|-------|------|-------------|
| Curve | 1 | `1` = Ed25519, `2` = P-256 |
| PubKey Len | 1 | 32 for Ed25519, 64 for P-256 |
| Public Key | 32/64 | Raw public key bytes |
| Fingerprint | 20 | SHA-1 GPG fingerprint |
| UserID Len | 1 | Length of User ID |
| User ID | max 63 | "Name <email>" (UTF-8) |

**Maximum Payload:** 150 Bytes

### Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Exchange Flow                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Client (Initiator)              Server (Responder)                  │
│        │                                │                            │
│        │──── Connect + Pairing ────────►│                            │
│        │                                │                            │
│        │──── GPG_WRITE_START (len) ────►│                            │
│        │──── GPG_WRITE_CONT ───────────►│                            │
│        │──── GPG_WRITE_END ────────────►│                            │
│        │                                │                            │
│        │◄─── GPG_DATA_START (len) ──────│                            │
│        │◄─── GPG_DATA_CONT ─────────────│                            │
│        │◄─── GPG_DATA_END ──────────────│                            │
│        │                                │                            │
│        │──── Disconnect ───────────────►│                            │
│        │                                │                            │
└─────────────────────────────────────────────────────────────────────┘
```

## Cross-Signature

### Data Being Signed

```c
// SHA256 over:
uint8_t data_to_sign[84];  // 20 + 64
memcpy(data_to_sign, fingerprint, 20);       // GPG Fingerprint
memcpy(data_to_sign + 20, user_id, 64);      // User ID (padded)

// Hash
uint8_t hash[32];
SHA256(data_to_sign, 84, hash);

// Signature with own GPG Signature Key (Slot 27)
gpg_sign_hash(hash, 32, signature, &sig_len);
```

### Signature Format

| Curve | Signature Length | Format |
|-------|------------------|--------|
| Ed25519 | 64 Bytes | R (32) + S (32) |
| P-256 | 64 Bytes | R (32) + S (32) |

## Storage

### NVS Schema

- **Namespace:** `gpg_recv`
- **Key Format:** `pk_<fingerprint_hex_8>`

### Structure

```c
typedef struct {
    uint8_t curve;                  // 1 Byte
    char user_id[64];               // 64 Bytes
    uint8_t pubkey[64];             // 64 Bytes
    uint8_t pubkey_len;             // 1 Byte
    uint8_t fingerprint[20];        // 20 Bytes
    uint32_t received_at;           // 4 Bytes (Unix timestamp)
    uint8_t my_signature[64];       // 64 Bytes (own cross-signature)
    uint8_t sig_len;                // 1 Byte
    uint8_t flags;                  // 1 Byte (0x01 = verified in person)
} gpg_received_key_nvs_t;
```

**Maximum Keys:** 16 (NVS limitation)

## API

### Receiving

```c
// Receive key from another badge and store
bool gpg_receive_pubkey(
    const uint8_t *pubkey, size_t pubkey_len,
    uint8_t curve,
    const char *user_id,
    const uint8_t *fingerprint
);
```

### Listing

```c
// Number of received keys
uint8_t gpg_received_count(void);

// Get info for a key
bool gpg_received_get_info(uint8_t index, gpg_received_key_info_t *info);
```

### Cross-Signing

```c
// Sign a key
bool gpg_cross_sign(uint8_t index);

// Get signature
bool gpg_received_get_signature(uint8_t index, uint8_t *sig_out, size_t *sig_len);
```

### Export for BLE

```c
// Export own key for BLE transmission
bool gpg_export_for_broadcast(
    uint8_t *pubkey, size_t *pubkey_len,
    uint8_t *curve,
    char *user_id,
    uint8_t *fingerprint
);
```

## Serial Commands

| Command | Description |
|---------|-------------|
| `GPG_RECV_LIST` | List all received keys |
| `GPG_RECV_INFO <index>` | Details for a key |
| `GPG_CROSS_SIGN <index>` | Sign key |
| `GPG_RECV_DELETE <index>` | Delete key |

### Example Output

```
> GPG_RECV_LIST
OK: 2 received keys
[0] Max Mustermann <max@example.com>
    FP: ABCD1234...
    Signed: Yes
[1] Anna Schmidt <anna@example.org>
    FP: 5678EFGH...
    Signed: No

> GPG_RECV_INFO 0
OK: Key details
User-ID: Max Mustermann <max@example.com>
Curve: Ed25519
Fingerprint: ABCD1234567890ABCDEF1234567890ABCDEF1234
Received: 2026-01-19 14:30:00
Signed: Yes
Signature: (hex dump)
```

## Security

### Prerequisites

- BLE Secure Connections enabled
- Numeric Comparison for pairing
- User must confirm pairing

### Verification

Cross-signing should only occur after personal verification:

1. Compare fingerprints (display on both badges)
2. Verify name/email
3. Then sign

### Fingerprints

The badge calculates two fingerprint formats:

| Version | Hash | Length | Standard | Usage |
|---------|------|--------|----------|-------|
| V4 | SHA-1 | 20 Bytes (40 Hex) | RFC 4880 | GnuPG 2.x |
| V5 | SHA-256 | 32 Bytes (64 Hex) | RFC 9580 | GnuPG 2.5+ |

Both are automatically calculated during key generation and stored.

### OpenPGP Export

Cross-signed keys can be exported in RFC 4880 format:

```bash
# Via Serial Command
GPG_EXPORT_SIGNED <index>

# Output: ASCII-armored OpenPGP
-----BEGIN PGP PUBLIC KEY BLOCK-----
...
-----END PGP PUBLIC KEY BLOCK-----
```

The export format contains:
- Public Key Packet (Tag 6, V4)
- User ID Packet (Tag 13)
- Certification Signature (Tag 2, Type 0x10)

Import into GnuPG:
```bash
gpg --import exported_key.asc
```

## Workflow (User Perspective)

### Send Key

1. GPG Menu → **Send Key**
2. Badge searches for other badges
3. Select target
4. Confirm BLE pairing
5. Exchange runs automatically

### View Received Keys

1. GPG Menu → **Received Keys**
2. Browse list
3. Select key for details

### Sign Key

1. Select received key in list
2. Select **Sign**
3. Compare fingerprint with owner
4. Confirm

## Compatibility

- **Badge-to-Badge:** Fully supported
- **With GnuPG 2.x:** Public keys and cross-signatures exportable as RFC 4880 packets
- **With GnuPG 2.5+:** V5 fingerprints (SHA-256) prepared
- **BLE Protocol:** Badge-specific GATT protocol (no standard BLE profile for GPG keys)

## Technical Details

### MPI Encoding (RFC 4880 Section 3.2)

Multi-Precision Integers are encoded with a leading bit count:
- Ed25519: 256 or 255 bits (depending on MSB)
- P-256: 520 bits (04 || X || Y = 65 Bytes × 8)

### Signature Semantics

| Algorithm | OID | Signing |
|-----------|-----|---------|
| EdDSA (Ed25519) | 1.3.6.1.4.1.11591.15.1 | Hash as "Message" |
| ECDSA (P-256) | 1.2.840.10045.3.1.7 | Hash directly |

Both produce 64-byte signatures (R || S, 32 bytes each).
