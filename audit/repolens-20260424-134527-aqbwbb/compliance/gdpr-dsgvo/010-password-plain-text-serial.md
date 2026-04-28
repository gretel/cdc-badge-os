---
title: "[MEDIUM] Password Printed in Plain Text via PASSWORD_GET Serial Command"
severity: MEDIUM
domain: gdpr-dsgvo
lens: compliance/gdpr-dsgvo
labels:
  - "audit:compliance/gdpr-dsgvo"
---

## Summary
The `PASSWORD_GET` serial command prints the complete password entry including the password field in plain text to the serial console. This allows anyone with serial access to view all stored passwords without additional authentication.

**File**: `components/mod_password/src/PasswordModule.cpp`

## Impact
**Data Exposure**: Anyone with physical access to the serial port (USB-C connection) can retrieve all stored passwords by issuing the `PASSWORD_GET <index>` command.

**No Additional Authentication**: The command is available after basic PIN entry for the badge, but no re-authentication is required to view passwords. This is a weakness for a security-focused device.

**Plain Text Transmission**: Passwords are sent over the serial connection without any encryption or obfuscation, making them visible if the serial connection is monitored.

**Comparison to Existing Findings**: This is different from finding #6 (GPG cardholder data) and finding #7 (BLE peer names) - this is actual secret data (passwords) being exposed.

## Evidence

### PASSWORD_GET Command Implementation
```cpp
// components/mod_password/src/PasswordModule.cpp:218-246
static void cmd_password_get(const char* args) {
    // ... argument parsing ...
    PasswordEntry entry = {};
    if (!PasswordStore::instance().readEntry(slot, &entry)) {
        cdc::serial::Console::printf("ERROR: read failed\r\n");
        return;
    }
    cdc::serial::Console::printf("Title: %s\r\n", entry.title);
    cdc::serial::Console::printf("Username: %s\r\n", entry.username);
    cdc::serial::Console::printf("Password: %s\r\n", entry.password);  // <-- PLAIN TEXT!
    cdc::serial::Console::printf("URL: %s\r\n", entry.url);
    // ... TOTP and notes ...
}
```

### Command Registration
```cpp
// components/mod_password/src/PasswordModule.cpp:332
reg.registerCommand({"PASSWORD_GET", "Get password entry", cmd_password_get, CMD_MODULE, true});
```

### PasswordEntry Structure
```cpp
// components/mod_password/include/mod_password/PasswordStore.h:42
struct PasswordEntry {
    char title[64];
    char username[64];
    char password[128];  // <-- Stored and printed in plain text
    char url[128];
    uint16_t totpSlot;
    char notes[256];
};
```

## Recommended Fix

### Option 1: Mask Password in Serial Output (~1 hour)
Modify `cmd_password_get` to print the password masked:

```cpp
// components/mod_password/src/PasswordModule.cpp:238
// Before:
cdc::serial::Console::printf("Password: %s\r\n", entry.password);

// After:
cdc::serial::Console::printf("Password: %s\r\n", "********");
// Or show length only
cdc::serial::Console::printf("Password: %d characters\r\n", strlen(entry.password));
```

### Option 2: Add --show Flag for Explicit Password Display (~1 hour)
Require an explicit flag to show the password:

```cpp
static void cmd_password_get(const char* args) {
    // ... argument parsing ...
    bool show_password = strstr(args, "--show") != nullptr;
    
    cdc::serial::Console::printf("Title: %s\r\n", entry.title);
    cdc::serial::Console::printf("Username: %s\r\n", entry.username);
    if (show_password) {
        cdc::serial::Console::printf("Password: %s\r\n", entry.password);
    } else {
        cdc::serial::Console::printf("Password: %d chars (use --show to reveal)\r\n", 
                                     strlen(entry.password));
    }
    // ... rest ...
}
```

### Option 3: Add Password Reveal via Serial Command with Timeout (~1 hour)
Add a separate command that reveals the password for a short time:

```cpp
static void cmd_password_reveal(const char* args) {
    // Get index
    char indexBuf[8];
    // ... parsing ...
    
    PasswordEntry entry;
    PasswordStore::instance().readEntry(slot, &entry);
    
    // Print password
    cdc::serial::Console::printf("PASSWORD: %s\r\n", entry.password);
    cdc::serial::Console::printf("Copy quickly - will be cleared in 5 seconds...\r\n");
    
    // Clear serial buffer after delay (pseudo-code)
    vTaskDelay(pdMS_TO_TICKS(5000));
    // Clear the last line somehow (terminal escape codes)
}

// In registerCommands():
reg.registerCommand({"PASSWORD_REVEAL", "Temporarily show password", cmd_password_reveal, CMD_MODULE, true});
```

### Option 4: Limit PASSWORD_GET to Metadata Only (~1 hour)
The serial command should only show non-sensitive metadata:

```cpp
static void cmd_password_get(const char* args) {
    // ... parsing ...
    cdc::serial::Console::printf("Title: %s\r\n", entry.title);
    cdc::serial::Console::printf("Username: %s\r\n", entry.username);
    cdc::serial::Console::printf("Password: [hidden]\r\n");  // Always hidden
    cdc::serial::Console::printf("URL: %s\r\n", entry.url);
    cdc::serial::Console::printf("Notes: %s\r\n", entry.notes);
}
```

### References
- **GDPR Art. 32**: "Security of processing" - Appropriate technical measures to protect personal data
- **Best Practice**: Password managers typically show passwords only on explicit request with visual masking
- **Security**: Serial console should be treated as a potentially insecure channel
