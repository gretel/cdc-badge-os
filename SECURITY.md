# Security Hardening Guide for CDC Badge

This document describes security measures for production deployment of the CDC Badge.

> **Status:** This is a collection of **TODOs and Best Practices**. Most security hardening steps are **not yet implemented** in the current alpha firmware.

---

## Implementation Status

| Feature | Status | Notes |
|---------|--------|-------|
| Flash Encryption | ❌ TODO | Not enabled in sdkconfig |
| Secure Boot v2 | ❌ TODO | Not enabled in sdkconfig |
| JTAG Disable | ❌ TODO | eFuse not burned |
| NVS Encryption | ❌ TODO | Not enabled in sdkconfig |
| Secure Serial Mode | ❌ TODO | `FEATURE_SECURE_SERIAL=0` |
| DEBUG_MODE disabled | ❌ TODO | Currently `DEBUG_MODE=1` |
| PIN Lockout (UI) | ✅ Done | 3 attempts, then locked |
| TROPIC01 Key Storage | ✅ Done | All keys in secure element |
| FIDO2 ClientPIN | ✅ Done | Protocol 1 & 2 supported |
| PIN Hash in TROPIC01 | ✅ Done | Salted SHA-256 |
| Custom Pairing Key | ❌ TODO | Using default `stm_cert.h` |

---

## Development vs Production

| Feature | Development (Current) | Production (Target) |
|---------|----------------------|---------------------|
| DEBUG_MODE | 1 (enabled) | **0 (disabled)** |
| JTAG | Enabled | **Disabled (eFuse)** |
| Flash Encryption | Disabled | **Enabled** |
| Secure Boot | Disabled | **Enabled** |
| NVS Encryption | Disabled | **Enabled** |
| Serial Debug | Open access | **Auth required or disabled** |
| FEATURE_SECURE_SERIAL | 0 | **1** |

---

## 1. Flash Encryption (TODO)

Protects firmware and data from being read via physical access.

### Enable Flash Encryption

```bash
# In sdkconfig or menuconfig
CONFIG_SECURE_FLASH_ENC_ENABLED=y
CONFIG_SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT=n
CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=y
```

**Warning:** Release mode is **irreversible**. The chip can only be flashed with encrypted firmware after this.

### First-Time Setup

1. Build with encryption enabled
2. Flash firmware (ESP-IDF encrypts automatically on first boot)
3. Encryption keys are generated and burned to eFuses

---

## 2. Secure Boot v2 (TODO)

Ensures only signed firmware can run on the device.

### Enable Secure Boot

```bash
CONFIG_SECURE_BOOT=y
CONFIG_SECURE_BOOT_V2_ENABLED=y
CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT=n
```

### Key Generation

```bash
# Generate signing key (KEEP THIS SAFE!)
espsecure.py generate_signing_key --version 2 secure_boot_signing_key.pem

# Store key securely - loss means no more updates possible!
```

### Signing Firmware

```bash
# Sign app binary
espsecure.py sign_data --version 2 --keyfile secure_boot_signing_key.pem \
    build/cdc_badge.bin
```

---

## 3. JTAG Disable (TODO)

Prevents debug access via JTAG pins.

### Permanent Disable (eFuse)

```bash
# WARNING: IRREVERSIBLE!
espefuse.py burn_efuse JTAG_DISABLE
```

### Verify

```bash
espefuse.py summary | grep JTAG
```

---

## 4. NVS Encryption (TODO)

Encrypts Non-Volatile Storage contents.

### Enable NVS Encryption

```bash
CONFIG_NVS_ENCRYPTION=y
```

**Note:** Requires Flash Encryption to be enabled first.

---

## 5. TROPIC01 Security (Partially Implemented)

The TROPIC01 secure element provides hardware-based key protection.

### What's Implemented ✅

- All FIDO2/TOTP private keys stored in TROPIC01 (never in ESP32 flash)
- P-256 key generation on-chip
- PIN hash stored in R-Memory slot 30
- Session-based access control

### Pairing Key Protection (TODO)

- **Current:** Using default pairing key from `stm_cert.h`
- **Production:** Must replace with custom pairing certificate

### Custom Pairing Certificate (TODO)

#### Option 1: Unencrypted (Development)

```bash
# Generate and convert directly
openssl ecparam -genkey -name prime256v1 -out tropic_pairing.key
xxd -i tropic_pairing.key > custom_cert.h
```

#### Option 2: Password-Protected (Production)

The pairing key is stored **encrypted** in firmware. User must enter a password at runtime to decrypt.

1. **Generate and Encrypt Certificate**
   ```bash
   # Generate key pair
   openssl ecparam -genkey -name prime256v1 -out tropic_pairing.key

   # Encrypt with AES-256 (will prompt for password)
   openssl enc -aes-256-cbc -salt -pbkdf2 -in tropic_pairing.key \
       -out tropic_pairing.key.enc

   # Convert encrypted blob to C header
   xxd -i tropic_pairing.key.enc > encrypted_cert.h
   ```

