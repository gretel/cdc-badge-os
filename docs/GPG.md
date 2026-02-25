# GPG Module

OpenPGP smartcard functionality for the CDC Badge using the TROPIC01 secure element.

## Overview

| Feature | Description |
|---------|-------------|
| **Key Storage** | Private keys in TROPIC01 ECC slots 1-3 |
| **Key Types** | Signature (SIG), Decryption (DEC), Authentication (AUT) |
| **Supported Curves** | Ed25519 (SIG/AUT), P-256 ECDH (DEC), P-256 ECDSA (all) |
| **USB CCID** | OpenPGP 3.4 smartcard interface |
| **Cross-Signing** | Key exchange with other badges via BLE |

## Quick Start

### Generate Keys via Menu

1. Main Menu → **GPG**
2. Select **Generate Key**
3. Enter name (T9 keyboard)
4. Enter email
5. Select curve (Ed25519 recommended)
6. Wait for key generation

### Generate Keys via Serial

```bash
# Generate with Ed25519 (SIG/AUT) + P-256 (DEC)
echo "GPG_GENERATE 1 Max Mustermann <max@example.com>" > /dev/ttyACM0

# Generate with P-256 for all keys
echo "GPG_GENERATE 2 Max Mustermann <max@example.com>" > /dev/ttyACM0

# Check status
echo "GPG_STATUS" > /dev/ttyACM0
```

**Note:** `GPG_GENERATE` creates three keys:
- **SIG** (Slot 1): Signature - Ed25519 or P-256 ECDSA
- **DEC** (Slot 2): Decryption - Always P-256 ECDH
- **AUT** (Slot 3): Authentication - Ed25519 or P-256 ECDSA

## Using with GnuPG

The badge functions as an OpenPGP 3.4 smartcard via USB CCID.

### Card Detection

```bash
# Detect card
gpg --card-status

# Fetch public key from card
gpg --card-edit
> fetch

# Verify key import
gpg --list-keys

# Test signature
echo "test" | gpg --sign --armor | gpg --verify
```

### Card Identification

| Property | Value |
|----------|-------|
| Manufacturer ID | "CD" (0x4344) |
| Serial Number | Derived from ESP32 MAC address |
| VID/PID | 0x08E6:0x4433 (Gemalto compatible) |

The Gemalto VID/PID bypasses libccid whitelist requirements.

## Serial Commands

| Command | Description |
|---------|-------------|
| `GPG_STATUS` | Show key status (User-ID, fingerprints, curves) |
| `GPG_GENERATE <curve> <user_id>` | Generate keys (1=Ed25519, 2=P-256) |
| `GPG_EXPORT` | Export public keys as PEM |
| `GPG_RESET` | Delete all keys (requires CONFIRM) |
| `GPG_RECV_LIST` | List received cross-signing keys |
| `GPG_RECV_INFO <index>` | Show received key details |
| `GPG_CROSS_SIGN <index>` | Sign a received key |
| `GPG_RECV_DELETE <index>` | Delete a received key |

## Storage

### TROPIC01 Allocation

| Slot | Type | Usage |
|------|------|-------|
| 1 | ECC | Signature Key (SIG) |
| 2 | ECC | Decryption Key (DEC) |
| 3 | ECC | Authentication Key (AUT) |

### Metadata Storage

| Location | Usage |
|----------|-------|
| NVS `openpgp` | Card state, fingerprints, generation times |
| NVS `gpg_recv` | Received public keys (max 16) |

## Cross-Signing (Badge-to-Badge)

Exchange and sign GPG public keys with other badges via BLE.

### Workflow

1. **Send Key**: GPG Menu → Send Key → Select peer badge
2. **Receive Keys**: GPG Menu → Received Keys → View list
3. **Sign Key**: Select key → Sign → Verify fingerprint → Confirm

### Signature Format

```
SHA256(fingerprint || user_id)
```

The signature is created using your SIG key. See [Cross-Signing Protocol](CROSS_SIGNING.md) for technical details.

## Security Model

### Hardware-Protected Keys (SIG/AUT)

Signature and authentication keys are stored in TROPIC01 ECC slots. Private key material **never leaves the secure element**.

| Aspect | SIG/AUT Keys |
|--------|--------------|
| Private Key Location | TROPIC01 hardware |
| Crypto Operations | Hardware accelerated |
| Key in RAM | Never |
| Security Level | High |

### Software ECDH (DEC Key)

The TROPIC01 does **not support native ECDH**. For GPG decryption, the DEC private key uses a hybrid approach:

| Aspect | DEC Key |
|--------|---------|
| Private Key Location | R-Memory (AES-256-GCM encrypted) |
| Crypto Operations | Software (MbedTLS) |
| Key in RAM | Temporary (~10ms during ECDH) |
| Security Level | Medium |

### DEC Key Protection

The decryption key is protected by:

1. **AES-256-GCM encryption** at rest in R-Memory
2. **Device-specific key** derived via HKDF-SHA256 from TROPIC01 Chip ID
3. **PIN verification** required before any operation
4. **Immediate RAM clearing** after ECDH computation

### Security Recommendations

**For high-security applications:**
- Use only signature functions (PSO:CDS)
- Perform encryption with a separate system
- The badge provides strong authentication via hardware-backed signatures

**For standard use:**
- ECDH implementation is acceptably secure
- Comparable to software-only GPG implementations
- PIN protection provides access control

### Security Checklist

Before deploying GPG decryption:
- [ ] Enable Secure Boot on ESP32
- [ ] Control physical access to device
- [ ] Configure strong PIN
- [ ] Understand DEC key has different security model

## Limitations

| Limitation | Reason |
|------------|--------|
| No RSA support | TROPIC01 hardware limitation |
| No X25519 for ECDH | Uses P-256 instead |
| No traditional subkeys | Uses dedicated slots |
| Max 16 received keys | NVS storage limit |
| DEC key in software | TROPIC01 has no ECDH API |

## Troubleshooting

### Key Generation Fails

1. Check User-ID is set: `GPG_STATUS`
2. Check TROPIC01 status: `TR01_STATUS`
3. Check slot availability: `TR01_SLOTS`

### CCID Not Recognized

1. Verify GPG module is enabled in build
2. Reconnect USB cable
3. Debug with: `gpg --debug ccid --card-status`

### Cross-Sign Not Working

1. Own keys must exist
2. BLE must be enabled
3. Other badge must be in range and discoverable
