# GPG Key Management

GPG-Schlüsselverwaltung auf dem CDC Badge mit TROPIC01 Secure Element.

## Übersicht

| Feature | Beschreibung |
|---------|--------------|
| **Key Storage** | Private Keys sicher im TROPIC01 Slot 29 |
| **Unterstützte Kurven** | Ed25519 (empfohlen), P-256 (NIST) |
| **USB CCID** | SmartCard-Interface für GnuPG (optional) |
| **Cross-Signing** | Key Exchange mit anderen Badges via BLE |
| **QR Export** | Public Key als QR-Code anzeigen |

## Schnellstart

### 1. GPG-Key über das Menü erstellen

1. Hauptmenü → **GPG**
2. **Key erstellen** wählen
3. Name eingeben (T9-Tastatur)
4. E-Mail eingeben
5. Kurve wählen (Ed25519 empfohlen)
6. Warten bis Key generiert ist

### 2. GPG-Key über Serial erstellen

```bash
# GPG initialisieren (Ed25519)
echo "GPG_GENERATE 1 Max Mustermann <max@example.com>" > /dev/ttyACM0

# Oder mit P-256
echo "GPG_GENERATE 2 Max Mustermann <max@example.com>" > /dev/ttyACM0

# Status prüfen
echo "GPG_STATUS" > /dev/ttyACM0
```

## Key-Details anzeigen

Im GPG-Menü:
1. **Status** wählen
2. Zeigt: User-ID, Fingerprint, Kurve, Erstelldatum, Signaturzähler

## Public Key exportieren

### Als QR-Code (auf dem Display)

1. GPG-Menü → **QR-Code**
2. QR-Code mit Smartphone/Computer scannen

### Über Serial

```bash
echo "GPG_EXPORT" > /dev/ttyACM0
# Gibt PEM-codierten Public Key aus
```

## USB CCID SmartCard (Optional)

Der Badge kann als GPG SmartCard über USB CCID fungieren.

### Aktivieren

In `feature_flags.h`:
```c
#define FEATURE_GPG 1
#define FEATURE_GPG_CCID 1
```

### Mit GnuPG verwenden

```bash
# Karte erkennen
gpg --card-status

# Public Key von Karte holen
gpg --card-edit
> fetch

# Key-Liste prüfen
gpg --list-keys

# Test-Signatur
echo "test" | gpg --sign --armor | gpg --verify
```

**Hinweis:** Der Fingerprint wird vom Host (GnuPG) berechnet und via PUT DATA an die Karte gesendet. Bei lokaler Key-Generierung ist der Fingerprint zunächst 0x00...00 bis `gpg --card-edit > fetch` ausgeführt wird.

### VID/PID

Der Badge verwendet Gemalto VID/PID für kompatibilität:
- VID: `0x08E6`
- PID: `0x4433`

Dies umgeht die libccid Whitelist ohne udev-Regeln.

## Cross-Signing (Badge-to-Badge)

Tausche GPG Public Keys mit anderen Badges via BLE und signiere sie.

### Workflow

1. **Eigenen Key broadcasten**
   - GPG-Menü → **Key senden**
   - BLE sucht nach anderen Badges

2. **Key empfangen**
   - GPG-Menü → **Empfangene Keys**
   - Zeigt Liste aller empfangenen Keys

3. **Key signieren**
   - Empfangenen Key auswählen
   - **Signieren** wählen
   - Cross-Signatur wird erstellt

### Cross-Sign Format

Die Signatur wird über folgende Daten erstellt:
```
SHA256(fingerprint || user_id)
```

### Serial Commands für empfangene Keys

```bash
# Empfangene Keys auflisten
echo "GPG_RECV_LIST" > /dev/ttyACM0

# Details zu einem Key
echo "GPG_RECV_INFO 0" > /dev/ttyACM0

# Key signieren
echo "GPG_CROSS_SIGN 0" > /dev/ttyACM0

# Key löschen
echo "GPG_RECV_DELETE 0" > /dev/ttyACM0
```

## Serial Commands

| Command | Beschreibung |
|---------|--------------|
| `GPG_STATUS` | Zeigt Key-Status (User-ID, Fingerprint, etc.) |
| `GPG_GENERATE <curve> <user_id>` | Key erstellen (curve: 1=Ed25519, 2=P-256) |
| `GPG_EXPORT` | Public Key als PEM exportieren |
| `GPG_RESET` | Key löschen (erfordert CONFIRM) |
| `GPG_RECV_LIST` | Empfangene Keys auflisten |
| `GPG_RECV_INFO <index>` | Details zu empfangenem Key |
| `GPG_CROSS_SIGN <index>` | Empfangenen Key signieren |
| `GPG_RECV_DELETE <index>` | Empfangenen Key löschen |

## Speicher-Architektur

### TROPIC01

| Slot | Verwendung |
|------|------------|
| 29 | GPG Master Key (ECC) |

### R-Memory

| Slot | Verwendung |
|------|------------|
| 134 | GPG Metadata (User-ID, Fingerprint, etc.) |

### NVS

| Namespace | Verwendung |
|-----------|------------|
| `gpg_recv` | Empfangene Public Keys |

## API-Referenz

Siehe `components/gpg/gpg.h` für die vollständige C-API.

### Wichtige Funktionen

```c
// Initialisierung
bool gpg_init(void);
bool gpg_is_initialized(void);
bool gpg_get_status(gpg_status_t *status);

// Key Management
bool gpg_set_pending_user_id(const char *user_id);
bool gpg_generate_key(uint8_t curve);  // CDC_CURVE_ED25519 oder CDC_CURVE_P256
bool gpg_reset(void);

// Export
bool gpg_export_pubkey_pem(char *buf, size_t size, size_t *out_len);
bool gpg_get_fingerprint(uint8_t *fp_out);

// Signing
bool gpg_sign_hash(const uint8_t *hash, size_t hash_len,
                   uint8_t *sig_out, size_t *sig_len);

// Cross-Signing
bool gpg_receive_pubkey(const uint8_t *pubkey, size_t pubkey_len, uint8_t curve,
                        const char *user_id, const uint8_t *fingerprint);
uint8_t gpg_received_count(void);
bool gpg_received_get_info(uint8_t index, gpg_received_key_info_t *info);
bool gpg_cross_sign(uint8_t index);
```

## Feature Flags

In `feature_flags.h`:

```c
#define FEATURE_GPG 1       // GPG Key Storage (Standard: aktiviert)
#define FEATURE_GPG_CCID 0  // USB CCID SmartCard (Standard: deaktiviert)
```

## Einschränkungen

- **ECDH/Decryption:** Nicht unterstützt (TROPIC01 Limitierung)
- **RSA:** Nicht unterstützt
- **Subkeys:** Nicht unterstützt (nur Master Key)
- **Empfangene Keys:** Max. 16 Keys in NVS

## Troubleshooting

### Key Generation schlägt fehl

1. Prüfen ob User-ID gesetzt: `GPG_STATUS`
2. TROPIC01 Status prüfen: `TR01_STATUS`
3. Slot 29 frei?: `TR01_SLOTS`

### CCID wird nicht erkannt

1. `FEATURE_GPG_CCID` in feature_flags.h aktiviert?
2. USB neu verbinden
3. `gpg --card-status` mit Debug: `gpg --debug ccid --card-status`

### Cross-Sign funktioniert nicht

1. Eigener Key muss existieren
2. BLE muss aktiviert sein
3. Anderer Badge muss in Reichweite sein