2. **Runtime Decryption Flow**
   ```
   Boot → User enters password via Serial/PIN pad
       → Firmware decrypts pairing key in RAM
       → TROPIC01 session established
       → Decrypted key zeroed from RAM after use
   ```

**Warning:** Once a pairing key is written to TROPIC01, it cannot be changed.

### Key Slot Allocation

| Slot | Purpose | Protection |
|------|---------|------------|
| ECC 0-26 | FIDO2 Credentials | PIN required for signing |
| ECC 27-29 | Reserved (SSH) | Not used |
| ECC 30 | FIDO2 Attestation | Device-bound |
| ECC 31 | CA Root Key | PIN required |
| R-Memory 30 | PIN Hash | TROPIC01 protected |
| R-Memory 33-132 | TOTP Secrets | TROPIC01 protected |

---

## 6. PIN Security (Partially Implemented)

### What's Implemented ✅

- 4-6 digit PIN
- Salted SHA-256 hash stored in TROPIC01 R-Memory
- Salt derived from ESP32 MAC address
- UI lockout after 3 failed attempts
- FIDO2 ClientPIN Protocol 1 & 2

### What's Missing (TODO)

- **Persistent lockout:** Currently only UI-level, reboot clears counter
- **Exponential backoff:** No delay between attempts
- **Hardware-enforced lockout:** TROPIC01 could enforce this

### Recommendations for Production

- Implement persistent failed attempt counter in NVS
- Add exponential backoff (e.g., 30s after 3 fails, 5min after 5 fails)
- Consider TROPIC01 monotonic counter for hardware-enforced lockout

---

## 7. Serial Interface Security (TODO)

### Current State

`FEATURE_SECURE_SERIAL=0` - All commands accessible without authentication.

### Secure Serial Mode

When `FEATURE_SECURE_SERIAL=1`:
- Read-only commands work without auth (STATUS, HELP, GET_TIME, etc.)
- Management commands require PIN authentication
- 5-minute session timeout

### Production Steps

1. Enable in `feature_flags.h`:
   ```c
   #define FEATURE_SECURE_SERIAL 1
   ```

2. Or disable serial interface entirely for highest security.

---

## 8. Debug Mode (TODO)

### Current State

`DEBUG_MODE=1` - Security features like lockout are disabled for development.

### Production Steps

In `feature_flags.h`:
```c
#define DEBUG_MODE 0
```

This enables:
- PIN lockout enforcement
- Reduced debug logging
- Stricter security checks

---

## 9. Firmware Update Security (TODO)

### OTA Updates

If implementing OTA updates:

```bash
CONFIG_SECURE_SIGNED_ON_UPDATE=y
CONFIG_SECURE_SIGNED_ON_UPDATE_NO_SECURE_BOOT=n
```

- All OTA images must be signed
- Rollback protection recommended
- Version checking prevents downgrade attacks

### USB/Serial Updates

- Disable serial flashing in production
- Or require physical button press to enable

---

## 10. Production Checklist

Before deploying to production:

### ESP32-S3 Hardening
- [ ] Set `DEBUG_MODE=0` in feature_flags.h
- [ ] Set `FEATURE_SECURE_SERIAL=1` in feature_flags.h
- [ ] Enable Flash Encryption (Release mode) in sdkconfig
- [ ] Enable Secure Boot v2 in sdkconfig
- [ ] Enable NVS Encryption in sdkconfig
- [ ] Burn JTAG_DISABLE eFuse
- [ ] Reduce log level to WARN or ERROR
- [ ] Backup signing key securely

### TROPIC01 Hardening
- [ ] Replace default pairing certificate
- [ ] Test pairing key before production deployment
- [ ] Document pairing key recovery procedure

### Operational Security
- [ ] Change default PIN (1234)
- [ ] Implement persistent PIN lockout
- [ ] Disable or secure serial console
- [ ] Test all security features

---

## 11. Known Limitations

### Cannot Be Mitigated by Software

| Risk | Mitigation |
|------|------------|
| Physical chip decapping | Tamper-evident enclosure |
| Side-channel attacks | TROPIC01 handles sensitive ops |
| Glitching attacks | Enable fault injection detection |
| Cold boot attacks | Minimize secrets in RAM |

### Hardware Limitations

| Feature | Status | Rationale |
|---------|--------|-----------|
| USB wake from deep sleep | Not supported | ESP32-S3 limitation |
| RSA certificates | Not supported | TROPIC01 only supports ECC |
| Key backup/export | Not possible | TROPIC01 keys are non-exportable |

---

## References

- [ESP-IDF Security Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/index.html)
- [TROPIC01 Documentation](https://github.com/tropicsquare/libtropic)
- [FIDO2 Security Considerations](https://fidoalliance.org/specs/fido-v2.1-ps-20210615/fido-client-to-authenticator-protocol-v2.1-ps-20210615.html#security-considerations)
