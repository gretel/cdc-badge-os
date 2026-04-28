---
title: "[MEDIUM] Uninitialized stack struct in makeCredential handler"
severity: MEDIUM
domain: security
lens: toolgate/session-zap
labels:
  - "audit:toolgate/session-zap"
---

## Summary

The `MakeCredentialParams` struct is declared on the stack without initialization in `ctap2_make_credential()`. While the `clear()` method exists, it is never called, potentially leaving sensitive stack data in response fields.

**Location:** `components/mod_fido2/src/ctap2.cpp:1181`

## Impact

The `MakeCredentialParams` struct contains 450+ bytes of data:
```cpp
struct MakeCredentialParams {
    uint8_t client_data_hash[32];
    char rp_id[FIDO2_RP_ID_MAX_LEN];       // 64 bytes
    uint8_t rp_id_hash[32];
    uint8_t user_id[FIDO2_USER_ID_MAX_LEN]; // 65 bytes
    uint8_t user_id_len;
    char user_name[FIDO2_USER_NAME_MAX_LEN]; // 64 bytes
    bool rk;
    uint8_t cred_protect;
    int alg;
    bool option_uv;
    bool option_up;
    char appid_exclude[256];
    bool has_appid_exclude;
    uint8_t pin_uv_auth_param[64];
    size_t pin_uv_auth_param_len;
    uint8_t pin_uv_auth_protocol;
    bool has_client_data;
    bool has_rp;
    bool has_user;
    bool has_alg;
};
```

**Potential data leakage:**
1. **Previous stack contents**: Arrays like `rp_id`, `user_name`, `appid_exclude`, `pin_uv_auth_param` may contain residual data from previous function calls
2. **PIN/Token exposure**: `pin_uv_auth_param[64]` could leak previous PIN authentication tokens
3. **Credential data**: `user_id[65]` could leak previous user handles
4. **RP ID leakage**: `appid_exclude[256]` could leak previously enumerated RP IDs

While the CBOR parser fills in fields based on what's in the request, fields not present in the request remain uninitialized. For example:
- If `appidExclude` is not sent, `appid_exclude[256]` remains uninitialized
- If `pinUvAuthParam` is not sent, `pin_uv_auth_param[64]` remains uninitialized
- Boolean flags default to garbage values

## Evidence

**File: `components/mod_fido2/src/ctap2.cpp`**

Line 675-679 (clear method defined):
```cpp
void clear() {
    memset(this, 0, sizeof(*this));
    option_up = true;  // Default: UP required
}
```

Line 1181 (struct declared, but clear() never called):
```cpp
uint8_t ctap2_make_credential(const uint8_t *params, uint16_t params_len,
                               uint8_t *response, uint16_t *response_len) {
    MakeCredentialParams p;  // <-- NOT initialized!
    
    // Step 1: Parse all CBOR parameters
    uint8_t status = parse_make_credential_params(params, params_len, &p);
    // ...
}
```

Line 1307-1315 (similar issue in getAssertion):
```cpp
uint8_t ctap2_get_assertion(const uint8_t *params, uint16_t params_len,
                             uint8_t *response, uint16_t *response_len) {
    GetAssertionParams p;
    // No clear() call here either!
```

## Recommended Fix

Initialize the struct before use:

```cpp
uint8_t ctap2_make_credential(const uint8_t *params, uint16_t params_len,
                               uint8_t *response, uint16_t *response_len) {
    MakeCredentialParams p;
    p.clear();  // Add this line
    
    // Step 1: Parse all CBOR parameters
    uint8_t status = parse_make_credential_params(params, params_len, &p);
    // ...
}
```

Similarly for `GetAssertionParams`:
```cpp
uint8_t ctap2_get_assertion(const uint8_t *params, uint16_t params_len,
                             uint8_t *response, uint16_t *response_len) {
    GetAssertionParams p;
    p.clear();  // Add this line
    // ...
}
```

## References

- CWE-457: Use of Uninitialized Variable
- [OWASP: Initialization of Variables](https://cheatsheetseries.owasp.org/cheatsheets/C_Coding_Cheat_Sheet.html#initialization-of-variables)
- [FIDO2 CTAP2 Specification](https://fidoalliance.org/specs/fido-v2.1-rd-20201209/fido-client-to-authenticator-protocol-v2.1-rd-20201209.html)
