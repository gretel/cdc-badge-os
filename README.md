# CDC Badge OS

Modular firmware for the CDC Badge v1.0 hardware security key featuring TROPIC01 secure element.

![CDC Badge Demo](docs/demo.jpg)

> **Early Alpha** - This firmware is in active development and **not production ready**. Security hardening is incomplete. Do not use for protecting critical accounts. See [SECURITY.md](SECURITY.md) for hardening steps required before production use.

## Features

| Feature | Status | Description |
|---------|--------|-------------|
| **FIDO2/WebAuthn** | Working | FIDO 2.1 passwordless authentication via USB HID |
| **SSH Hardware Keys** | Working | Native SSH via ed25519-sk (OpenSSH 8.2+) |
| **U2F** | Working | Legacy two-factor authentication |
| **TOTP Authenticator** | Working | Time-based OTP (100 accounts, Google Authenticator compatible) |
| **Password Vault** | Working | Secure password storage (362 entries) |
| **GPG Key Management** | WIP | GPG key storage via TROPIC01 |
| **WiFi + NTP** | Working | Time synchronization over WiFi |
| **BLE Serial** | Basic | Bluetooth serial console (Nordic UART Service) |
| **E-Paper Display** | Working | 2.9" low-power display with backlight |
| **12-Button Keypad** | Working | Phone-style T9 input |
| **Multi-Language** | Working | English and German UI |
| **Secure Serial** | Working | PIN authentication for serial commands |

### Not Yet Ported

- Badge Mode (QR Code vCard display)
- BLE vCard (Badge2Badge exchange)
- Certificate Authority (CA)
- SAO Port detection
- USB Keyboard auto-type

## Architecture

The firmware uses a modular plugin architecture:

```
components/
  cdc_core/       Core services (EventBus, ServiceRegistry, ModuleRegistry)
  cdc_hal/        Hardware abstraction (Display, Keypad, Power, SecureElement)
  cdc_ui/         UI framework (ViewStack, I18n)
  cdc_views/      Reusable views (ListView, T9Input, PinEntry, etc.)
  cdc_os_ui/      OS-level UI (LockScreen, Settings, Sleep)
  usb_badge/      USB CDC/HID composite device
  serial_cmd/     Serial command interface

  mod_fido2/      FIDO2/WebAuthn/U2F module
  mod_totp/       TOTP authenticator module
  mod_password/   Password vault module
  mod_gpg/        GPG key management module
```

Modules are self-contained and can be enabled/disabled in `main/CMakeLists.txt`.

See [Module Development Guide](docs/MODULE_DEVELOPMENT.md) for creating new modules.

## Security Architecture

| Feature | Implementation |
|---------|----------------|
| **Key Storage** | All private keys in TROPIC01 secure element |
| **Key Generation** | P-256 and Ed25519 generated on-chip, never exported |
| **PIN Protection** | 4-6 digit PIN with 3 attempt lockout |
| **FIDO2 ClientPIN** | Protocol 2 with HKDF-SHA256 |
| **Attestation** | Self-signed (device-unique AAGUID) |

### TROPIC01 Secure Element

- 32 ECC key slots (P-256 and Ed25519)
- 512 R-Memory slots (444 bytes each)
- Hardware random number generator
- Tamper-resistant key storage
- Keys cannot be extracted or cloned

See [Module Development Guide](docs/MODULE_DEVELOPMENT.md) for the storage map.

## Hardware

| Component | Model |
|-----------|-------|
| MCU | ESP32-S3-WROOM-1 (16MB Flash, PSRAM) |
| Display | GDEY029T94-FL03 (2.9" E-Paper + Frontlight) |
| Secure Element | TROPIC01 |
| I/O Expander | TCA9535 (Keypad) |
| Power IC | BQ25895 (LiPo Charger) |

Schematics and PCB: https://github.com/riatlabs/cdc-badge

## Getting Started

### Build & Flash

Requires PlatformIO with ESP-IDF framework.

```bash
# Initialize submodules
git submodule update --init --recursive

# Build
~/.platformio/penv/bin/pio run

# Flash
~/.platformio/penv/bin/pio run -t upload

# Monitor (115200 baud)
~/.platformio/penv/bin/pio device monitor
```

### First-Time Setup

1. **Change the default PIN** (Settings -> Change PIN)
   - Default PIN: `1234`
   - FIDO2 requires a non-default PIN

2. **Set the time** via WiFi NTP or serial command:
   ```bash
   echo "SET_DATE $(date +%s)" > /dev/ttyACM0
   ```

3. **Register your first WebAuthn credential** at a supported site

### Using FIDO2/WebAuthn

1. Navigate to a WebAuthn-enabled site (GitHub, Google, etc.)
2. Badge displays the site name for confirmation
3. Press **Y** to approve, **N** to deny
4. Enter PIN on badge if required

### Using SSH Hardware Keys

```bash
# Generate Ed25519-SK resident key
ssh-keygen -t ed25519-sk -O resident -O application=ssh:myserver

# Export public keys from badge
ssh-keygen -K

# Connect (badge prompts for confirmation)
ssh user@server
```

### Using TOTP

1. Add accounts via serial command or on-device
2. View codes in TOTP menu
3. Copy code manually (auto-type not yet implemented)

## Serial Commands

Connect at 115200 baud via USB CDC. Use `HELP` to list all commands.

### System
| Command | Description |
|---------|-------------|
| `HELP` | Show all commands |
| `PING` | Connection test |
| `STATUS` | Badge status |
| `MEM` | Memory usage (Heap/PSRAM/NVS) |

### Time
| Command | Description |
|---------|-------------|
| `SET_TIME HH:MM:SS` | Set time |
| `SET_DATE YYYY-MM-DD` | Set date |
| `SET_DATE <timestamp>` | Set from Unix timestamp |

### FIDO2
| Command | Description |
|---------|-------------|
| `FIDO_STATUS` | Module status |
| `FIDO_LIST` | List credentials |
| `FIDO_DEL <index>` | Delete credential |

### TOTP
| Command | Description |
|---------|-------------|
| `TOTP_LIST` | List accounts |
| `TOTP_ADD name secret [issuer] [digits] [period]` | Add account |
| `TOTP_DEL <index>` | Delete account |
| `TOTP_GET <index>` | Generate code |

### Password Vault
| Command | Description |
|---------|-------------|
| `PASS_LIST` | List entries |
| `PASS_ADD name user url password [notes]` | Add entry |
| `PASS_GET <index>` | Show entry |
| `PASS_DEL <index>` | Delete entry |

## Power Management

| Mode | Trigger | Wake |
|------|---------|------|
| Active | Normal use | - |
| Light Sleep | Lock screen idle | Any key |
| Deep Sleep | Hold N 5s on lock | Y key only |
| Shipping | Hold BOOT 3s | USB power |

## License

GNU General Public License v3.0 - see [LICENSE.md](LICENSE.md)

---

*Co-developed with [Claude Code](https://claude.ai/code) by Anthropic.*
