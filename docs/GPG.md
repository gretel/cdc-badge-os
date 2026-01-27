# GPG Key Management

> **Note:** This document is a design snapshot and may not reflect the current implementation. Features described here may have changed significantly. Always refer to the source code for accurate details.

GPG key management on the CDC Badge using the TROPIC01 Secure Element.

## Overview

| Feature | Description |
|---------|-------------|
| **Key Storage** | Private keys stored securely in TROPIC01 Slots 27-29 |
| **Key Types** | Signature (SIG), Decryption (DEC), Authentication (AUT) |
| **Supported Curves** | Ed25519 (SIG/AUT), P-256 ECDH (DEC), P-256 ECDSA (all) |
| **USB CCID** | SmartCard interface for GnuPG (OpenPGP 3.4) |
| **Cross-Signing** | Key exchange with other badges via BLE |
| **QR Export** | Display public key as QR code |

## Quick Start

### 1. Generate GPG Keys via Menu

1. Main Menu → **GPG**
2. Select **Generate Key**
3. Enter name (T9 keyboard)
4. Enter email
5. Select curve (Ed25519 recommended)
6. Wait for key generation (creates all 3 keys)

### 2. Generate GPG Keys via Serial

```bash
# Generate with Ed25519 (SIG/AUT) + P-256 (DEC)
echo "GPG_GENERATE 1 Max Mustermann <max@example.com>" > /dev/ttyACM0

# Or with P-256 for all keys
echo "GPG_GENERATE 2 Max Mustermann <max@example.com>" > /dev/ttyACM0

# Check status
echo "GPG_STATUS" > /dev/ttyACM0
```

**Note:** `GPG_GENERATE` always creates three keys:
- **SIG** (Slot 27): Signature key - Ed25519 or P-256 ECDSA
- **DEC** (Slot 28): Decryption key - Always P-256 ECDH (TROPIC01 has no X25519)
- **AUT** (Slot 29): Authentication key - Ed25519 or P-256 ECDSA

## Key Details

In the GPG menu:
1. Select **Status**
2. Shows: User-ID, Fingerprints, Curves, Creation date, Signature counter

## Public Key Export

### As QR Code (on display)

1. GPG Menu → **QR Code**
2. Scan QR code with smartphone/computer

### Via Serial

```bash
echo "GPG_EXPORT" > /dev/ttyACM0
# Outputs PEM-encoded public keys
```

## USB CCID SmartCard

The badge functions as an OpenPGP 3.4 SmartCard via USB CCID.

### Enable

In `feature_flags.h`:
```c
#define FEATURE_GPG 1
```

### Use with GnuPG

```bash
# Detect card
gpg --card-status

# Fetch public key from card
gpg --card-edit
> fetch

# Check key list
gpg --list-keys

# Test signature
echo "test" | gpg --sign --armor | gpg --verify
```

### Card Serial Number

The card serial number is derived from the ESP32 MAC address (last 4 bytes), making each badge unique. Manufacturer ID is "CD" (0x4344).

### VID/PID

The badge uses Gemalto VID/PID for compatibility:
- VID: `0x08E6`
- PID: `0x4433`

This bypasses the libccid whitelist without requiring udev rules.

## Cross-Signing (Badge-to-Badge)

Exchange GPG public keys with other badges via BLE and sign them.

### Workflow

1. **Broadcast own key**
   - GPG Menu → **Send Key**
   - BLE searches for other badges

2. **Receive key**
   - GPG Menu → **Received Keys**
   - Shows list of all received keys

3. **Sign key**
   - Select received key
   - Select **Sign**
   - Cross-signature is created

### Cross-Sign Format

The signature is created over the following data:
```
SHA256(fingerprint || user_id)
```

### Serial Commands for Received Keys

```bash
# List received keys
echo "GPG_RECV_LIST" > /dev/ttyACM0

# Details for a key
echo "GPG_RECV_INFO 0" > /dev/ttyACM0

# Sign key
echo "GPG_CROSS_SIGN 0" > /dev/ttyACM0

# Delete key
echo "GPG_RECV_DELETE 0" > /dev/ttyACM0
```

## Serial Commands

| Command | Description |
|---------|-------------|
| `GPG_STATUS` | Show key status (User-ID, Fingerprints, etc.) |
| `GPG_GENERATE <curve> <user_id>` | Generate keys (curve: 1=Ed25519, 2=P-256) |
| `GPG_EXPORT` | Export public keys as PEM |
| `GPG_RESET` | Delete all keys (requires CONFIRM) |
| `GPG_RECV_LIST` | List received keys |
| `GPG_RECV_INFO <index>` | Details for received key |
| `GPG_CROSS_SIGN <index>` | Sign received key |
| `GPG_RECV_DELETE <index>` | Delete received key |

## Storage Architecture

### TROPIC01 ECC Slots

| Slot | Usage |
|------|-------|
| 27 | GPG Signature Key (SIG) |
| 28 | GPG Decryption Key (DEC) |
| 29 | GPG Authentication Key (AUT) |

### R-Memory

| Slot | Usage |
|------|-------|
| 134 | GPG Metadata (User-ID, Fingerprints, etc.) |

### NVS

| Namespace | Usage |
|-----------|-------|
| `openpgp` | OpenPGP card state (fingerprints, gen times) |
| `gpg_recv` | Received public keys |

## API Reference

See `components/gpg/gpg.h` for the complete C API.

### Key Functions

```c
// Initialization
bool gpg_init(void);
bool gpg_is_initialized(void);
bool gpg_get_status(gpg_status_t *status);

// Key Management
bool gpg_set_pending_user_id(const char *user_id);
bool gpg_generate_key(uint8_t curve);  // CDC_CURVE_ED25519 or CDC_CURVE_P256
bool gpg_reset(void);                  // Deletes all 3 keys

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
#define FEATURE_GPG 1       // GPG Key Storage (default: enabled)
```

## Limitations

- **RSA:** Not supported (TROPIC01 limitation)
- **X25519:** Not supported for ECDH (uses P-256 instead)
- **Subkeys:** Not supported in traditional sense (uses dedicated slots)
- **Received Keys:** Max 16 keys in NVS

## Troubleshooting

### Key Generation Fails

1. Check if User-ID is set: `GPG_STATUS`
2. Check TROPIC01 status: `TR01_STATUS`
3. Check slots: `TR01_SLOTS`

### CCID Not Recognized

1. `FEATURE_GPG` enabled in feature_flags.h?
2. Reconnect USB
3. Debug: `gpg --debug ccid --card-status`

### Cross-Sign Not Working

1. Own keys must exist
2. BLE must be enabled
3. Other badge must be in range
