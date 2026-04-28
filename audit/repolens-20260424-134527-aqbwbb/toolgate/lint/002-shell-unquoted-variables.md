---
title: "[MEDIUM] Shell script uses unquoted variable expansion"
severity: MEDIUM
domain: lint
lens: toolgate/lint
labels:
  - "audit:toolgate/lint"
---

## Summary
Shell scripts use unquoted variable expansions that may cause word splitting or globbing issues when paths contain spaces.

**Files affected:**
- `tools/flash_firmware.py` (line 58): `os.listdir(directory)` - Python, but similar issue in shell scripts
- `third_party/libtropic/scripts/test_runner/openocd_test_L432KC.sh` (line 48): `PATH_TO_THIS_FOLDER=\`pwd\``
- `third_party/libtropic/scripts/test_runner/openocd_test_F439ZI.sh` (line 13): `PATH_TO_THIS_FOLDER=\`pwd\``

## Impact
Unquoted variable expansions can cause:
1. **Word splitting**: Paths with spaces break commands
2. **Globbing**: Paths with `*`, `?`, or `[` characters expand unexpectedly
3. **Silent failures**: Commands may run with wrong arguments

## Evidence
**third_party/libtropic/scripts/test_runner/openocd_test_F439ZI.sh (lines 13-14):**
```bash
PATH_TO_THIS_FOLDER=`pwd`

# Later used unquoted:
openocd -f ${PATH_TO_THIS_FOLDER}/ts11-jtag.cfg \
```

**third_party/libtropic/scripts/test_runner/openocd_test_L432KC.sh (line 48):**
```bash
PATH_TO_THIS_FOLDER=`pwd`

# Later used unquoted:
openocd -f ${PATH_TO_THIS_FOLDER}/ts11-swd.cfg -c "init;  ftdi_set_signal SPI_EN 1; ftdi_set_signal PLTF_PWR_EN 1; exit"
```

The variables are used without quotes in multiple places throughout the scripts (lines 77, 87, 95 in L432KC.sh; lines 35, 43, 53, 62 in F439ZI.sh).

## Recommended Fix
1. Use `$(pwd)` instead of backticks (modern syntax)
2. Quote all variable expansions:

```bash
PATH_TO_THIS_FOLDER="$(pwd)"

# Use quoted variables:
openocd -f "${PATH_TO_THIS_FOLDER}/ts11-jtag.cfg" \
```

Apply the same fix to both shell scripts:
- `third_party/libtropic/scripts/test_runner/openocd_test_L432KC.sh`
- `third_party/libtropic/scripts/test_runner/openocd_test_F439ZI.sh`

## References
- ShellCheck SC2086: Double quote to prevent globbing and word splitting: https://www.shellcheck.net/wiki/SC2086
- Bash Best Practices: https://mywiki.wooledge.org/BashBestPractices
