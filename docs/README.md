# CDC Badge OS Documentation

Documentation for the CDC Badge v1.0/v1.1 hardware security key firmware.

## Quick Links

| Document | Description |
|----------|-------------|
| [Getting Started](#getting-started) | Build, flash, and first steps |
| [Module Development](MODULE_DEVELOPMENT.md) | Create custom modules |
| [Serial Commands](SERIAL_COMMANDS.md) | Command reference |
| [UI Flows](UI_FLOWS.md) | User interface navigation |

## Overview

CDC Badge OS is modular firmware for a hardware security key based on:

- **ESP32-S3** with 16MB flash and PSRAM
- **TROPIC01** secure element for key storage
- **E-Paper display** (296x128) with 12-button keypad
- **USB-C** for CDC serial, CCID smartcard, and HID
- **Bluetooth LE** for vCard exchange and HID keyboard

### Features

| Module | Description | Status |
|--------|-------------|--------|
| **FIDO2/WebAuthn** | Passwordless authentication | Working |
| **TOTP** | Time-based one-time passwords | Working |
| **Passwords** | Encrypted password vault | Working |
| **GPG** | OpenPGP smartcard (CCID) | WIP |
| **BLE vCard** | Contact exchange between badges | Working |
| **BLE HID** | Bluetooth keyboard for auto-type | Working |

## Getting Started

### Prerequisites

- CDC Badge v1.0/v1.1 hardware
- USB-C cable

### Flash Pre-built Firmware

No build environment needed - flash a release directly.

**Web Flasher:** [CDC Badge Web Flasher](https://krim404.github.io/cdc-badge-os/) (Chrome/Edge with Web Serial)

**Python Flash Tool:**

```bash
pip install -r tools/requirements.txt
python tools/flash_firmware.py --release latest
```

See `python tools/flash_firmware.py --help` for all options (specific versions, local files, NVS erase).

### Build from Source

Requires [PlatformIO](https://platformio.org/) with ESP-IDF framework.

```bash
# Initialize submodules (first time only)
git submodule update --init --recursive

# Build firmware
~/.platformio/penv/bin/pio run

# Build and flash
~/.platformio/penv/bin/pio run -t upload

# Monitor serial output
~/.platformio/penv/bin/pio device monitor
```

### First Boot

1. Connect badge via USB-C
2. Default PIN: `123456` (change immediately!)
3. Press **Y** on lock screen to unlock
4. Navigate with **1-9** keys, confirm with **Y**, back with **N**

## Architecture

### Component Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                         Modules                                  │
│  mod_fido2 │ mod_totp │ mod_password │ mod_gpg │ mod_vcard │ ...│
├─────────────────────────────────────────────────────────────────┤
│                      OS UI (cdc_os_ui)                          │
│          Lock Screen │ Settings │ WiFi/BLE Menus                │
├─────────────────────────────────────────────────────────────────┤
│                    UI Framework (cdc_ui + cdc_views)            │
│        ViewStack │ ListView │ T9Input │ Toast │ Confirm │ ...   │
├─────────────────────────────────────────────────────────────────┤
│                      Core Services (cdc_core)                   │
│   ServiceRegistry │ EventBus │ ModuleRegistry │ TropicStorage   │
├─────────────────────────────────────────────────────────────────┤
│                   HAL (cdc_hal)                                 │
│   Display │ Keypad │ SecureElement │ Power │ WiFi │ Bluetooth   │
├─────────────────────────────────────────────────────────────────┤
│                      ESP-IDF / FreeRTOS                         │
└─────────────────────────────────────────────────────────────────┘
```

### Secure Storage (TROPIC01)

Private keys and secrets are stored in the TROPIC01 secure element:

| Resource | Capacity | Usage |
|----------|----------|-------|
| ECC Slots | 32 (0-31) | Cryptographic keys |
| R-Memory Slots | 512 (0-511) | Encrypted data (422 bytes payload each, 444-byte slot incl. 22-byte header) |

Slot allocation is defined in `main/tropic_slot_map.h`. See [Module Development](MODULE_DEVELOPMENT.md) for details.

## Tools

Python tools under `tools/`. Install all dependencies at once:

```bash
pip install -r tools/requirements.txt
```

| Tool | Description |
|------|-------------|
| `flash_firmware.py` | Flash pre-built firmware from GitHub releases or local files |
| `ble_serial.py` | BLE serial console via Nordic UART Service |
| `coredump.py` | Read and analyze ESP32 core dumps from flash |

## Documentation Index

### User Guides

- [UI Flows](UI_FLOWS.md) - Navigation and interface reference
- [Serial Commands](SERIAL_COMMANDS.md) - USB serial command reference

### Developer Guides

- [Module Development](MODULE_DEVELOPMENT.md) - Creating custom modules
- [GPG Implementation](GPG.md) - GPG/OpenPGP smartcard details

### Protocol Specifications

- [BLE vCard Protocol](ble_vcard_protocol.md) - Badge-to-badge contact exchange
- [GPG Cross-Signing](CROSS_SIGNING.md) - Badge-to-badge key signing

### Implementation Plans

Historical implementation plans are archived in [plans/](plans/). These documents reflect the design process and may not match the current implementation.

## Contributing

- All code and documentation in **English**
- Follow existing patterns in the codebase
- Use `cdc_log` for logging (never `ESP_LOG` directly)
- See [Module Development](MODULE_DEVELOPMENT.md) for architecture guidelines

## License

See repository root for license information.
