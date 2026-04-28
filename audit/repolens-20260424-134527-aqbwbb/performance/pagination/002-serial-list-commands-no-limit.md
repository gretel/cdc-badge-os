---
title: "[LOW] Serial list commands output all records without pagination limit"
severity: LOW
domain: performance/pagination
lens: pagination-streaming
labels:
  - "audit:performance/pagination"
---

## Summary
The serial command interface for listing passwords and TOTP accounts outputs **all entries** to the console without any pagination or limiting mechanism. This can result in large amounts of data being transmitted over the serial port at 115200 baud.

**File**: `components/mod_password/src/PasswordModule.cpp:184-212`
```cpp
static void cmd_password_list(const char* args) {
    // ...
    auto list = std::unique_ptr<PasswordStore::EntryIndex[]>(new (std::nothrow) PasswordStore::EntryIndex[cap]);
    // ...
    for (uint16_t i = 0; i < count; i++) {
        cdc::serial::Console::printf("%u: %s (slot %u)\r\n", i, list[i].title, list[i].slot);
    }
}
```

**File**: `components/mod_totp/src/TotpModule.cpp:195-221`
```cpp
static void cmd_totp_list(const char* args) {
    // ...
    cdc::core::TropicStorage::instance().forEachSlot(
        TotpStore::instance().moduleId(),
        TotpStore::instance().rmemStart(),
        TotpStore::instance().rmemEnd(),
        cb, &ctx);
    // ...
}
```

## Impact
- **Serial output**: With 362 password entries, output could be ~5-10KB of text, taking ~0.5-1 second to transmit
- **Memory usage**: Full allocation of entry array for all records
- **No filtering**: Users cannot request specific ranges or search for entries
- **Blocking I/O**: Serial output is synchronous, blocking other operations during transmission

## Evidence
**File**: `components/mod_password/src/PasswordModule.cpp:184-212`
```cpp
static void cmd_password_list(const char* args) {
    (void)args;  // No arguments parsed for pagination
    auto& store = PasswordStore::instance();
    // ...
    auto list = std::unique_ptr<PasswordStore::EntryIndex[]>(new (std::nothrow) PasswordStore::EntryIndex[cap]);
    // ...
    for (uint16_t i = 0; i < count; i++) {  // Outputs ALL entries
        cdc::serial::Console::printf("%u: %s (slot %u)\r\n", i, list[i].title, list[i].slot);
    }
}
```

**File**: `components/mod_totp/src/TotpModule.cpp:195-221`
```cpp
static void cmd_totp_list(const char* args) {
    (void)args;  // No arguments parsed for pagination
    // ...
    cdc::core::TropicStorage::instance().forEachSlot(
        TotpStore::instance().moduleId(),
        TotpStore::instance().rmemStart(),
        TotpStore::instance().rmemEnd(),
        cb, &ctx);  // Iterates ALL slots
}
```

## Recommended Fix
Add optional pagination parameters to list commands:

1. **Parse arguments** for `offset` and `limit`:
   ```
   PASSWORD_LIST [offset] [limit]
   TOTP_LIST [offset] [limit]
   ```

2. **Default limit** of 20-30 entries when not specified

3. **Output pagination metadata**:
   ```
   Showing 1-20 of 35 entries
   Use: PASSWORD_LIST 20 20  (to see next page)
   ```

4. **Use streaming output** instead of buffering all entries:
   ```cpp
   // Process one entry at a time instead of allocating full array
   uint16_t count = 0;
   uint16_t displayed = 0;
   auto cb = [](uint16_t slot, const auto& entry, void* user) {
       auto* ctx = static_cast<Context*>(user);
       if (count < ctx->offset) { count++; return; }
       if (ctx->displayed >= ctx->limit) return;
       cdc::serial::Console::printf("%u: %s (slot %u)\r\n", 
                                    ctx->offset + ctx->displayed, 
                                    entry.name, slot);
       ctx->displayed++;
       count++;
   };
   ```

## References
- Serial baud rate: 115200 (~11.5 KB/s effective)
- NVS storage for TROPIC01 metadata cache
- `TropicStorage::forEachSlot()` already uses callback pattern suitable for streaming
