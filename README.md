# CDC Badge OS - Developer Demonstrator

Demonstrator firmware for the CDC Badge v1.0, base for CDCBos

## Hardware

For hardware details, schematics and PCB design see: https://github.com/riatlabs/cdc-badge

| Component | Description |
|-----------|-------------|
| MCU | ESP32-S3-WROOM-1 |
| Display | GDEY029T94-FL03 (2.9" E-Paper with Frontlight) |
| Secure Element | TROPIC01 |
| I/O Expander | TCA9535 (12-Button Keypad) |
| Power | BQ25895 (LiPo Charger) |

## Features Demonstrated

### Power Management Best Practices

- **Light Sleep Mode**: Device enters light sleep on lock screen, waking every 25s for clock updates
- **Deep Sleep Mode**: Hold N for 5s on lock screen - display cleared, only Y key wakes device
- **Shipping Mode**: Hold BOOT button for 3s - battery disconnected, minimal power draw
- **GPIO Wakeup**: Instant wake on keypad press via I/O expander interrupt
- **PWM Backlight**: Configurable brightness (0-1023) with NVS persistence
- **WiFi Power Control**: WiFi disabled by default, only enabled when needed
- **Charging Safety**: BQ25895 configured for safe 512mA charging (vs. 2048mA default)

### Async Display Rendering

- Background FreeRTOS task handles e-paper updates
- UI remains responsive during slow e-paper refresh cycles
- Partial refresh support for faster updates

### Async Keypad Input

- Dedicated keypad task at high priority
- Key presses buffered via queue (e.g., enter full PIN without display wait)
- Debouncing handled in task

### Secure Element Integration

- TROPIC01 chip for secure key storage
- PSA Crypto API abstraction
- PIN storage in secure memory

### NVS Persistence

- Backlight level saved to NVS
- Badge name/info text stored persistently
- RTC time backed up to NVS for power loss recovery

## Implemented Views

Reusable UI components in `components/cdc_badge/views.h`:

| View | Description |
|------|-------------|
| **Lock Screen** | Status display with battery, clock, 3 editable text lines, lock icon |
| **PIN Entry** | Secure PIN input with masked digits, attempt counter, lockout |
| **List Screen** | Selectable menu with scrolling, max 4 visible items |
| **Info Screen** | Scrollable long text display for help/info content |
| **T9 Input** | Text input with T9 multi-tap (phone-style), cursor timeout |
| **Slider** | Value adjustment with visual bar (e.g., brightness) |
| **Toast** | Temporary overlay messages (success/error/info) |

All views support partial refresh for fast updates.

## Build & Flash

Requires PlatformIO with ESP-IDF framework.

```bash
# Initialize submodules (CalEPD, libtropic, Adafruit-GFX)
git submodule update --init --recursive

# Build
pio run

# Flash
pio run -t upload

# Monitor serial output
pio device monitor
```

## Serial Commands

Connect at 115200 baud:

| Command | Description |
|---------|-------------|
| `SET_TIME HH:MM:SS` | Set current time |
| `SET_DATE YYYY-MM-DD` | Set current date |
| `SET_DATE <timestamp>` | Set from Unix timestamp |
| `SET_NAME text` | Set name line on display |
| `SET_INFO text` | Set info line on display |
| `SET_INFO2 text` | Set info2 line on display |
| `GET_TIME` | Get current time |
| `GET_DATE` | Get current date |
| `HELP` | Show available commands |

### Quick Time Sync

Set time from host system in one command:

```bash
echo "SET_DATE $(date +%s)" > /dev/ttyACM0
```

## License

This project is licensed under the GNU General Public License v3.0 - see [LICENSE.md](LICENSE.md) for details.

---

*This firmware was co-developed with [Claude Code](https://claude.ai/code) by Anthropic.*
