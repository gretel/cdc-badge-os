---
title: "[LOW] NVS command uses sscanf without bounds checking for namespace/key"
severity: LOW
domain: security
lens: session-nuclei
labels:
  - audit:toolgate/session-nuclei
---

## Summary

In `components/serial_cmd/src/SerialCmd.cpp`, the NVS read/delete commands use `sscanf` with format specifiers that could be improved for clarity:

```cpp
char ns[NVS_NAMESPACE_MAX_LEN + 1] = {0};
char key[NVS_KEY_MAX_LEN + 1] = {0};
if (sscanf(args, "%15s %15s", ns, key) != 2) {
```

While the format specifiers do limit the input to 15 characters (matching the buffer size - 1 for null terminator), using `%s` without whitespace handling could cause issues with arguments containing spaces.

## Impact

- **Argument parsing**: If a namespace or key contains spaces, the parsing will fail silently.
- **Consistency**: The format specifier `%15s` is correct but could be more explicit about whitespace handling.
- **Error messages**: The error handling doesn't distinguish between too many arguments vs. wrong format.

## Evidence

**File**: `components/serial_cmd/src/SerialCmd.cpp` (lines 580-585)

```cpp
char ns[NVS_NAMESPACE_MAX_LEN + 1] = {0};
char key[NVS_KEY_MAX_LEN + 1] = {0};
if (sscanf(args, "%15s %15s", ns, key) != 2) {
    Console::printf("Usage: NVS_READ <namespace> <key>\r\n");
    return;
}
```

**File**: `components/serial_cmd/src/SerialCmd.cpp` (lines 617-622)

```cpp
char ns[NVS_NAMESPACE_MAX_LEN + 1] = {0};
char key[NVS_KEY_MAX_LEN + 1] = {0};
int parsed = sscanf(args, "%15s %15s", ns, key);

if (parsed < 1) {
    Console::printf("Usage: NVS_DEL <namespace> [key]\r\n");
```

## Recommended Fix

**Option 1: Use more explicit parsing**
For better error handling, consider manual parsing:

```cpp
// Skip leading whitespace
while (*args && isspace(*args)) args++;
// Find first space
char* space = strchr(args, ' ');
if (!space) {
    // Handle single argument case
}
strncpy(ns, args, NVS_NAMESPACE_MAX_LEN);
ns[NVS_NAMESPACE_MAX_LEN] = '\0';
```

**Option 2: Add validation**
After sscanf, validate that the arguments are reasonable:

```cpp
if (sscanf(args, "%15s %15s", ns, key) != 2) {
    Console::printf("Usage: NVS_READ <namespace> <key>\r\n");
    return;
}
// Validate no trailing garbage
char* rest = args;
while (*rest && !isspace(*rest)) rest++;
while (*rest && isspace(*rest)) rest++;
char* key_start = rest;
while (*rest && !isspace(*rest)) rest++;
while (*rest && isspace(*rest)) rest++;
if (*rest) {
    Console::printf("WARNING: Extra arguments ignored\r\n");
}
```

**Option 3: Keep as-is with documentation**
The current implementation is correct, just add a comment explaining the format:

```cpp
// Parse namespace and key (max 15 chars each, space-separated)
// Using %15s ensures null-termination in 16-byte buffers
if (sscanf(args, "%15s %15s", ns, key) != 2) {
```

## References

- C sscanf documentation
- ESP-NVS library documentation
- Command-line parsing best practices
