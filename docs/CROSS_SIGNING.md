# GPG Cross-Signing Protocol (Badge2Badge)

Protokoll für den Austausch und das Cross-Signing von GPG Public Keys zwischen CDC Badges via BLE.

## Übersicht

Cross-Signing ermöglicht:
- Austausch von GPG Public Keys zwischen Badges
- Signieren empfangener Keys (Web of Trust)
- Persönliche Verifikation bei Conferences/Meetups

## Konzept

```
Badge A                             Badge B
   │                                   │
   │  ─────── GPG Key senden ───────►  │
   │                                   │
   │  ◄────── GPG Key empfangen ─────  │
   │                                   │
   │      ┌─────────────────────┐      │
   │      │  In Person Check:   │      │
   │      │  Fingerprint        │      │
   │      │  vergleichen        │      │
   │      └─────────────────────┘      │
   │                                   │
   │  ─────── Cross-Signatur ───────►  │
   │                                   │
   │  ◄────── Cross-Signatur ────────  │
   │                                   │
```

## BLE Protokoll

### UUIDs

Verwendet den gleichen GATT Service wie vCard Exchange:
- Service: `8E2F1F20-8B5D-4D7A-9A6E-4C9D6A8B1A01`

### GPG-spezifische Opcodes

| Opcode | Name | Richtung | Beschreibung |
|--------|------|----------|--------------|
| `0x11` | GPG_WRITE_START | Client→Server | Start GPG Key Transfer, +2 Byte Länge |
| `0x12` | GPG_WRITE_CONT | Client→Server | Fortsetzen |
| `0x13` | GPG_WRITE_END | Client→Server | Abschluss |
| `0x91` | GPG_DATA_START | Server→Client | Start GPG Key Response, +2 Byte Länge |
| `0x92` | GPG_DATA_CONT | Server→Client | Fortsetzen |
| `0x93` | GPG_DATA_END | Server→Client | Abschluss |

### Payload-Format

```
┌─────────┬────────────┬─────────────┬─────────────┬─────────────┬────────────┐
│ Curve   │ PubKey Len │ Public Key  │ Fingerprint │ UserID Len  │ User ID    │
│ 1 Byte  │ 1 Byte     │ 32/64 Bytes │ 20 Bytes    │ 1 Byte      │ max 63 B   │
└─────────┴────────────┴─────────────┴─────────────┴─────────────┴────────────┘
```

| Feld | Größe | Beschreibung |
|------|-------|--------------|
| Curve | 1 | `1` = Ed25519, `2` = P-256 |
| PubKey Len | 1 | 32 für Ed25519, 64 für P-256 |
| Public Key | 32/64 | Raw Public Key Bytes |
| Fingerprint | 20 | SHA-1 GPG Fingerprint |
| UserID Len | 1 | Länge der User ID |
| User ID | max 63 | "Name <email>" (UTF-8) |

**Maximum Payload:** 150 Bytes

### Ablauf

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

## Cross-Signatur

### Daten die signiert werden

```c
// SHA256 über:
uint8_t data_to_sign[84];  // 20 + 64
memcpy(data_to_sign, fingerprint, 20);       // GPG Fingerprint
memcpy(data_to_sign + 20, user_id, 64);      // User ID (padded)

// Hash
uint8_t hash[32];
SHA256(data_to_sign, 84, hash);

// Signatur mit eigenem GPG Key
gpg_sign_hash(hash, 32, signature, &sig_len);
```

### Signatur-Format

| Kurve | Signatur-Länge | Format |
|-------|----------------|--------|
| Ed25519 | 64 Bytes | R (32) + S (32) |
| P-256 | 64 Bytes | R (32) + S (32) |

## Speicherung

### NVS Schema

- **Namespace:** `gpg_recv`
- **Key-Format:** `pk_<fingerprint_hex_8>`

### Struktur

```c
typedef struct {
    uint8_t curve;                  // 1 Byte
    char user_id[64];               // 64 Bytes
    uint8_t pubkey[64];             // 64 Bytes
    uint8_t pubkey_len;             // 1 Byte
    uint8_t fingerprint[20];        // 20 Bytes
    uint32_t received_at;           // 4 Bytes (Unix timestamp)
    uint8_t my_signature[64];       // 64 Bytes (eigene Cross-Signatur)
    uint8_t sig_len;                // 1 Byte
    uint8_t flags;                  // 1 Byte (0x01 = verified in person)
} gpg_received_key_nvs_t;
```

**Maximum Keys:** 16 (NVS-Limitierung)

## API

### Empfangen

