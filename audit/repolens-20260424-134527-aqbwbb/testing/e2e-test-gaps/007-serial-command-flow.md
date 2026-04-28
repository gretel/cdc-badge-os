---
title: "[MEDIUM] No E2E Tests for Serial Command Interface"
severity: MEDIUM
domain: testing
lens: e2e-test-gaps
labels:
  - "audit:testing/e2e-test-gaps"
---

## Summary

The **Serial Command Interface** (`components/serial_cmd/`) provides a text-based command interface over USB CDC at 115200 baud, but has **no E2E tests**. Commands are used for:

1. System configuration (time, display, PIN)
2. Module operations (TOTP, Passwords, GPG)
3. TROPIC01 secure element management
4. Debug and diagnostics

**Critical command flows untested:**
- Authentication (AUTH/LOGOUT)
- TOTP CRUD operations
- Password CRUD operations
- GPG key generation
- System reboot
- NVS management

## Impact

**Usability Risk:**
- Command parsing bugs could break configuration
- Auth flow bugs could expose privileged commands
- Error messages might be unclear

**Integration Risk:**
- Serial interface is primary configuration method
- Tools like `flash_firmware.py` depend on serial
- No automated verification of command responses

## Evidence

**Serial Command Module** (`components/serial_cmd/`):
- `SerialCmd.cpp` - Main command handler
- `SerialCmd.h` - Command registration API

**Command Structure** (`docs/SERIAL_COMMANDS.md`):
```markdown
## Authentication
| Command | Description |
|---------|-------------|
| `AUTH <pin>` | Authenticate with PIN |
| `LOGOUT` | End authenticated session |

## TOTP Module
| Command | Description |
|---------|-------------|
| `TOTP_LIST` | List all TOTP accounts `[AUTH]` |
| `TOTP_ADD <name> <secret> [issuer] [digits] [period]` | Add TOTP account `[AUTH]` |
| `TOTP_DEL <index>` | Delete TOTP account by index `[AUTH]` |
| `TOTP_GET <index>` | Generate TOTP code by index `[AUTH]` |

## Password Module
| Command | Description |
|---------|-------------|
| `PASSWORD_LIST` | List password entries `[AUTH]` |
| `PASSWORD_ADD <name> <user> <url> <password>` | Add password entry `[AUTH]` |
| `PASSWORD_DEL <index>` | Delete password entry `[AUTH]` |
```

**Command Registration** (`components/serial_cmd/src/SerialCmd.cpp`):
```cpp
// Simplified command registration
void SerialCmd::init() {
    CommandRegistry::registerCommand("HELP", cmd_help);
    CommandRegistry::registerCommand("PING", cmd_ping);
    CommandRegistry::registerCommand("STATUS", cmd_status);
    CommandRegistry::registerCommand("AUTH", cmd_auth);
    CommandRegistry::registerCommand("TOTP_ADD", cmd_totp_add);
    CommandRegistry::registerCommand("TOTP_DEL", cmd_totp_del);
    // ... many more
}
```

**No E2E tests exist** for serial commands.

## Recommended Fix

Create E2E test file `test/e2e/e2e_serial_commands.cpp`:

