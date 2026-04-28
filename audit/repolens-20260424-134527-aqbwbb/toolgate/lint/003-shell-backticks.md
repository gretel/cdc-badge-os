---
title: "[MEDIUM] Python uses deprecated backtick syntax in shell scripts"
severity: MEDIUM
domain: lint
lens: toolgate/lint
labels:
  - "audit:toolgate/lint"
---

## Summary
Shell scripts use deprecated backtick syntax for command substitution instead of the modern `$(...)` syntax.

**Files affected:**
- `third_party/libtropic/scripts/test_runner/openocd_test_L432KC.sh` (line 48)
- `third_party/libtropic/scripts/test_runner/openocd_test_F439ZI.sh` (line 13)

## Impact
Backtick syntax is harder to read, especially with nested commands or escaping. The `$(...)` syntax is more readable and allows for easier nesting.

## Evidence
**third_party/libtropic/scripts/test_runner/openocd_test_F439ZI.sh (line 13):**
```bash
PATH_TO_THIS_FOLDER=`pwd`
```

**third_party/libtropic/scripts/test_runner/openocd_test_L432KC.sh (line 48):**
```bash
PATH_TO_THIS_FOLDER=`pwd`
```

## Recommended Fix
Replace backticks with `$(...)` syntax:

```bash
PATH_TO_THIS_FOLDER="$(pwd)"
```

Apply to both scripts:
- `third_party/libtropic/scripts/test_runner/openocd_test_L432KC.sh`
- `third_party/libtropic/scripts/test_runner/openocd_test_F439ZI.sh`

## References
- POSIX Shell Command Language: https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_05_03
- Bash Manual - Command Substitution: https://www.gnu.org/software/bash/manual/html_node/Command-Substitution.html
