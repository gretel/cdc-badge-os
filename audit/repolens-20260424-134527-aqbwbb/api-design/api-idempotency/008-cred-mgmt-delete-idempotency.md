---
title: "[LOW] CTAP2 credMgmt deleteCredential returns error for non-existent credentials"
severity: LOW
domain: api-design/api-idempotency
lens: api-idempotency
labels:
  - audit:api-design/api-idempotency
---

## Summary
The CTAP2 `authenticatorCredentialManagement` DELETE command (`CRED_MGMT_DELETE_CREDENTIAL`) returns `CTAP2_ERR_NO_CREDENTIALS` when trying to delete a credential that doesn't exist. For true idempotency, deleting a non-existent credential should be idempotent - either return success (credential is gone) or a more appropriate status.

**Location:** `components/mod_fido2/src/ctap2.cpp:3335-3364` (CRED_MGMT_DELETE_CREDENTIAL case)

## Impact
- **Non-idempotent DELETE:** Same delete request produces different results (success on first call, error on second)
- **Client retry confusion:** Clients retrying a delete might get an error thinking it failed
- **Inconsistent with REST idempotency:** DELETE /resource/123 should return 200 or 204 even if resource was already deleted
- **FIDO2 spec ambiguity:** CTAP2 spec doesn't explicitly define behavior for deleting non-existent credentials

## Evidence
```cpp
// components/mod_fido2/src/ctap2.cpp:3335-3364
case CRED_MGMT_DELETE_CREDENTIAL:
    {
        if (!has_cred_id) {
            response[0] = CTAP2_ERR_MISSING_PARAMETER;
            *response_len = 1;
            return CTAP2_ERR_MISSING_PARAMETER;
        }

        // Find credential by ID
        int8_t slot = fido2_storage_find_slot_by_cred_id(cred_id, cred_id_len);
        if (slot < 0) {
            response[0] = CTAP2_ERR_NO_CREDENTIALS;  // Error on second call
            *response_len = 1;
            return CTAP2_ERR_NO_CREDENTIALS;
        }

        // Delete it
        if (!fido2_storage_delete_credential(slot)) {
            response[0] = CTAP2_ERR_OTHER;
            *response_len = 1;
            return CTAP2_ERR_OTHER;
        }

        LOG_I("CTAP2", "credMgmt deleted credential slot %d", slot);

        // Success - empty response
        response[0] = CTAP2_OK;
        *response_len = 1;
        return CTAP2_OK;
    }
```

Behavior:
- **First call (credential exists):** `CTAP2_OK`
- **Second call (credential already deleted):** `CTAP2_ERR_NO_CREDENTIALS`

## Recommended Fix
Make DELETE idempotent by returning success when credential doesn't exist:

```cpp
case CRED_MGMT_DELETE_CREDENTIAL:
    {
        if (!has_cred_id) {
            response[0] = CTAP2_ERR_MISSING_PARAMETER;
            *response_len = 1;
            return CTAP2_ERR_MISSING_PARAMETER;
        }

        // Find credential by ID
        int8_t slot = fido2_storage_find_slot_by_cred_id(cred_id, cred_id_len);
        
        if (slot < 0) {
            // Credential not found - already deleted? Return success for idempotency
            LOG_I("CTAP2", "credMgmt delete: credential not found (already deleted?)");
            response[0] = CTAP2_OK;  // Idempotent success
            *response_len = 1;
            return CTAP2_OK;
        }

        // Delete it
        if (!fido2_storage_delete_credential(slot)) {
            response[0] = CTAP2_ERR_OTHER;
            *response_len = 1;
            return CTAP2_ERR_OTHER;
        }

        LOG_I("CTAP2", "credMgmt deleted credential slot %d", slot);

        response[0] = CTAP2_OK;
        *response_len = 1;
        return CTAP2_OK;
    }
```

Alternatively, use a more specific error code that indicates "already deleted":
```cpp
if (slot < 0) {
    response[0] = CTAP2_OK;  // Treat as idempotent success
    *response_len = 1;
    return CTAP2_OK;
}
```

## References
- REST API idempotency: DELETE should return 200/204 even if resource doesn't exist
- FIDO2 CTAP2 spec: `authenticatorCredentialManagement` delete subcommand
- Similar pattern: FIDO2 `authenticatorMakeCredential` replaces existing credentials (idempotent)