```cpp
#include <unity.h>
#include "serial_cmd/SerialCmd.h"
#include "serial_cmd/CommandRegistry.h"
#include "cdc_core/PinManager.h"

using namespace cdc::serial;

void setUp() {
    SerialCmd::init();
    PinManager::instance().init();
}

// ============================================
// Basic Command Tests
// ============================================

void test_serial_help_command() {
    const char* response = SerialCmd::execute("HELP");
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(strstr(response, "HELP") != NULL);
}

void test_serial_ping_command() {
    const char* response = SerialCmd::execute("PING");
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(strstr(response, "PONG") != NULL);
}

void test_serial_status_command() {
    const char* response = SerialCmd::execute("STATUS");
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(strlen(response) > 0);
}

// ============================================
// Authentication Tests
// ============================================

void test_serial_auth_success() {
    const char* response = SerialCmd::execute("AUTH 123456");
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(strstr(response, "OK") != NULL);
    
    // Verify session active
    TEST_ASSERT_TRUE(SerialCmd::isAuthenticated());
}

void test_serial_auth_failure() {
    const char* response = SerialCmd::execute("AUTH wrong");
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(strstr(response, "FAIL") != NULL);
    
    // Verify session not active
    TEST_ASSERT_FALSE(SerialCmd::isAuthenticated());
}

void test_serial_logout() {
    // Auth first
    SerialCmd::execute("AUTH 123456");
    TEST_ASSERT_TRUE(SerialCmd::isAuthenticated());
    
    // Logout
    const char* response = SerialCmd::execute("LOGOUT");
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(strstr(response, "OK") != NULL);
    
    // Verify session ended
    TEST_ASSERT_FALSE(SerialCmd::isAuthenticated());
}

// ============================================
// TOTP Command Tests
// ============================================

void test_serial_totp_add() {
    // Auth first
    SerialCmd::execute("AUTH 123456");
    
    const char* response = SerialCmd::execute("TOTP_ADD GitHub JBSWY3DPEHPK3PXP");
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(strstr(response, "OK") != NULL);
}

void test_serial_totp_list() {
    // Auth and add account first
    SerialCmd::execute("AUTH 123456");
    SerialCmd::execute("TOTP_ADD GitHub JBSWY3DPEHPK3PXP");
    
    const char* response = SerialCmd::execute("TOTP_LIST");
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(strstr(response, "GitHub") != NULL);
}

void test_serial_totp_get() {
    // Auth and add account first
    SerialCmd::execute("AUTH 123456");
    SerialCmd::execute("TOTP_ADD GitHub JBSWY3DPEHPK3PXP");
    
    const char* response = SerialCmd::execute("TOTP_GET 0");
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(strlen(response) == 6);  // 6-digit code
}

void test_serial_totp_del() {
    // Auth and add account first
    SerialCmd::execute("AUTH 123456");
    SerialCmd::execute("TOTP_ADD GitHub JBSWY3DPEHPK3PXP");
    
    const char* response = SerialCmd::execute("TOTP_DEL 0");
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(strstr(response, "OK") != NULL);
}

// ============================================
// Password Command Tests
// ============================================

void test_serial_password_add() {
    // Auth first
    SerialCmd::execute("AUTH 123456");
    
    const char* response = SerialCmd::execute("PASSWORD_ADD GitHub user github.com secret123");
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(strstr(response, "OK") != NULL);
}

void test_serial_password_list() {
    // Auth and add entry first
    SerialCmd::execute("AUTH 123456");
    SerialCmd::execute("PASSWORD_ADD GitHub user github.com secret123");
    
    const char* response = SerialCmd::execute("PASSWORD_LIST");
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(strstr(response, "GitHub") != NULL);
}

void test_serial_password_del() {
    // Auth and add entry first
    SerialCmd::execute("AUTH 123456");
    SerialCmd::execute("PASSWORD_ADD GitHub user github.com secret123");
    
    const char* response = SerialCmd::execute("PASSWORD_DEL 0");
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(strstr(response, "OK") != NULL);
}

// ============================================
// GPG Command Tests
// ============================================

void test_serial_gpg_status() {
    // Auth first
    SerialCmd::execute("AUTH 123456");
    
    const char* response = SerialCmd::execute("GPG_STATUS");
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(strlen(response) > 0);
}

void test_serial_gpg_generate() {
    // Auth with PW3 (admin PIN)
    // Note: May need to set PW3 first
    SerialCmd::execute("AUTH 123456");
    
    const char* response = SerialCmd::execute("GPG_GENERATE 1 user@example.com");
    TEST_ASSERT_NOT_NULL(response);
    // TEST_ASSERT_TRUE(strstr(response, "OK") != NULL);
}

// ============================================
// NVS Command Tests
// ============================================

void test_serial_nvs_list() {
    const char* response = SerialCmd::execute("NVS_LIST");
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(strlen(response) > 0);
}

void test_serial_nvs_read() {
    const char* response = SerialCmd::execute("NVS_READ system name");
    TEST_ASSERT_NOT_NULL(response);
    // May succeed or fail depending on stored data
}

// ============================================
// Error Handling Tests
// ============================================

void test_serial_unknown_command() {
    const char* response = SerialCmd::execute("UNKNOWN_CMD");
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(strstr(response, "UNKNOWN") != NULL);
}

void test_serial_invalid_syntax() {
    const char* response = SerialCmd::execute("TOTP_ADD");  // Missing args
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(strstr(response, "ERROR") != NULL || strstr(response, "FAIL") != NULL);
}

void test_serial_command_requires_auth() {
    // Not authenticated
    const char* response = SerialCmd::execute("TOTP_LIST");
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(strstr(response, "AUTH") != NULL);
}

extern "C" void app_main() {
    UNITY_BEGIN();
    
    // Basic commands
    RUN_TEST(test_serial_help_command);
    RUN_TEST(test_serial_ping_command);
    RUN_TEST(test_serial_status_command);
    
    // Authentication
    RUN_TEST(test_serial_auth_success);
    RUN_TEST(test_serial_auth_failure);
    RUN_TEST(test_serial_logout);
    
    // TOTP
    RUN_TEST(test_serial_totp_add);
    RUN_TEST(test_serial_totp_list);
    RUN_TEST(test_serial_totp_get);
    RUN_TEST(test_serial_totp_del);
    
    // Password
    RUN_TEST(test_serial_password_add);
    RUN_TEST(test_serial_password_list);
    RUN_TEST(test_serial_password_del);
    
    // GPG
    RUN_TEST(test_serial_gpg_status);
    RUN_TEST(test_serial_gpg_generate);
    
    // NVS
    RUN_TEST(test_serial_nvs_list);
    RUN_TEST(test_serial_nvs_read);
    
    // Error handling
    RUN_TEST(test_serial_unknown_command);
    RUN_TEST(test_serial_invalid_syntax);
    RUN_TEST(test_serial_command_requires_auth);
    
    UNITY_END();
}
```

## References

- [Serial Commands](components/serial_cmd/) - Command module
- [Serial Commands Reference](docs/SERIAL_COMMANDS.md) - Full command list
- [Command Registry](components/serial_cmd/src/CommandRegistry.cpp) - Registration API
