---
title: "[MEDIUM] Shell script uses unquoted variables with xargs"
severity: MEDIUM
domain: lint
lens: toolgate/lint
labels:
  - "audit:toolgate/lint"
---

## Summary
Shell script uses unquoted variable expansion with `xargs`, which can cause word splitting issues when paths contain spaces.

**Files affected:**
- `third_party/libtropic/vendor/trezor_crypto/fuzzer/extract_fuzzer_dictionary.sh` (line 18)

## Impact
Unquoted variables with xargs can cause:
1. **Word splitting**: Paths with spaces break the find command
2. **Globbing**: Paths with `*`, `?`, or `[` expand unexpectedly
3. **Silent failures**: Commands may run with wrong arguments

## Evidence
**third_party/libtropic/vendor/trezor_crypto/fuzzer/extract_fuzzer_dictionary.sh (lines 14-19):**
```bash
TARGET_DIR=../tests
OUTPUT_FILE=${1:-fuzzer_crypto_tests_strings_dictionary1.txt}

multiline_string_search() {
  # TODO the `find` regex behavior is Linux-specific
  find $TARGET_DIR -type f -regextype posix-extended -regex '.*\.(c|h|py|json|java|js)' | xargs cat | perl -p0e 's/"\s*\n\s*\"//smg'
}
```

The `$TARGET_DIR` variable is used unquoted in the `find` command.

## Recommended Fix
Quote all variable expansions:

```bash
TARGET_DIR=../tests
OUTPUT_FILE=${1:-fuzzer_crypto_tests_strings_dictionary1.txt}

multiline_string_search() {
  # TODO the `find` regex behavior is Linux-specific
  find "$TARGET_DIR" -type f -regextype posix-extended -regex '.*\.(c|h|py|json|java|js)' | xargs cat | perl -p0e 's/"\s*\n\s*\"//smg'
}
```

Also consider using modern command substitution:
```bash
multiline_string_search() {
  find "${TARGET_DIR}" -type f -regextype posix-extended -regex '.*\.(c|h|py|json|java|js)' | xargs -r cat | perl -p0e 's/"\s*\n\s*\"//smg'
}
```

The `-r` flag for xargs prevents running cat with no arguments.

## References
- ShellCheck SC2086: Double quote to prevent globbing and word splitting: https://www.shellcheck.net/wiki/SC2086
- xargs -r flag: https://linux.die.net/man/1/xargs
