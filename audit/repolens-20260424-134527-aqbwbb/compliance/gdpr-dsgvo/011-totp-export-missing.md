---
title: "[LOW] TOTP Account Metadata Not Exportable for Data Portability"
severity: LOW
domain: gdpr-dsgvo
lens: compliance/gdpr-dsgvo
labels:
  - "audit:compliance/gdpr-dsgvo"
---

## Summary
The TOTP module stores account metadata (name, issuer, secret, digits, period, algorithm) but lacks a serial command or API to export this data in a machine-readable format. Users cannot easily backup or migrate their TOTP accounts to another device.

**File**: `components/mod_totp/src/TotpModule.cpp` and `components/mod_totp/include/mod_totp/TotpStore.h`

## Impact
**Data Portability (Art. 20 GDPR)**: Users cannot export their TOTP account data in a structured, commonly used format. This makes it difficult to migrate to a different device or backup their TOTP setup.

**Vendor Lock-in**: Without an export mechanism, users are effectively locked into this specific badge firmware for their TOTP accounts.

**Backup Difficulty**: No easy way to backup TOTP accounts before firmware updates or device migration.

**Comparison**: This is different from the password module which at least has serial commands to list and view entries (even if plain text).

## Evidence

### TotpAccount Structure
```cpp
// components/mod_totp/include/mod_totp/TotpStore.h:15-24
struct TotpAccount {
    char name[16 + 1];        // Account name (e.g., "john@example.com")
    char issuer[32 + 1];      // Issuer (e.g., "Google", "GitHub")
    uint8_t secret[32];       // Base256 secret
    uint8_t secretLen;
    uint8_t digits;           // Usually 6 or 8
    uint32_t period;          // Usually 30 seconds
    uint8_t algorithm;        // SHA1, SHA256, SHA512
    uint8_t flags;
};
```

### Serial Commands Available (No Export)
```cpp
// components/mod_totp/src/TotpModule.cpp:328-332
reg.registerCommand({"TOTP_LIST",   "List TOTP accounts",      cmd_totp_list,  CMD_MODULE, true});
reg.registerCommand({"TOTP_ADD",    "Add TOTP account",        cmd_totp_add,   CMD_MODULE, true});
reg.registerCommand({"TOTP_DELETE", "Delete TOTP account",     cmd_totp_del,   CMD_MODULE, true});
reg.registerCommand({"TOTP_SHOW",   "Show current TOTP code",  cmd_totp_show,  CMD_MODULE, true});
```

### TOTP List Command (Limited Output)
```cpp
// components/mod_totp/src/TotpModule.cpp:113-117
static void cmd_totp_list(const char* args) {
    (void)args;
    auto& store = TotpStore::instance();
    uint16_t cap = store.capacity();
    for (uint8_t i = 0; i < cap; i++) {
        TotpStore::TotpEntry entry;
        if (store.readAccount(i, &entry)) {
            cdc::serial::Console::printf("%u: %s (slot %u)\r\n", i, entry.name, logical);
        }
    }
}
```

### No Export Command
No `TOTP_EXPORT` or `TOTP_BACKUP` command exists. The only way to see the full data is through the UI or by reading the raw NVS/TROPIC storage.

## Recommended Fix

### Option 1: Add TOTP_EXPORT Command (~1 hour)
Add a serial command that exports all TOTP accounts in a machine-readable format:

```cpp
static void cmd_totp_export(const char* args) {
    (void)args;
    auto& store = TotpStore::instance();
    uint16_t cap = store.capacity();
    
    cdc::serial::Console::printf("TOTP Accounts (JSON format):\r\n");
    cdc::serial::Console::printf("[\r\n");
    
    bool first = true;
    for (uint8_t i = 0; i < cap; i++) {
        TotpStore::TotpEntry entry;
        if (store.readAccount(i, &entry)) {
            if (!first) cdc::serial::Console::printf(",\r\n");
            first = false;
            
            cdc::serial::Console::printf("  {\r\n");
            cdc::serial::Console::printf("    \"name\": \"%s\",\r\n", entry.name);
            cdc::serial::Console::printf("    \"issuer\": \"%s\",\r\n", entry.issuer);
            cdc::serial::Console::printf("    \"digits\": %u,\r\n", entry.digits);
            cdc::serial::Console::printf("    \"period\": %u,\r\n", entry.period);
            cdc::serial::Console::printf("    \"algorithm\": \"%s\",\r\n", 
                entry.algorithm == 0 ? "SHA1" : entry.algorithm == 1 ? "SHA256" : "SHA512");
            // Note: Secret should be Base32 encoded for OATH compatibility
            uint8_t base32[65];
            base32_encode(entry.secret, entry.secretLen, base32, sizeof(base32));
            cdc::serial::Console::printf("    \"secret\": \"%s\"\r\n", base32);
            cdc::serial::Console::printf("  }");
        }
    }
    
    cdc::serial::Console::printf("\r\n]\r\n");
}

// In registerCommands():
reg.registerCommand({"TOTP_EXPORT", "Export TOTP accounts as JSON", cmd_totp_export, CMD_MODULE, true});
```

