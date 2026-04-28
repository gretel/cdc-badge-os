---
title: "[LOW] Shell scripts lack set -u for undefined variables"
severity: LOW
domain: lint
lens: toolgate/lint
labels:
  - "audit:toolgate/lint"
---

## Summary
Shell scripts use `set -e` but not `set -u`, which means undefined variables won't cause immediate failures.

**Files affected:**
- `third_party/libtropic/scripts/test_runner/openocd_test_L432KC.sh`
- `third_party/libtropic/scripts/test_runner/openocd_test_F439ZI.sh`
- `third_party/libtropic/scripts/codechecker/codechecker_build.sh`
- `third_party/libtropic/vendor/trezor_crypto/tests/wycheproof/kokoro/presubmit.sh`
- `third_party/libtropic/vendor/trezor_crypto/tests/wycheproof/kokoro/continuous.sh`
- `components/Adafruit-GFX/fontconvert/makefonts.sh`

## Impact
Without `set -u`, scripts may continue running with empty or unexpected variable values, leading to subtle bugs that are harder to debug.

## Evidence
**third_party/libtropic/scripts/test_runner/openocd_test_F439ZI.sh (lines 3-7):**
```bash
#!/usr/bin/env bash

# Fail on any error.
set -e

# Display commands to stderr.
set -x
```

The script has `set -e` (exit on error) and `set -x` (trace commands) but is missing `set -u` (treat undefined variables as errors).

## Recommended Fix
Add `set -u` to the shell scripts that already have `set -e`:

```bash
#!/usr/bin/env bash

# Fail on any error.
set -eu

# Display commands to stderr.
set -x
```

Or use the more comprehensive `set -euo pipefail`:
```bash
#!/usr/bin/env bash
set -euo pipefail
```

Apply to:
- `third_party/libtropic/scripts/test_runner/openocd_test_L432KC.sh`
- `third_party/libtropic/scripts/test_runner/openocd_test_F439ZI.sh`

## References
- Bash Best Practices: https://mywiki.wooledge.org/BashBestPractices
- ShellCheck SC2317: Consider using set -u: https://www.shellcheck.net/wiki/SC2317
