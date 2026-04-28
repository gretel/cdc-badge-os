---
title: "[LOW] Serial commands examples section incomplete"
severity: LOW
domain: api-design/api-documentation
lens: serial-command-api
labels:
  - "audit:api-design/api-documentation"
---

## Summary
The `docs/SERIAL_COMMANDS.md` has an Examples section (lines 118-134) but it only covers:
- SET_TIME
- TOTP_ADD (2 examples)
- MEM
- TR01_WIPE

Missing examples for:
- Authentication (AUTH, LOGOUT)
- GPG commands
- Password commands
- NVS commands
- Display commands
- Error handling examples

## Impact
- Users must guess syntax for many commands
- Common workflows (like setting up the badge) not demonstrated
- Harder to learn the command interface
- Inconsistent with other documentation sections that have examples

## Evidence
**Current Examples section (docs/SERIAL_COMMANDS.md:118-134):**
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

**Commands without examples:**
- `AUTH <pin>` - Most important for new users
- `GPG_STATUS`, `GPG_GENERATE` - Key module
- `PASSWORD_LIST`, `PASSWORD_ADD` - Common use case
- `NVS_LIST`, `NVS_READ` - Debugging
- `GET_TIME`, `SET_TIME` - Basic setup

## Recommended Fix
Expand the Examples section with common workflows:

```markdown
## Examples

### Basic Setup
```bash
# Authenticate
echo "AUTH 1234" > /dev/ttyACM0

# Set time and date
echo "SET_TIME 14:30:00" > /dev/ttyACM0
echo "SET_DATE 25.04.2026" > /dev/ttyACM0

# Set display name
echo "SET_NAME John Doe" > /dev/ttyACM0
```

### TOTP Setup
```bash
# Add TOTP account
echo "TOTP_ADD GitHub JBSWY3DPEHPK3PXP" > /dev/ttyACM0

# Generate code
echo "TOTP_GET 0" > /dev/ttyACM0
```

### GPG Setup
```bash
# Generate keys
echo "GPG_GENERATE 1 John Doe <john@example.com>" > /dev/ttyACM0

# Export public key
echo "GPG_EXPORT" > /dev/ttyACM0

# Check status
echo "GPG_STATUS" > /dev/ttyACM0
```

### Password Vault
```bash
# Add password entry
echo "PASSWORD_ADD GitHub jdoe MySecret123 https://github.com" > /dev/ttyACM0

# List entries
echo "PASSWORD_LIST" > /dev/ttyACM0

# Get details
echo "PASSWORD_GET 0" > /dev/ttyACM0
```

### Debugging
```bash
# Check memory
echo "MEM" > /dev/ttyACM0

# List NVS entries
echo "NVS_LIST" > /dev/ttyACM0

# Read specific key
echo "NVS_READ nvs_key name" > /dev/ttyACM0

# Show error log
echo "ERROR_LOG" > /dev/ttyACM0
```

### Session Management
```bash
# Check status
echo "STATUS" > /dev/ttyACM0

# Re-authenticate if session expired
echo "AUTH 1234" > /dev/ttyACM0

# Explicitly logout
echo "LOGOUT" > /dev/ttyACM0
```
```

## References
- docs/SERIAL_COMMANDS.md:118-134 (current examples)
- docs/SERIAL_COMMANDS.md:7-111 (all commands that need examples)
