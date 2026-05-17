---
title: "[MEDIUM] Inconsistent use of OpenPGP Data Object tag constants"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "magic-values"
---

## Summary
OpenPGP Data Object (DO) tag constants are defined in `openpgp.h` but are not consistently used throughout the implementation. Some switch cases use the defined constants while others use hardcoded hex literals, making the code harder to maintain and understand.

**Files affected:**
- `components/mod_gpg/src/openpgp/openpgp.cpp:906-927` - SET_DATA implementation

**Defined constants (in `openpgp.h:47-77`):**
```cpp
#define DO_AID              0x004F  // Application Identifier
#define DO_CARDHOLDER       0x0065  // Cardholder Related Data
#define DO_NAME             0x005B  // Name (Cardholder)
#define DO_LANG_PREF        0x5F2D  // Language preference
#define DO_SEX              0x5F35  // Sex
#define DO_FP_SIG           0x00C7  // Fingerprint Signature key
#define DO_FP_DEC           0x00C8  // Fingerprint Decryption key
#define DO_FP_AUT           0x00C9  // Fingerprint Authentication key
// ... (24 more constants)
```

**Inconsistent usage:**
- Line 906: Uses magic `0x005B` instead of `DO_NAME`
- Line 917: Uses magic `0x5F2D` instead of `DO_LANG_PREF`
- Line 927: Uses magic `0x5F35` instead of `DO_SEX`
- Line 936: Uses constant `DO_FP_SIG` (consistent)
- Line 945: Uses constant `DO_FP_DEC` (consistent)
- Line 953: Uses constant `DO_FP_AUT` (consistent)

## Impact
- **Maintainability**: Adding new DO tags requires knowing which locations use constants vs magic numbers
- **Readability**: `case 0x005B:` is less clear than `case DO_NAME:`
- **Consistency**: Developers can't rely on a single pattern when modifying code
- **Error-prone**: Easy to use wrong hex value when adding new tags

## Evidence
```cpp
// components/mod_gpg/src/openpgp/openpgp.cpp:906-933
uint16_t tag = (apdu->p1 << 8) | apdu->p2;

switch (tag) {
    // Cardholder data (0x5B: Name, part of 0x65)
    case 0x005B:  // Magic: Should use DO_NAME
        if (apdu->lc < sizeof(cardholder_name)) {
            memcpy(cardholder_name, apdu->data, apdu->lc);
            cardholder_name[apdu->lc] = '\0';
            save_state_to_nvs();
            ESP_LOGI(TAG, "Cardholder name set: %s", cardholder_name);
            return apdu_sw(resp, SW_OK);
        }
        return apdu_sw(resp, SW_WRONG_LENGTH);

    // Language preference
    case 0x5F2D:  // Magic: Should use DO_LANG_PREF
        if (apdu->lc < sizeof(cardholder_lang)) {
            memcpy(cardholder_lang, apdu->data, apdu->lc);
            cardholder_lang[apdu->lc] = '\0';
            save_state_to_nvs();
            return apdu_sw(resp, SW_OK);
        }
        return apdu_sw(resp, SW_WRONG_LENGTH);

    // Sex
    case 0x5F35:  // Magic: Should use DO_SEX
        if (apdu->lc == 1) {
            cardholder_sex = apdu->data[0];
            save_state_to_nvs();
            return apdu_sw(resp, SW_OK);
        }
        return apdu_sw(resp, SW_WRONG_LENGTH);

    // Fingerprints (consistent usage)
    case DO_FP_SIG:  // Good: Uses defined constant
        if (apdu->lc == 20) {
            memcpy(fingerprint_sig, apdu->data, 20);
            save_state_to_nvs();
            ESP_LOGI(TAG, "Fingerprint SIG stored");
            return apdu_sw(resp, SW_OK);
        }
        return apdu_sw(resp, SW_WRONG_LENGTH);
```

## Recommended Fix
1. **Replace magic hex literals** with defined constants in `openpgp.cpp`:
    ```cpp
    // Before:
    case 0x005B:  // Cardholder name
        // ...
    
    // After:
    case DO_NAME:  // Cardholder name
        // ...
    
    // Before:
    case 0x5F2D:  // Language preference
        // ...
    
    // After:
    case DO_LANG_PREF:  // Language preference
        // ...
    
    // Before:
    case 0x5F35:  // Sex
        // ...
    
    // After:
    case DO_SEX:  // Sex
        // ...
    ```

2. **Search for all other magic DO tags** and replace them:
    ```bash
    grep -n "case 0x[0-9A-Fa-f]\{2,4\}:" components/mod_gpg/src/openpgp/openpgp.cpp
    ```

3. **Add static analysis rule** (optional):
    Consider adding a lint rule or comment to enforce using constants:
    ```cpp
    // Prefer DO_* constants over raw hex literals for OpenPGP data objects
    ```

## References
- [OpenPGP Smart Card Application 3.4.1 Specification](https://www.openpgp.org/application/)
- [ISO/IEC 7816-4 Data Objects](https://www.iso.org/standard/74660.html)
