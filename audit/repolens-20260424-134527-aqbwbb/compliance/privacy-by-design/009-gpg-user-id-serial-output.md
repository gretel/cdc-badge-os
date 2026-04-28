---
title: "[LOW] GPG User-ID (name, email) printed to serial console in status and export commands"
severity: LOW
domain: Privacy by Design
lens: PII-in-Logs
labels:
  - "audit:compliance/privacy-by-design"
---

## Summary
The GPG module prints the User-ID (which contains the user's name and optionally email address) to the serial console when executing the `GPG_STATUS` command and displaying status details. This PII is output via `cdc::serial::Console::printf()` which sends data over USB CDC.

**Location:** `components/mod_gpg/src/GpgModule.cpp`

Key print statements:
- Line 139: `cdc::serial::Console::printf("User-ID: %s\r\n", status.user_id);`
- Line 401-402: `snprintf(detail, sizeof(detail), "User-ID: %s\n...", status.user_id, ...);`

The User-ID follows the OpenPGP format `Name <email>` (e.g., `John Doe <john@example.com>`) and is constructed during key generation at lines 458-488.

## Impact
- **Serial Console PII Exposure:** User names and emails are printed to the serial console, which is commonly captured during debugging, production troubleshooting, or when users connect the badge to a computer.
- **Command Output Logging:** The `GPG_STATUS` command output may be redirected to log files, exposing PII in shell history or log aggregation systems.
- **UI Display:** The User-ID is also displayed in the detail view (line 401-402), which may be captured in screenshots or screen recording during support sessions.
- **Email Format:** The User-ID often follows the format `Name <email@example.com>`, exposing both name and email in a single field.

## Evidence
**File:** `components/mod_gpg/src/GpgModule.cpp`
**Line 139 (serial command):**
```cpp
static void cmd_gpg_status(const char* args) {
    ...
    cdc::serial::Console::printf("User-ID: %s\r\n", status.user_id);
    ...
}
```

**Lines 401-402 (UI display):**
```cpp
snprintf(detail, sizeof(detail),
         "User-ID: %s\nCurve: %s\nFingerprint: %s\nCreated: %lu\nSign Count: %lu",
         status.user_id, curveName, fp_hex,
         static_cast<unsigned long>(status.created_at),
         static_cast<unsigned long>(status.sign_count));
```

**Lines 458-488 (User-ID construction):**
```cpp
char user_id[GPG_USER_ID_MAX] = {};
...
if (email_len == 0) {
    snprintf(user_id, sizeof(user_id), "%.*s", static_cast<int>(sizeof(user_id) - 1), s_wizard.name);
} else {
    ...
    memcpy(user_id + pos, s_wizard.name, name_fit);
    user_id[pos++] = ' ';
    user_id[pos++] = '<';
    memcpy(user_id + pos, s_wizard.email, email_fit);
    pos += email_fit;
    user_id[pos++] = '>';
    user_id[pos] = '\0';
}
```

**File:** `components/mod_gpg/include/mod_gpg/GpgStorage.h`
**User-ID structure:**
```cpp
typedef struct {
    char user_id[GPG_USER_ID_MAX];  // e.g., "John Doe <john@example.com>"
    ...
} gpg_status_t;
```

## Recommended Fix
1. **Add option to mask User-ID in serial output:**
   ```cpp
   // For GPG_STATUS command
   cdc::serial::Console::printf("User-ID: %.*s...\r\n", 20, status.user_id);
   ```

2. **Add DEBUG_MODE guard for full User-ID logging:**
   ```cpp
   #ifdef DEBUG_MODE
   cdc::serial::Console::printf("User-ID: %s\r\n", status.user_id);
   #else
   cdc::serial::Console::printf("User-ID: (present)\r\n");
   #endif
   ```

3. **Add a `--verbose` flag to serial commands** to control PII output:
   ```cpp
   static void cmd_gpg_status(const char* args) {
       bool verbose = strstr(args, "--verbose") != nullptr;
       ...
       if (verbose) {
           cdc::serial::Console::printf("User-ID: %s\r\n", status.user_id);
       } else {
           cdc::serial::Console::printf("User-ID: (present)\r\n");
       }
   }
   ```

4. **For UI display, consider truncating** or showing only first character of name:
   ```cpp
   // Show first letter of name + email
   char masked_id[64];
   if (strlen(status.user_id) > 0) {
       snprintf(masked_id, sizeof(masked_id), "%c... <...>", status.user_id[0]);
   }
   ```

## References
- GDPR Article 5(1)(c) - Data minimization: "Personal data shall be adequate, relevant and limited to what is necessary"
- Common logging best practices: Avoid logging PII in command output
- Serial console security: Consider that serial output may be captured in logs, shell history, or forwarded to remote systems

</content>