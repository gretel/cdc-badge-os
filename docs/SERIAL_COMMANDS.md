# Serial Commands Reference

USB serial command interface for the CDC Badge.

Connect via USB CDC at **115200 baud**. Use `HELP` to list all available commands.

Commands marked with `[AUTH]` require authentication when `FEATURE_SECURE_SERIAL` is enabled.

## Authentication

| Command | Description |
|---------|-------------|
| `AUTH <pin>` | Authenticate with PIN |
| `LOGOUT` | End authenticated session |

## System

| Command | Description |
|---------|-------------|
| `HELP` | Show available commands |
| `PING` | Check if device is responsive (returns PONG) |
| `STATUS` | Show system status |
| `MEM` | Show memory usage (Heap/PSRAM/NVS) |
| `ERROR_LOG` | Show error log |
| `ERROR_LOG CLEAR` | Clear error log |
| `REBOOT` | Restart the device `[AUTH]` |

## Time

| Command | Description |
|---------|-------------|
| `GET_TIME` | Show current time |
| `GET_DATE` | Show current date |
| `SET_TIME HH:MM:SS` | Set time |
| `SET_DATE DD.MM.YYYY` | Set date |
| `SET_DATE <timestamp>` | Set date from Unix timestamp |

## Display

| Command | Description |
|---------|-------------|
| `SET_NAME <text>` | Set display name (lock screen) |
| `SET_INFO <text>` | Set info line 1 |
| `SET_INFO2 <text>` | Set info line 2 |

## NVS (Non-Volatile Storage)

| Command | Description |
|---------|-------------|
| `NVS_LIST [namespace]` | List NVS entries |
| `NVS_READ <ns> <key>` | Read NVS key value |
| `NVS_DEL <ns> [key]` | Delete NVS key or namespace `[AUTH]` |
| `NVS_CLEAR YES` | Erase entire NVS `[AUTH]` |

## PIN Management

| Command | Description |
|---------|-------------|
| `PIN_STATUS` | Show PIN status and retry counts |
| `PIN_RESET` | Reset PIN retries (debug only) |

## TROPIC01 Secure Element

| Command | Description |
|---------|-------------|
| `TR01_STATUS` | Show TR01 connection status |
| `TR01_INFO` | Show TR01 chip info (ID, firmware) |
| `TR01_SESSION` | Start/restart TR01 session |
| `TR01_SLOTS` | Show slot usage summary |
| `TR01_RMEM_READ <slot>` | Read and dump R-Memory slot |
| `TR01_ECC_DEL <slot>` | Delete ECC key slot `[AUTH]` |
| `TR01_RMEM_DEL <slot>` | Delete R-Memory slot `[AUTH]` |
| `TR01_RESYNC` | Resync TR01 session and cache |
| `TR01_CACHE_REBUILD` | Rebuild TR01 cache from chip |
| `TR01_CLEANUP` | Cleanup mismatched slots + rebuild cache `[AUTH]` |
| `TR01_WIPE CONFIRM` | Factory reset all TR01 data `[AUTH]` |

## TOTP Module

| Command | Description |
|---------|-------------|
| `TOTP_LIST` | List all TOTP accounts `[AUTH]` |
| `TOTP_ADD <name> <secret> [issuer] [digits] [period]` | Add TOTP account `[AUTH]` |
| `TOTP_DEL <index>` | Delete TOTP account by index `[AUTH]` |
| `TOTP_GET <index>` | Generate TOTP code by index `[AUTH]` |

**TOTP_ADD Parameters:**
- `name` - Account name (required)
- `secret` - Base32 encoded secret (required)
- `issuer` - Issuer name (optional)
- `digits` - Code length: 6, 7, or 8 (default: 6)
- `period` - Time period in seconds (default: 30)

## Password Module

| Command | Description |
|---------|-------------|
| `PASSWORD_LIST` | List password entries `[AUTH]` |
| `PASSWORD_GET <index>` | Get password entry details `[AUTH]` |
| `PASSWORD_ADD <name> <user> <url> <password>` | Add password entry `[AUTH]` |
| `PASSWORD_DEL <index>` | Delete password entry `[AUTH]` |

## GPG Module

| Command | Description |
|---------|-------------|
| `GPG_STATUS` | Show GPG key status `[AUTH]` |
| `GPG_GENERATE <curve> <user_id>` | Generate GPG keys (1=Ed25519, 2=P-256) `[AUTH]` |
| `GPG_EXPORT` | Export public keys `[AUTH]` |
| `GPG_RESET` | Two-step destructive reset of all GPG keys (`GPG_RESET` prints a token; confirm within 30 s via `GPG_RESET <token>`) `[AUTH]` |

## Examples

```bash
# Set time from Unix timestamp
echo "SET_DATE $(date +%s)" > /dev/ttyACM0

# Add TOTP account
echo "TOTP_ADD GitHub JBSWY3DPEHPK3PXP" > /dev/ttyACM0

# Add TOTP with all options
echo "TOTP_ADD AWS HXDMVJECJJWSRB3HWIZR4IFUGFTMXBOZ Amazon 6 30" > /dev/ttyACM0

# Show memory usage
echo "MEM" > /dev/ttyACM0

# Factory reset (dangerous!)
echo "TR01_WIPE CONFIRM" > /dev/ttyACM0
```
