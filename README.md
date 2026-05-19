# CDC Badge OS

Modular firmware for the CDC Badge v1.0/v1.1 hardware security key featuring TROPIC01 secure element.

![CDC Badge Demo](docs/demo.jpg)

> **Early Alpha** - This firmware is in active development and **not production ready**. Security hardening is incomplete. Do not use for protecting critical accounts. See [SECURITY.md](SECURITY.md) for hardening steps required before production use.

> **Rewrite** - This is a complete rewrite of the original firmware to create a cleaner, more maintainable codebase with modular architecture.

## Features

| Feature | Status | Description |
|---------|--------|-------------|
| **FIDO2/WebAuthn** | Working | FIDO 2.1 passwordless authentication via USB HID |
| **SSH Hardware Keys** | Working | Native SSH via ed25519-sk (OpenSSH 8.2+) |
| **U2F** | Working | Legacy two-factor authentication |
| **TOTP Authenticator** | Working | Time-based OTP (100 accounts, Google Authenticator compatible) |
| **Password Vault** | Working | Secure password storage (353 entries) |
| **GPG/CCID** | WIP | OpenPGP smartcard via USB CCID with TROPIC01 key storage |
| **BLE vCard** | WIP | Badge-to-badge contact exchange via BLE |
| **BLE HID** | WIP | Bluetooth keyboard for auto-type |
| **WiFi + NTP** | Working | Time synchronization over WiFi |
| **BLE Serial** | WIP | Bluetooth serial console (Nordic UART Service) |
| **SAO Detection** | Working | Shitty Add-On port detection and info |
| **E-Paper Display** | Working | 2.9" low-power display with backlight |
| **12-Button Keypad** | Working | Phone-style T9 input |
| **Multi-Language** | Working | English and German UI |
| **Secure Serial** | Working | PIN authentication for serial commands |

### Planned

- [ ] Certificate Authority (CA) module
- [ ] TROPIC01 firmware updater
- [ ] Home Assistant controller (`mod_homeassistant`): keypad-driven quick-action remote with favorites, WiFi on-demand, token stored in TROPIC01 R-Memory

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
  mod_gpg/        OpenPGP smartcard (CCID) module
  mod_vcard/      BLE vCard exchange (Badge2Badge)
  mod_hid/        BLE HID keyboard for auto-type
  mod_ble_serial/ BLE Serial console (Nordic UART Service)
  mod_sao/        SAO port detection
  mod_nvsedit/    NVS editor (privileged)
```

Modules are self-contained and can be enabled/disabled in `main/CMakeLists.txt`.

See [Module Development Guide](docs/MODULE_DEVELOPMENT.md) for creating new modules.

## Security Architecture

| Feature | Implementation |
|---------|----------------|
| **Key Storage** | All private keys in TROPIC01 secure element |
| **Key Generation** | P-256 and Ed25519 generated on-chip, never exported |
| **PIN Protection** | 4-8 digit PIN with 3 attempt lockout |
| **FIDO2 ClientPIN** | Protocol 2 with HKDF-SHA256 |
| **Attestation** | Self-signed (device-unique AAGUID) |

### PIN Lockout

The device uses a multi-PIN system with brute-force protection:

| PIN | Purpose | Max Retries | Lockout |
|-----|---------|-------------|---------|
| Badge PIN | Device unlock, serial auth | 3 | 60 seconds |
| PW1 | FIDO2/GPG user operations | 3 | 60 seconds |
| PW3 | GPG admin operations | 3 | 60 seconds |

After 3 failed attempts, the device locks for 60 seconds. Retries reset after successful authentication.

**Note:** `DEBUG_MODE=1` disables lockouts for development. Set to 0 for production!

### TROPIC01 Secure Element

- 32 ECC key slots (P-256 and Ed25519)
- 512 R-Memory slots (422 bytes payload each)
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

### Flash Pre-built Firmware

No build environment needed. Flash a release directly to the badge.

**Option A: Web Flasher (easiest)**

Use the browser-based flasher at [CDC Badge Web Flasher](https://krim404.github.io/cdc-badge-os/) - requires Chrome/Edge with Web Serial support.

**Option B: Python Flash Tool**

```bash
# Install dependencies
pip install -r tools/requirements.txt

# Flash the latest release from GitHub
python tools/flash_firmware.py --release latest

# Flash a specific version
python tools/flash_firmware.py --release v0.4.1

# Flash from a local directory
python tools/flash_firmware.py --dir ./artifacts/

# Specify port manually (auto-detected by default)
python tools/flash_firmware.py --release latest --port /dev/cu.usbmodem1101

# Erase all settings (NVS) after flashing
python tools/flash_firmware.py --release latest --erase-nvs
```

If the device is not detected, hold **BOOT** while pressing **RESET** to enter download mode.

### Build from Source

Requires [PlatformIO](https://platformio.org/) with ESP-IDF framework.

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

### Compile-Time Flags

Feature flags in `components/cdc_core/include/cdc_core/feature_flags.h`:

| Flag | Default | Description |
|------|---------|-------------|
| `DEBUG_MODE` | 1 | Disables PIN lockouts and increases log verbosity. **Set to 0 for production!** |
| `FEATURE_SECURE_SERIAL` | 0 | Require PIN authentication for serial commands |
| `FEATURE_NVS_EDIT` | 0 | Enable NVS delete operations in the privileged NVS editor |

Set via build flags in `platformio.ini`:
```ini
build_flags =
    -DDEBUG_MODE=0
    -DFEATURE_SECURE_SERIAL=1
    -DFEATURE_NVS_EDIT=1
```

Or via Kconfig menuconfig:
```bash
~/.platformio/penv/bin/pio run -t menuconfig
```

### First-Time Setup

1. **Change the default PIN** (Settings -> Change PIN)
   - Default PIN: `123456`
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

Connect at 115200 baud via USB CDC. Type `HELP` to list all available commands.

The [Web Flasher](https://krim404.github.io/cdc-badge-os/) also provides a serial console for configuration.

See [Serial Commands Reference](docs/SERIAL_COMMANDS.md) for the full command list.

## Power Management

| Mode | Trigger | Wake |
|------|---------|------|
| Active | Normal use | - |
| Light Sleep | Lock screen idle | Any key except [3] (Menu) |
| Deep Sleep | Hold N 5s on lock | Any key (reset) |
| Shipping | Hold BOOT 3s | USB power |

## License

GNU General Public License v3.0 - see [LICENSE.md](LICENSE.md)

---

## Disclaimer

*Co-developed with [Claude Code](https://claude.ai/code) by Anthropic.*

This repository is a **proof-of-concept / demonstrator**. It may contain **serious bugs**, incomplete edge-case handling, and other "sharp edges". Do **not** use it as-is for production or security-critical deployments.

While I'm experienced with cryptography and encryption concepts, this is my first project implemented directly on the ESP32. For ESP-IDF/embedded best practices I relied heavily on external guidance and reviews. As a result, you may still find non-idiomatic ESP32 code, suboptimal design patterns, duplication, or refactoring debt.

The intent is to clean this up before the first major release (v1.0.0), once I have more routine in ESP32 development and can consolidate patterns, structure, and implementation details specific for this device.