```c
// Key von anderem Badge empfangen und speichern
bool gpg_receive_pubkey(
    const uint8_t *pubkey, size_t pubkey_len,
    uint8_t curve,
    const char *user_id,
    const uint8_t *fingerprint
);
```

### Auflisten

```c
// Anzahl empfangener Keys
uint8_t gpg_received_count(void);

// Info zu einem Key abrufen
bool gpg_received_get_info(uint8_t index, gpg_received_key_info_t *info);
```

### Cross-Signing

```c
// Key signieren
bool gpg_cross_sign(uint8_t index);

// Signatur abrufen
bool gpg_received_get_signature(uint8_t index, uint8_t *sig_out, size_t *sig_len);
```

### Export für BLE

```c
// Eigenen Key für BLE-Übertragung exportieren
bool gpg_export_for_broadcast(
    uint8_t *pubkey, size_t *pubkey_len,
    uint8_t *curve,
    char *user_id,
    uint8_t *fingerprint
);
```

## Serial Commands

| Command | Beschreibung |
|---------|--------------|
| `GPG_RECV_LIST` | Alle empfangenen Keys auflisten |
| `GPG_RECV_INFO <index>` | Details zu einem Key |
| `GPG_CROSS_SIGN <index>` | Key signieren |
| `GPG_RECV_DELETE <index>` | Key löschen |

### Beispiel-Ausgabe

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

## Sicherheit

### Voraussetzungen

- BLE Secure Connections aktiviert
- Numeric Comparison für Pairing
- User muss Pairing bestätigen

### Verifikation

Cross-Signing sollte nur nach persönlicher Verifikation erfolgen:

1. Fingerprints vergleichen (Display beider Badges)
2. Name/Email verifizieren
3. Dann erst signieren

### Fingerprints

Das Badge berechnet zwei Fingerprint-Formate:

| Version | Hash | Länge | Standard | Verwendung |
|---------|------|-------|----------|------------|
| V4 | SHA-1 | 20 Bytes (40 Hex) | RFC 4880 | GnuPG 2.x |
| V5 | SHA-256 | 32 Bytes (64 Hex) | RFC 9580 | GnuPG 2.5+ |

Beide werden automatisch bei Key-Generierung berechnet und gespeichert.

### OpenPGP Export

Cross-signierte Keys können im RFC 4880 Format exportiert werden:

```bash
# Via Serial Command
GPG_EXPORT_SIGNED <index>

# Output: ASCII-armored OpenPGP
-----BEGIN PGP PUBLIC KEY BLOCK-----
...
-----END PGP PUBLIC KEY BLOCK-----
```

Das Export-Format enthält:
- Public Key Packet (Tag 6, V4)
- User ID Packet (Tag 13)
- Certification Signature (Tag 2, Type 0x10)

Import in GnuPG:
```bash
gpg --import exported_key.asc
```

## Workflow (Benutzer-Sicht)

### Key senden

1. GPG-Menü → **Key senden**
2. Badge sucht andere Badges
3. Ziel auswählen
4. BLE-Pairing bestätigen
5. Austausch läuft automatisch

### Empfangene Keys anzeigen

1. GPG-Menü → **Empfangene Keys**
2. Liste durchblättern
3. Key auswählen für Details

### Key signieren

1. Empfangenen Key in Liste auswählen
2. **Signieren** wählen
3. Fingerprint mit Besitzer vergleichen
4. Bestätigen

## Kompatibilität

- **Badge-zu-Badge:** Vollständig unterstützt
- **Mit GnuPG 2.x:** Public Keys und Cross-Signaturen exportierbar als RFC 4880 Pakete
- **Mit GnuPG 2.5+:** V5 Fingerprints (SHA-256) vorbereitet
- **BLE-Protokoll:** Badge-spezifisches GATT-Protokoll (kein Standard-BLE-Profil für GPG-Keys)

## Technische Details

### MPI Encoding (RFC 4880 Section 3.2)

Multi-Precision Integers werden mit führendem Bit-Count codiert:
- Ed25519: 256 oder 255 Bits (abhängig von MSB)
- P-256: 520 Bits (04 || X || Y = 65 Bytes × 8)

### Signatur-Semantik

| Algorithmus | OID | Signierung |
|-------------|-----|------------|
| EdDSA (Ed25519) | 1.3.6.1.4.1.11591.15.1 | Hash als "Message" |
| ECDSA (P-256) | 1.2.840.10045.3.1.7 | Hash direkt |

Beide produzieren 64-Byte Signaturen (R || S, je 32 Bytes).