### Option 2: Add TOTP_GET Command for Single Account (~1 hour)
Add a command to get a single account's details:

```cpp
static void cmd_totp_get(const char* args) {
    char indexBuf[8];
    const char* p = nextToken(args, indexBuf, sizeof(indexBuf));
    if (!p || !indexBuf[0]) {
        cdc::serial::Console::printf("Usage: TOTP_GET <index>\r\n");
        return;
    }
    uint16_t index = static_cast<uint16_t>(atoi(indexBuf));
    
    TotpStore::TotpEntry entry;
    if (!TotpStore::instance().readAccount(index, &entry)) {
        cdc::serial::Console::printf("ERROR: invalid index\r\n");
        return;
    }
    
    cdc::serial::Console::printf("Name: %s\r\n", entry.name);
    cdc::serial::Console::printf("Issuer: %s\r\n", entry.issuer);
    cdc::serial::Console::printf("Digits: %u\r\n", entry.digits);
    cdc::serial::Console::printf("Period: %u\r\n", entry.period);
    cdc::serial::Console::printf("Algorithm: %s\r\n", 
        entry.algorithm == 0 ? "SHA1" : entry.algorithm == 1 ? "SHA256" : "SHA512");
    // Secret in Base32 for compatibility
    uint8_t base32[65];
    base32_encode(entry.secret, entry.secretLen, base32, sizeof(base32));
    cdc::serial::Console::printf("Secret: %s\r\n", base32);
}

// In registerCommands():
reg.registerCommand({"TOTP_GET", "Get TOTP account details", cmd_totp_get, CMD_MODULE, true});
```

### Option 3: Add OATH-URI Export for Easy Migration (~1 hour)
Export in `otpauth://` URI format for easy import into other TOTP apps:

```cpp
static void cmd_totp_uri(const char* args) {
    char indexBuf[8];
    const char* p = nextToken(args, indexBuf, sizeof(indexBuf));
    if (!p || !indexBuf[0]) {
        cdc::serial::Console::printf("Usage: TOTP_URI <index>\r\n");
        return;
    }
    uint16_t index = static_cast<uint16_t>(atoi(indexBuf));
    
    TotpStore::TotpEntry entry;
    if (!TotpStore::instance().readAccount(index, &entry)) {
        cdc::serial::Console::printf("ERROR: invalid index\r\n");
        return;
    }
    
    // Build otpauth://totp/URI
    uint8_t base32[65];
    base32_encode(entry.secret, entry.secretLen, base32, sizeof(base32));
    
    cdc::serial::Console::printf("otpauth://totp/%s:%s?secret=%s&issuer=%s&digits=%u&period=%u\r\n",
        entry.issuer, entry.name, base32, entry.issuer, entry.digits, entry.period);
}

// In registerCommands():
reg.registerCommand({"TOTP_URI", "Export TOTP as otpauth URI", cmd_totp_uri, CMD_MODULE, true});
```

### Helper Function: Base32 Encoding
```cpp
// Simple Base32 encoding for OATH compatibility
static void base32_encode(const uint8_t* in, uint8_t len, char* out, size_t out_max) {
    static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    size_t pos = 0;
    uint8_t bits = 0;
    uint8_t acc = 0;
    
    for (uint8_t i = 0; i < len && pos < out_max - 1; i++) {
        acc = (acc << 8) | in[i];
        bits += 8;
        while (bits >= 5) {
            out[pos++] = alphabet[(acc >> (bits - 5)) & 0x1F];
            bits -= 5;
        }
    }
    if (bits > 0) {
        out[pos++] = alphabet[(acc << (5 - bits)) & 0x1F];
    }
    // Add padding
    while (pos < out_max - 1 && pos % 8 != 0) {
        out[pos++] = '=';
    }
    out[pos] = '\0';
}
```

### References
- **GDPR Art. 20**: "Right to data portability" - Receive data in structured, commonly used format
- **OATH TOTP Specification**: https://tools.ietf.org/html/rfc6238
- **otpauth URI Format**: https://github.com/google/google-authenticator/wiki/Key-Uri-Format
