---
title: "[MEDIUM] Shell scripts use cd without error checking"
severity: MEDIUM
domain: lint
lens: toolgate/lint
labels:
  - "audit:toolgate/lint"
---

## Summary
Shell scripts use `cd` commands without checking if the directory exists or if the change was successful. Even with `set -e`, the scripts don't explicitly handle cd failures.

**Files affected:**
- `third_party/libtropic/scripts/codechecker/codechecker_build.sh` (lines 2, 4)

## Impact
If the target directory doesn't exist or isn't accessible:
1. **Silent failures**: `cd` might succeed but in the wrong directory
2. **Confusing errors**: Subsequent commands fail with path-related errors that don't indicate the root cause
3. **Harder debugging**: Users need to trace back to find the cd failure

## Evidence
**third_party/libtropic/scripts/codechecker/codechecker_build.sh (lines 1-7):**
```bash
#!/bin/bash
cd tropic01_model/
mkdir -p build
cd build
cmake -DLT_BUILD_EXAMPLES=1 -DLT_CAL=mbedtls_v4 ..
make clean && make
```

The script changes directories twice without:
- Verifying the directories exist first
- Using absolute paths
- Capturing the original directory to return to

## Recommended Fix
Add error checking and use absolute paths:

```bash
#!/bin/bash
set -euo pipefail

# Get script directory for absolute paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}/tropic01_model/"

# Check if directory exists
if [ ! -d "tropic01_model/" ]; then
    echo "ERROR: tropic01_model/ directory not found"
    exit 1
fi

mkdir -p build
cd build
cmake -DLT_BUILD_EXAMPLES=1 -DLT_CAL=mbedtls_v4 ..
make clean && make
```

Or use a more robust pattern:
```bash
#!/bin/bash
set -euo pipefail

# Change to script directory first
cd "$(dirname "${BASH_SOURCE[0]}")"

# Use relative paths from script location
cd tropic01_model/ || { echo "ERROR: Failed to cd to tropic01_model/"; exit 1; }
mkdir -p build
cd build || { echo "ERROR: Failed to cd to build/"; exit 1; }
cmake -DLT_BUILD_EXAMPLES=1 -DLT_CAL=mbedtls_v4 ..
make clean && make
```

## References
- Bash Best Practices: https://mywiki.wooledge.org/BashBestPractices
- ShellCheck SC2164: Use cd ... || exit: https://www.shellcheck.net/wiki/SC2164
