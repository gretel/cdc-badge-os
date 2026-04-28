---
title: "[LOW] Peer Consent is Device-Level Only, Not Works Council-Level"
severity: LOW
domain: employee-monitoring
lens: betriebsrat-compliance
labels:
  - consent
  - works-council
  - peer-tracking
---

## Summary

The **mod_vcard** component implements **peer-level consent** for vCard exchange (when two devices connect), but lacks **Works Council-level consent** for the overall monitoring system. Under BetrVG, the Works Council must approve the system before individual employee consent is valid.

**Files Affected:**
- `components/mod_vcard/src/ble_vcard.cpp` (lines 165-180, 665-690)
- `components/mod_vcard/src/VcardModule.cpp` (lines 200-230)

**Evidence:**

```cpp
// components/mod_vcard/src/ble_vcard.cpp:165-180
static bool s_consent_pending = false;
static char s_consent_peer_name[VCARD_BLE_NAME_MAX] = {};
static uint16_t s_consent_conn_handle = INVALID_HANDLE;

static vcard_consent_callback_t s_consent_callback = nullptr;

// components/mod_vcard/src/ble_vcard.cpp:665-690 (GATT control characteristic)
s_gattChars[2].onWrite = [](uint16_t connHandle, uint16_t, const uint8_t* data, uint16_t len) -> int {
    if (len < 1) return 0;

    if (data[0] == CMD_REQUEST_EXCHANGE) {
        if (s_consent_pending || s_exchange_in_progress) {
            sendStatusNotification(connHandle, STATUS_BUSY);
            return 0;
        }

        // Peer name from scan list is not easily available here;
        // connHandle-to-address mapping would need API extension
        snprintf(s_consent_peer_name, sizeof(s_consent_peer_name), "BLE Device");

        s_consent_pending = true;
        s_consent_conn_handle = connHandle;
        
        if (s_consent_callback) {
            s_consent_callback(s_consent_peer_name);  // Only prompts user, not Works Council
        }
    }
    return 0;
};

// components/mod_vcard/src/VcardModule.cpp:200-230
static void onConsentRequest(const char* peerName) {
    // Builds simple prompt: "Exchange Request - NAME moechte vCard tauschen"
    static char promptText[256];
    snprintf(promptText, sizeof(promptText),
             "%s\n\n%s\nmoechte vCard tauschen\n\n[Y] %s\n[N] %s",
             mstr(STR_EXCHANGE_REQ), peerName, mstr(STR_ACCEPT), mstr(STR_DECLINE));
    
    s_consentView.init(mstr(STR_EXCHANGE_REQ), promptText);
    s_consentView.setYesNoCallbacks(onConsentAccept, onConsentDecline, nullptr);
    ui::ViewStack::instance().push(&s_consentView);
}
```

## Impact

Under **BetrVG §87 Abs. 1 Nr. 6**, the Works Council (Betriebsrat) must approve any system that monitors employees **before** individual consent is valid. Current issues:

1. **Missing System-Level Consent**: Only peer-to-peer consent exists, no Works Council agreement tracking
2. **No Agreement Documentation**: No field to store Works Council agreement ID or date
3. **No Audit Trail**: No record of when the system was approved by Works Council
4. **User Consent ≠ Works Council Consent**: Individual user accepting exchange doesn't satisfy BetrVG requirements

## Recommended Fix

Implement the following within ~1 hour:

1. **Add Works Council agreement structure** in `ble_vcard.h`:
   ```cpp
   typedef struct {
       char agreement_id[32];      // Works Council agreement number
       uint32_t signed_date;       // Date when agreement was signed
       char department[64];        // Department covered by agreement
       bool is_active;             // Is agreement currently valid
   } betriebsrat_agreement_t;
   
   void ble_vcard_set_works_council_agreement(const betriebsrat_agreement_t* agreement);
   bool ble_vcard_get_works_council_agreement(betriebsrat_agreement_t* out);
   bool ble_vcard_is_works_council_approved(void);
   ```

2. **Add agreement check in init()** in `ble_vcard.cpp`:
   ```cpp
   bool ble_vcard_init(void) {
       if (s_initialized) return true;
       
       // Check if Works Council agreement is active
       if (!ble_vcard_is_works_council_approved()) {
           LOG_W(TAG, "Works Council approval not configured");
           // Optionally: Disable scanning by default until approved
           s_scan_enabled = false;
       }
       
       // ... rest of init
   }
   ```

3. **Add serial command for agreement management** in `VcardModule.cpp`:
   ```cpp
   static void cmdVcardAgreement(const char* args) {
       // Usage: VCARD_AGREEMENT <id> <date_epoch> <department>
       betriebsrat_agreement_t agreement = {};
       // Parse args and set agreement
       ble_vcard_set_works_council_agreement(&agreement);
       serial::Console::printf("Agreement set: %s\r\n", agreement.agreement_id);
   }
   
   reg.registerCommand({"VCARD_AGREEMENT", "Set Works Council agreement", cmdVcardAgreement, "vcard", false});
   ```

## References

- **BetrVG §87 Abs. 1 Nr. 6**: Works Council co-determination for monitoring systems
- **BAG Urteil vom 1.12.2015 (Az.: 1 ABR 10/14)**: System approval must precede individual consent
- **GDPR Article 7**: Conditions for consent (must be informed, specific, unambiguous)
- **LAG Berlin-Brandenburg, 06.08.2020 (Az.: 13 Sa 1000/20)**: Works Council agreement is prerequisite for employee consent
