---
title: "[MEDIUM] FIDO2 user-presence consent not recorded with timestamp and RP details"
severity: MEDIUM
domain: compliance
lens: consent-flows
labels:
  - consent-persistence
  - fido2
  - audit-trail
---

## Summary

When a user approves or denies a FIDO2/WebAuthn authentication or registration request, the consent decision is processed but **not recorded** with any persistent metadata. The FIDO2 module (`components/mod_fido2/src/Fido2Ui.cpp`) shows the user a prompt with the Relying Party (RP) ID and waits for Y/N confirmation, but once the decision is made, no record is kept of:

- Which RP ID requested authentication
- What action was performed (register vs. authenticate)
- When the consent was given
- Whether it was approved or denied

**Location:** `components/mod_fido2/src/Fido2Ui.cpp:302-330` (promptComplete function)

```cpp
static void promptComplete(fido2_user_presence_result_t result) {
    s_promptActive = false;

    if (result == FIDO2_UP_APPROVED) {
        ui::showToastSuccess(ui::tr(ui::StringId::OK), 2000);
    } else {
        ui::showToastError(ui::tr(ui::StringId::FAILED), 2000);
    }

    restoreView();
    // ... view stack management ...

    s_promptResult = result;
    if (s_promptSem) {
        xSemaphoreGive(s_promptSem);
    }
}
```

The RP ID is stored in `s_promptRpId` (line 98) and the action in `s_promptAction` (line 99), but these are **volatile static variables** that are lost after the prompt completes.

## Impact

1. **No audit trail for FIDO2 approvals:** When a user approves a FIDO2 authentication for "github.com", there is no way to later retrieve:
   - That the approval happened
   - When it happened
   - What the sign count was at the time

2. **Troubleshooting difficulty:** If a user reports "I didn't approve this login", there is no record to check.

3. **Compliance gap:** While FIDO2 is primarily for authentication (not data processing), having an audit trail of approvals helps with:
   - Security investigations
   - Understanding device usage patterns
   - Providing transparency to users

4. **Lost context:** The device could show a "Recent FIDO2 Activity" screen, but without stored records, this is impossible.

## Evidence

**Fido2Ui.cpp:95-104** - Volatile prompt state:
```cpp
static SemaphoreHandle_t s_promptSem = nullptr;
static volatile fido2_user_presence_result_t s_promptResult = FIDO2_UP_PENDING;
static char s_promptRpId[FIDO2_RP_ID_MAX_LEN] = {};  // RP ID stored temporarily
static fido2_action_t s_promptAction = FIDO2_ACTION_AUTHENTICATE;  // Action stored temporarily
static uint8_t s_promptReturnDepth = 0;
static ui::IView* s_promptReturnView = nullptr;
static bool s_promptWasLocked = false;
static bool s_promptBacklightWasOn = false;
static volatile bool s_promptActive = false;
```

**Fido2Ui.cpp:460-591** - User presence callback processes consent but doesn't record it:
```cpp
fido2_user_presence_result_t fido2_ui_user_presence_callback(
    const char* rp_id,
    fido2_action_t action,
    const char* user_name
) {
    // ... logs the request ...
    strncpy(s_promptRpId, rp_id ? rp_id : "Unknown", sizeof(s_promptRpId) - 1);
    s_promptAction = action;
    // ... shows prompt ...
    // When user presses Y/N, promptComplete() is called
    // But s_promptRpId and s_promptAction are never persisted!
}
```

**ctap2.cpp:508-534** - User presence is awaited but result is only boolean:
```cpp
static bool wait_for_user_presence(const char *rp_id, fido2_action_t action, const char *user_name) {
    LOG_I("CTAP2", "User presence required for %s at %s",
          action == FIDO2_ACTION_REGISTER ? "registration" : "authentication",
          rp_id ? rp_id : "unknown");

    fido2_user_presence_result_t result = fido2_request_user_presence(rp_id, action, user_name);

    switch (result) {
        case FIDO2_UP_APPROVED:
            LOG_I("CTAP2", "User presence approved");
            return true;
        // ...
    }
}
```

Note that the RP ID and action are logged but not stored anywhere persistent.

## Recommended Fix

Add NVS-based consent recording for FIDO2 approvals:

1. **Define a consent record structure:**
```cpp
typedef struct {
    uint32_t timestamp_ms;      // Uptime when consent given
    char rp_id[FIDO2_RP_ID_MAX_LEN];
    uint8_t action;             // FIDO2_ACTION_REGISTER or FIDO2_ACTION_AUTHENTICATE
    bool approved;              // User decision
    uint32_t sign_count;        // Sign count at time of approval (for audit)
} fido2_consent_record_t;
```

2. **Add storage functions:**
```cpp
// Record a FIDO2 consent decision
void fido2_record_consent(const char* rp_id, fido2_action_t action, bool approved);

// Get recent consent history (for display in UI)
uint8_t fido2_get_consent_history(fido2_consent_record_t* out, uint8_t max_records);

// Clear consent history
void fido2_clear_consent_history(void);
```

3. **Call recording function in promptComplete:**
```cpp
static void promptComplete(fido2_user_presence_result_t result) {
    s_promptActive = false;

    // Record the consent decision
    fido2_record_consent(s_promptRpId, s_promptAction, result == FIDO2_UP_APPROVED);

    if (result == FIDO2_UP_APPROVED) {
        ui::showToastSuccess(ui::tr(ui::StringId::OK), 2000);
    } else {
        ui::showToastError(ui::tr(ui::StringId::FAILED), 2000);
    }

    // ... rest of function ...
}
```

4. **Implementation using NVS:**
```cpp
void fido2_record_consent(const char* rp_id, fido2_action_t action, bool approved) {
    nvs_handle_t nvs;
    if (nvs_open("fido2_consent", NVS_READWRITE, &nvs) != ESP_OK) {
        return;
    }

    // Get next index
    uint16_t idx = nvs_get_u16(nvs, "next_idx", 0);

    // Store record with index as key
    fido2_consent_record_t record = {
        .timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS,
        .action = action,
        .approved = approved,
        .sign_count = fido2_get_auth_counter()
    };
    strncpy(record.rp_id, rp_id, FIDO2_RP_ID_MAX_LEN - 1);

    size_t key_len = snprintf(NULL, 0, "rec_%d", idx) + 1;
    char* key = malloc(key_len);
    snprintf(key, key_len, "rec_%d", idx);

    nvs_set_blob(nvs, key, &record, sizeof(record));
    nvs_set_u16(nvs, "next_idx", idx + 1);
    nvs_commit(nvs);

    nvs_close(nvs);
    free(key);
}
```

5. **Optional: Add UI view to show recent approvals** in the FIDO2 module menu.

## References

- FIDO2 WebAuthn spec: User presence is the primary consent mechanism for authentication
- ISO/IEC 29100: Privacy framework - Audit trail requirements
- GDPR Article 5(2) "Accountability": Need to demonstrate compliance with consent requirements

</content>