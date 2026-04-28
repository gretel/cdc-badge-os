---
title: "[MEDIUM] Long function ctap2_make_credential() with sequential validation steps"
severity: MEDIUM
domain: Code Quality
lens: cyclomatic-complexity
labels:
  - "audit:code-quality/complexity"
---

## Summary

The `ctap2_make_credential()` function in `components/mod_fido2/src/ctap2.cpp` (lines 1179-1252) is a long function (~73 lines) that handles the complete makeCredential workflow. While it uses early returns well, the function orchestrates 8 sequential steps with different validation and processing logic.

**Estimated Cyclomatic Complexity: ~12** (slightly above threshold)
**Function Length: ~73 lines** (threshold is 50)

## Impact

**Readability:**
- Function spans most of a screen, requiring scrolling
- Hard to see the overall flow at a glance
- Each step has different error handling patterns

**Maintenance:**
- Adding new validation steps requires understanding all existing steps
- Debugging requires tracing through many conditional branches
- Test coverage requires mocking many dependencies

## Evidence

**File:** `components/mod_fido2/src/ctap2.cpp:1179-1252`

**Code excerpt:**
```cpp
uint8_t ctap2_make_credential(const uint8_t *params, uint16_t params_len,
                               uint8_t *response, uint16_t *response_len) {
    MakeCredentialParams p;

    // Step 1: Parse all CBOR parameters
    uint8_t status = parse_make_credential_params(params, params_len, &p);
    if (status != CTAP2_OK) {
        response[0] = status;
        *response_len = 1;
        return status;
    }

    LOG_I("CTAP2", "makeCredential rp_id=%s rk=%d uv=%d up=%d alg=%d pinProto=%d pinAuthLen=%zu",
          p.rp_id[0] ? p.rp_id : "(none)", p.rk, p.option_uv, p.option_up, p.alg,
          p.pin_uv_auth_protocol, p.pin_uv_auth_param_len);

    // Step 2: Check appidExclude extension
    status = check_appid_exclude(&p);
    if (status != CTAP2_OK) {
        response[0] = status;
        *response_len = 1;
        return status;
    }

    // Step 3: Verify PIN/UV auth parameter
    status = verify_pin_uv_auth(&p);
    if (status != CTAP2_OK) {
        response[0] = status;
        *response_len = 1;
        return status;
    }

    // Step 4: Handle browser probe requests
    if (is_browser_probe(p.rp_id)) {
        return handle_browser_probe(&p, response, response_len);
    }

    // Step 5: Determine curve from algorithm
    uint8_t curve;
    if (p.alg == COSE_ALG_ES256) {
        curve = CDC_CURVE_P256;
    } else if (p.alg == COSE_ALG_EDDSA) {
        curve = CDC_CURVE_ED25519;
    } else {
        response[0] = CTAP2_ERR_UNSUPPORTED_ALGORITHM;
        *response_len = 1;
        return CTAP2_ERR_UNSUPPORTED_ALGORITHM;
    }

    // Step 6: Validate options
    if (p.option_uv) {
        response[0] = CTAP2_ERR_UNSUPPORTED_OPTION;
        *response_len = 1;
        return CTAP2_ERR_UNSUPPORTED_OPTION;
    }
    if (!p.option_up) {
        response[0] = CTAP2_ERR_INVALID_OPTION;
        *response_len = 1;
        return CTAP2_ERR_INVALID_OPTION;
    }

    // Step 7: Request user presence
    if (!wait_for_user_presence(p.rp_id, FIDO2_ACTION_REGISTER, p.user_name)) {
        response[0] = CTAP2_ERR_OPERATION_DENIED;
        *response_len = 1;
        return CTAP2_ERR_OPERATION_DENIED;
    }

    LOG_I("CTAP2", "User presence OK, creating credential (curve=%d)...", curve);

    // Step 8: Create credential and build response
    return create_credential_and_respond(&p, curve, response, response_len);
}
```

**Branching count:**
- Line 1184: `if (status != CTAP2_OK)` (+1)
- Line 1196: `if (status != CTAP2_OK)` (+1)
- Line 1205: `if (status != CTAP2_OK)` (+1)
- Line 1211: `if (is_browser_probe(p.rp_id))` (+1)
- Line 1216: `if (p.alg == COSE_ALG_ES256)` (+1)
- Line 1218: `else if (p.alg == COSE_ALG_EDDSA)` (+1)
- Line 1223: `if (p.option_uv)` (+1)
- Line 1228: `if (!p.option_up)` (+1)
- Line 1234: `if (!wait_for_user_presence(...))` (+1)

Total: ~12 independent paths

## Recommended Fix

**Extract validation steps into a validation pipeline:**

1. Create a validation context struct:
```cpp
struct MakeCredContext {
    MakeCredentialParams params;
    uint8_t curve;
    bool is_probe;
};

struct ValidationResult {
    uint8_t status;
    const char* step_name;
};
```

