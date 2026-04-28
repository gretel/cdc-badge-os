---
title: "[MEDIUM] vCard Delete via Serial Command Lacks Confirmation"
severity: MEDIUM
domain: destructive-actions
lens: serial-commands
labels:
  - audit:ux-antipatterns/destructive-actions
---

## Summary
The vCard module's serial command `VCARD_DELETE` executes immediate deletion of the local vCard without any confirmation step. The `cmdVcardDelete()` function in `components/mod_vcard/src/VcardModule.cpp:422-430` directly calls `vcard_store_clear_own()` with no intermediate confirmation.

**Evidence:**
- File: `components/mod_vcard/src/VcardModule.cpp`
- Lines: 422-430
- Function: `cmdVcardDelete(const char* args)`

```cpp
static void cmdVcardDelete(const char* args) {
    (void)args;
    if (vcard_store_clear_own()) {  // Immediate delete!
        serial::Console::printf("OK: vCard deleted\r\n");
    } else {
        serial::Console::printf("ERROR: Failed to delete vCard\r\n");
    }
}
```

The command is registered at line 438:
```cpp
reg.registerCommand({"VCARD_DELETE", "Delete own vCard", cmdVcardDelete, "vcard", false});
```

## Impact
- **Data Loss Risk**: The local vCard can be accidentally deleted via serial console with a single command
- **Manual Recovery Required**: Users must manually recreate their vCard (name, contact info, social profiles, etc.)
- **Inconsistent UX**: Other modules implement confirmation patterns for delete operations

## Recommended Fix
Add a confirmation step to the `cmdVcardDelete()` serial command handler using a two-step pattern:

```cpp
static void cmdVcardDelete(const char* args) {
    if (!args || strcmp(args, "CONFIRM") != 0) {
        // Show current vCard summary and warning
        char out[VCARD_MAX_LEN + 1];
        size_t len = vcard_store_get_own(out, sizeof(out));
        
        if (len == 0) {
            serial::Console::printf("No vCard configured.\r\n");
            return;
        }
        
        // Extract key info for display
        char fn[100] = {};
        char tel[50] = {};
        char email[50] = {};
        
        // Simple parsing (could be improved)
        for (char* line = out; *line; ) {
            char* next = strchr(line, '\n');
            if (next) *next = '\0';
            
            if (strncmp(line, "FN:", 3) == 0) strncpy(fn, line + 3, sizeof(fn) - 1);
            if (strncmp(line, "TEL:", 4) == 0 && tel[0] == '\0') strncpy(tel, line + 4, sizeof(tel) - 1);
            if (strncmp(line, "EMAIL:", 6) == 0 && email[0] == '\0') strncpy(email, line + 6, sizeof(email) - 1);
            
            if (next) { line = next + 1; } else break;
        }
        
        serial::Console::printf("WARNING: Delete your vCard?\r\n");
        serial::Console::printf("  Name: %s\r\n", fn[0] ? fn : "(none)");
        serial::Console::printf("  Phone: %s\r\n", tel[0] ? tel : "(none)");
        serial::Console::printf("  Email: %s\r\n", email[0] ? email : "(none)");
        serial::Console::printf("\r\nTo proceed, type: VCARD_DELETE CONFIRM\r\n");
        return;
    }
    
    if (vcard_store_clear_own()) {
        serial::Console::printf("OK: vCard deleted\r\n");
    } else {
        serial::Console::printf("ERROR: Failed to delete vCard\r\n");
    }
}
```

## References
- Similar serial command pattern: `TR01_WIPE` in `serial_cmd/src/SerialCmd.cpp:1132`
- UI confirmation pattern: `components/cdc_views/include/cdc_views/ConfirmView.h`
