---
title: "[MEDIUM] Extensive test code duplication across functional test files"
severity: MEDIUM
domain: testing
lens: test-anti-patterns
labels:
  - "audit:testing/test-anti-patterns"
---

## Summary
The functional tests in `third_party/libtropic/tests/functional/` contain significant code duplication. Nearly identical cleanup functions, test setup patterns, and assertion sequences are repeated across 15+ test files instead of being extracted into shared helpers.

**Affected files (12+ files with duplicated cleanup patterns):**
- `lt_test_rev_ecc_key_generate.c`
- `lt_test_rev_ecc_key_store.c`
- `lt_test_rev_ecdsa_sign.c`
- `lt_test_rev_eddsa_sign.c`
- `lt_test_rev_erase_r_config.c`
- `lt_test_rev_get_info_req_bootloader.c`
- `lt_test_rev_get_log_req.c`
- `lt_test_rev_mcounter.c`
- `lt_test_rev_r_mem.c`
- `lt_test_rev_resend_req.c`
- `lt_test_rev_startup_req.c`
- `lt_test_rev_write_r_config.c`

## Impact
1. **Maintenance burden**: Changes to cleanup logic require updating 12+ files.
2. **Inconsistent bug fixes**: If a bug is found in the cleanup pattern, it may be fixed in some files but not others.
3. **Larger test suite**: Duplicated code increases build time and binary size.
4. **Harder to extend**: Adding new test types requires copying more boilerplate.

## Evidence
**Example 1: Duplicate cleanup function structure**

`lt_test_rev_ecc_key_generate.c:20-70` and `lt_test_rev_ecc_key_store.c:40-91` have nearly identical cleanup functions:

```c
// Both files have this exact pattern:
static lt_ret_t lt_test_rev_*_cleanup(void)
{
    lt_ret_t ret;
    uint8_t read_pub_key[TR01_CURVE_P256_PUBKEY_LEN];
    lt_ecc_curve_type_t curve;
    lt_ecc_key_origin_t origin;

    LT_LOG_INFO("Starting secure session with slot %d", ...);
    ret = lt_verify_chip_and_start_secure_session(g_h, ...);
    // ... 15+ lines of identical session setup ...

    LT_LOG_INFO("Erasing all ECC key slots");
    for (uint8_t i = TR01_ECC_SLOT_0; i <= TR01_ECC_SLOT_31; i++) {
        // ... 10+ lines of identical loop ...
    }

    LT_LOG_INFO("Aborting secure session");
    ret = lt_session_abort(g_h);
    // ... 5+ lines of identical abort/deinit ...
    return LT_OK;
}
```

**Example 2: Duplicate test setup**

Every test file repeats the same initialization pattern:
```c
LT_LOG_INFO("Initializing handle");
LT_TEST_ASSERT(LT_OK, lt_init(h));

LT_LOG_INFO("Starting Secure Session with key %d", ...);
LT_TEST_ASSERT(LT_OK, lt_verify_chip_and_start_secure_session(...));
LT_LOG_LINE();

lt_test_cleanup_function = &lt_test_*_cleanup;
```

**Example 3: Duplicate ECC slot iteration**

Tests iterate through all 32 ECC slots with identical logic in at least 4 files:
```c
for (uint8_t i = TR01_ECC_SLOT_0; i <= TR01_ECC_SLOT_31; i++) {
    LT_LOG_INFO();
    LT_LOG_INFO("Testing ECC key slot #%" PRIu8 "...", i);
    // ... 20+ lines of per-slot test logic ...
}
```

## Recommended Fix
Extract common patterns into shared test helpers:

1. **Create `lt_test_helpers.c/h`** with:
```c
// Common cleanup for ECC slot tests
lt_ret_t lt_test_cleanup_ecc_slots(lt_handle_t *h);

// Common setup for secure session tests
void lt_test_setup_secure_session(lt_handle_t *h, const char *test_name);

// Helper to iterate all ECC slots
void lt_test_for_each_ecc_slot(lt_handle_t *h, void (*test_func)(lt_handle_t*, uint8_t));
```

2. **Refactor cleanup functions** to use helpers:
```c
static lt_ret_t lt_test_rev_ecc_key_store_cleanup(void)
{
    return lt_test_cleanup_ecc_slots(g_h);
}
```

3. **Create test macros** for common patterns:
```c
#define LT_TEST_ECC_SLOTS(h, body) \
    for (uint8_t i = TR01_ECC_SLOT_0; i <= TR01_ECC_SLOT_31; i++) { \
        LT_LOG_INFO("Testing slot #%" PRIu8, i); \
        body; \
    }
```

## References
- [DRY principle in testing](https://www.testingexcellence.com/dry-dont-repeat-yourself-principle-in-testing/)
- [Test helper functions best practices](https://martinfowler.com/articles/practical-test-pyramid.html#TestHelpers)