2. Create validation step functions:
```cpp
static ValidationResult parseParams(const uint8_t *params, uint16_t params_len, MakeCredContext *ctx) {
    uint8_t status = parse_make_credential_params(params, params_len, &ctx->params);
    return {status, "parse_params"};
}

static ValidationResult checkAppidExclude(const MakeCredContext *ctx) {
    uint8_t status = check_appid_exclude(&ctx->params);
    return {status, "appid_exclude"};
}

static ValidationResult verifyPinAuth(const MakeCredContext *ctx) {
    uint8_t status = verify_pin_uv_auth(&ctx->params);
    return {status, "pin_auth"};
}

static ValidationResult checkBrowserProbe(MakeCredContext *ctx) {
    ctx->is_probe = is_browser_probe(ctx->params.rp_id);
    return {CTAP2_OK, "browser_probe"};
}

static ValidationResult determineCurve(MakeCredContext *ctx) {
    if (ctx->params.alg == COSE_ALG_ES256) {
        ctx->curve = CDC_CURVE_P256;
    } else if (ctx->params.alg == COSE_ALG_EDDSA) {
        ctx->curve = CDC_CURVE_ED25519;
    } else {
        return {CTAP2_ERR_UNSUPPORTED_ALGORITHM, "curve"};
    }
    return {CTAP2_OK, "curve"};
}

static ValidationResult validateOptions(const MakeCredContext *ctx) {
    if (ctx->params.option_uv) {
        return {CTAP2_ERR_UNSUPPORTED_OPTION, "options"};
    }
    if (!ctx->params.option_up) {
        return {CTAP2_ERR_INVALID_OPTION, "options"};
    }
    return {CTAP2_OK, "options"};
}

static ValidationResult waitForUserPresence(const MakeCredContext *ctx) {
    bool approved = wait_for_user_presence(ctx->params.rp_id, FIDO2_ACTION_REGISTER, ctx->params.user_name);
    return {approved ? CTAP2_OK : CTAP2_ERR_OPERATION_DENIED, "user_presence"};
}
```

3. Create validation pipeline:
```cpp
static uint8_t runValidationPipeline(const uint8_t *params, uint16_t params_len, MakeCredContext *ctx) {
    struct {
        ValidationResult (*step)(const uint8_t*, uint16_t, MakeCredContext*);
        bool takes_context;
    } pipeline[] = {
        {[](const uint8_t *p, uint16_t l, MakeCredContext *c) {
             return parseParams(p, l, c);
         }, false},
        {[](const uint8_t *p, uint16_t l, MakeCredContext *c) {
             return checkAppidExclude(c);
         }, true},
        {[](const uint8_t *p, uint16_t l, MakeCredContext *c) {
             return verifyPinAuth(c);
         }, true},
        {[](const uint8_t *p, uint16_t l, MakeCredContext *c) {
             return checkBrowserProbe(c);
         }, true},
        {[](const uint8_t *p, uint16_t l, MakeCredContext *c) {
             return determineCurve(c);
         }, true},
        {[](const uint8_t *p, uint16_t l, MakeCredContext *c) {
             return validateOptions(c);
         }, true},
        {[](const uint8_t *p, uint16_t l, MakeCredContext *c) {
             return waitForUserPresence(c);
         }, true},
    };

    for (auto &step : pipeline) {
        ValidationResult result = step.step(params, params_len, ctx);
        if (result.status != CTAP2_OK) {
            return result.status;
        }
        // Handle probe special case
        if (ctx->is_probe) {
            return CTAP2_OK;  // Will be handled after pipeline
        }
    }
    return CTAP2_OK;
}
```

4. Simplified main function:
```cpp
uint8_t ctap2_make_credential(const uint8_t *params, uint16_t params_len,
                               uint8_t *response, uint16_t *response_len) {
    MakeCredContext ctx = {};

    uint8_t status = runValidationPipeline(params, params_len, &ctx);
    if (status != CTAP2_OK) {
        response[0] = status;
        *response_len = 1;
        return status;
    }

    // Handle probe case
    if (ctx.is_probe) {
        return handle_browser_probe(&ctx.params, response, response_len);
    }

    LOG_I("CTAP2", "User presence OK, creating credential (curve=%d)...", ctx.curve);
    return create_credential_and_respond(&ctx.params, ctx.curve, response, response_len);
}
```

**Expected result:**
- Main function reduced to ~25 lines
- Each validation step is ~5-10 lines
- Pipeline is easily extensible
- Better testability with isolated steps

## References

- [Pipeline Pattern](https://refactoring.com/catalog/replaceComplexConditionWithPipeline.html)
- [Extract Function refactoring](https://refactoring.com/catalog/extractFunction.html)
