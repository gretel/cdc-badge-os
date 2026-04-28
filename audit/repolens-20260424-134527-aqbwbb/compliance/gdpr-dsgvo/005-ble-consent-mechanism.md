---
title: "[LOW] BLE vCard Exchange Consent Mechanism Could Be More Explicit"
severity: LOW
domain: gdpr-dsgvo
lens: compliance/gdpr-dsgvo
labels:
  - "audit:compliance/gdpr-dsgvo"
---

## Summary
The BLE vCard exchange mechanism (`components/mod_vcard/src/ble_vcard.cpp`) implements consent through a two-way exchange protocol, but the consent flow could be more explicit to better comply with GDPR requirements for informed consent (Article 7). The current implementation relies on implicit consent through the exchange process rather than explicit opt-in for each data field.

## Impact
**Legal Risk**: While the current implementation has user confirmation (numeric comparison, passkey), it doesn't explicitly inform users about what specific data will be exchanged before confirmation.

**User Awareness**: Users may not fully understand that accepting an exchange means receiving and storing another person's vCard data.

**Best Practice**: GDPR encourages explicit, informed consent with clear information about what data is being processed.

## Evidence

### Current Consent Flow
From `docs/ble_vcard_protocol.md` section 3.4 and `ble_vcard.cpp`:

1. **Connection + Pairing** (lines 370-400 in ble_vcard.cpp)
   ```cpp
   // Line 370-399: onConnect()
   // Service discovery happens automatically
   // No user prompt about data exchange yet
   ```

2. **Exchange Request** (lines 516-540)
   ```cpp
   // Line 516: readPeerVcard()
   // Reads vCard without showing content to user first
   ```

3. **Storage Decision** (lines 291-318)
   ```cpp
   // Line 291-318: onCharacteristicRead()
   // Automatically stores vCard after reading
   // No user confirmation about what will be stored
   if (vcard_store_add(s_rx_vcard, len, err, sizeof(err))) {
       LOG_I(TAG, "Peer vCard stored");
   }
   ```

### Consent Callbacks (Underutilized)
```cpp
// Line 174-175: Callback definitions
static vcard_consent_callback_t s_consent_callback = nullptr;
static vcard_exchange_complete_callback_t s_exchange_complete_callback = nullptr;

// Line 548: Callback usage (limited)
vcard_peer_t peer = {};
// ... peer data populated ...
if (s_consent_callback) {
    s_consent_callback(&peer);  // Called but may not show full details
}
```

### Missing Consent Elements
1. **Pre-exchange disclosure**: No display of what data fields will be exchanged
2. **Field-level consent**: No option to select which fields to share
3. **Post-exchange review**: No confirmation screen showing what was received
4. **Right to decline specific fields**: All-or-nothing approach

## Recommended Fix

### Option 1: Add Pre-Exchange Disclosure (~30 min)

Add function to show exchange summary before confirmation:
```cpp
/**
 * \brief Shows vCard exchange summary before user confirms.
 * \param peer Peer information.
 * \return `true` if user confirms exchange.
 */
static bool showExchangeConsentDialog(const vcard_peer_t* peer) {
    // Parse peer vCard to extract key fields
    char name[64], phone[32], email[64];
    vcard_extract_fields(s_rx_vcard, name, phone, email);
    
    // Display on E-Paper:
    // "Exchange with: {name}"
    // "They will receive: your vCard"
    // "You will receive: {name}, {phone}, {email}"
    // "Y=Accept, N=Decline"
    
    return waitForUserConfirmation();
}

// Call in exchange flow (line ~305):
if (!showExchangeConsentDialog(&peer)) {
    sendStatusNotification(s_client_conn_handle, STATUS_DECLINED);
    return;
}
```

### Option 2: Add Field-Level Selection (~1 hour)

Create a view for field selection:
```cpp
/**
 * \brief Allows user to select which vCard fields to share.
 * \param out Selected fields bitmask.
 * \return `true` if user confirms selection.
 */
static bool selectVcardFields(uint8_t* out) {
    // Show checkboxes:
    // [x] Name
    // [x] Phone
    // [ ] Email
    // [x] Address
    // Y=Save, N=Cancel
}
```

### Option 3: Add Post-Exchange Summary (~30 min)

After successful exchange, show what was received:
```cpp
/**
 * \brief Shows summary of received vCard data.
 * \param vcard Received vCard.
 */
static void showReceivedVcardSummary(const char* vcard) {
    char name[64], phone[32];
    vcard_extract_fields(vcard, name, phone, NULL);
    
    // Display: "Received: {name}, {phone}"
    // Display: "Stored in slot X"
    // Display: "Press any key to continue"
}

// Call after line 308:
showReceivedVcardSummary(s_rx_vcard);
```

### Option 4: Add Consent Logging (~30 min)

Log consent events for audit trail:
```cpp
/**
 * \brief Logs vCard exchange consent event.
 * \param peer Peer information.
 * \param accepted `true` if exchange was accepted.
 */
static void logConsentEvent(const vcard_peer_t* peer, bool accepted) {
    struct {
        uint32_t timestamp;
        uint8_t peer_addr[6];
        bool accepted;
        uint8_t fields_shared;
    } log_entry;
    
    log_entry.timestamp = time(nullptr);
    memcpy(log_entry.peer_addr, peer->addr, 6);
    log_entry.accepted = accepted;
    
    // Store in NVS for audit trail
    nvs_set_blob(nvs, "consent_last", &log_entry, sizeof(log_entry));
}
```

## References
- **Art. 7 DSGVO** - Conditions for consent
- **Art. 4(11) DSGVO** - Definition of consent
- **Article 29 Working Party Guidelines on Consent** - https://ec.europa.eu/justice/data-protection/article-29/documentation/opinion-recommendation/files/2017/wp251_rev.01_en.pdf
- **BfD Consent Checklist** - https://www.bfdi.bund.de/
