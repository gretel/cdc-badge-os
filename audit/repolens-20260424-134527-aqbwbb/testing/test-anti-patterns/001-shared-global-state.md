---
title: "[HIGH] Shared mutable global state (`g_h`) in functional tests"
severity: HIGH
domain: testing
lens: test-anti-patterns
labels:
  - "audit:testing/test-anti-patterns"
---

## Summary
Multiple functional test files in `third_party/libtropic/tests/functional/` use a module-level global variable `g_h` (type `lt_handle_t*`) to share the test handle between the main test function and cleanup functions. This creates shared mutable state between tests.

**Affected files:**
- `third_party/libtropic/tests/functional/lt_test_rev_ecc_key_generate.c:18`
- `third_party/libtropic/tests/functional/lt_test_rev_ecc_key_store.c:38`
- `third_party/libtropic/tests/functional/lt_test_rev_ecdsa_sign.c:28`
- `third_party/libtropic/tests/functional/lt_test_rev_eddsa_sign.c:26`
- `third_party/libtropic/tests/functional/lt_test_rev_erase_r_config.c`
- `third_party/libtropic/tests/functional/lt_test_rev_get_info_req_bootloader.c`
- `third_party/libtropic/tests/functional/lt_test_rev_get_log_req.c`
- `third_party/libtropic/tests/functional/lt_test_rev_mcounter.c`
- `third_party/libtropic/tests/functional/lt_test_rev_r_mem.c`
- `third_party/libtropic/tests/functional/lt_test_rev_resend_req.c`
- `third_party/libtropic/tests/functional/lt_test_rev_startup_req.c:21`
- `third_party/libtropic/tests/functional/lt_test_rev_write_r_config.c`

Example from `lt_test_rev_ecc_key_generate.c:18`:
```c
// Shared with cleanup function
lt_handle_t *g_h;

static lt_ret_t lt_test_rev_ecc_key_generate_cleanup(void)
{
    // Uses g_h without it being passed as parameter
    ret = lt_verify_chip_and_start_secure_session(g_h, ...);
    ...
}

void lt_test_rev_ecc_key_generate(lt_handle_t *h)
{
    // Making the handle accessible to the cleanup function.
    g_h = h;  // Global assignment
    ...
}
```

## Impact
1. **Tests may fail when run in different order**: If tests run in a shuffled order, the global `g_h` may still contain a stale pointer from a previous test.
2. **Tests fail in isolation**: Running a single test file may work, but running multiple tests together can cause collisions when `g_h` is overwritten.
3. **Hard-to-debug race conditions**: On embedded systems or parallel test runners, concurrent access to `g_h` could cause crashes.
4. **Cleanup may operate on wrong handle**: If a test fails and cleanup runs, it may use a handle from a different test if timing is off.

## Evidence
Pattern found in 12+ test files:
```c
// Line 18 in lt_test_rev_ecc_key_generate.c
lt_handle_t *g_h;  // Module-level global

// Line 78-79 in lt_test_rev_ecc_key_generate.c
// Making the handle accessible to the cleanup function.
g_h = h;  // Assignment to global

// Line 30 in cleanup function
ret = lt_verify_chip_and_start_secure_session(g_h, LT_TEST_SH0_PRIV, ...);
```

The `lt_test_cleanup_function` pointer (defined in `lt_test_common.c:19`) is also a shared global that compounds this issue.

## Recommended Fix
Pass the handle as a parameter to cleanup functions instead of using a global:

1. **Change cleanup function signature** to accept `lt_handle_t *h`:
```c
static lt_ret_t lt_test_rev_ecc_key_generate_cleanup(lt_handle_t *h)
{
    lt_ret_t ret;
    // Use h directly instead of g_h
    ret = lt_verify_chip_and_start_secure_session(h, ...);
    ...
}
```

2. **Update `lt_test_cleanup_function` type** to include handle parameter:
```c
// In libtropic_functional_tests.h
typedef lt_ret_t (*lt_test_cleanup_function_t)(lt_handle_t *h);
extern lt_test_cleanup_function_t lt_test_cleanup_function;
```

3. **Call cleanup with handle**:
```c
// In main test function
if (lt_test_cleanup_function) {
    lt_test_cleanup_function(h);  // Pass handle
}
```

This ensures each test's cleanup operates on its own handle without global state.

## References
- [Testing anti-patterns: Shared state between tests](https://martinfowler.com/bliki/TestSmell.html)
- [Test isolation best practices](https://www.agilealliance.org/glossary/test-isolation/)
