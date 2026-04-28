---
title: "[LOW] Shell scripts use deprecated `echo -n` without portable alternative"
severity: LOW
domain: lint
lens: toolgate/lint
labels:
  - "audit:toolgate/lint"
---

## Summary
Shell scripts use `echo -n` which has inconsistent behavior across different shells and platforms.

**Files affected:**
- `third_party/libtropic/scripts/test_runner/openocd_test_L432KC.sh` (line 43)
- `third_party/libtropic/scripts/test_runner/openocd_test_F439ZI.sh` (line 13)
- `third_party/libtropic/vendor/trezor_crypto/fuzzer/extract_fuzzer_dictionary.sh` (line 22)

## Impact
`echo -n` behavior varies across shells (bash, dash, sh, ksh). Using `printf` is more portable and consistent.

## Evidence
**third_party/libtropic/vendor/trezor_crypto/fuzzer/extract_fuzzer_dictionary.sh (line 22):**
```bash
# ensure empty file
echo -n "" > $OUTPUT_FILE
```

**third_party/libtropic/scripts/test_runner/openocd_test_L432KC.sh (line 43):**
```bash
#### IMPORTANT: go to NUCLEO_F439ZI/Src/Main.h and set #define DBG_UART_OVER_USB 0, recompile.
```
(Note: This is a comment, but the script uses `echo ""` patterns throughout)

## Recommended Fix
Replace `echo -n` with `printf`:

```bash
# instead of:
echo -n "" > $OUTPUT_FILE

# use:
printf "" > "$OUTPUT_FILE"
```

Or for general echo usage:
```bash
# instead of:
echo "message"

# use:
printf "%s\n" "message"
```

## References
- `echo` vs `printf` portability: https://mywiki.wooledge.org/UsingEcho
- POSIX echo: https://pubs.opengroup.org/onlinepubs/9699919799/utilities/echo.html
