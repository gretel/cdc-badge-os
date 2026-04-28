---
title: "[MEDIUM] Tests share global state through `lt_test_cleanup_function` and `g_h`"
severity: MEDIUM
domain: testing
lens: test-determinism
labels:
  - "test-determinism"
  - "global-state"
  - "test-isolation"
---

## Summary
Multiple test files in `third_party/libtropic/tests/functional/` share global state through:
1. `lt_test_cleanup_function` - a function pointer stored in `lt_test_common.c:17`
2. `g_h` - a global handle pointer used in cleanup functions

This creates implicit dependencies between tests and can cause non-deterministic behavior when tests run in different orders or when cleanup from one test affects another.

## Impact
- **Order-dependent tests**: Tests may pass or fail depending on the order they are run
- **State leakage**: Cleanup from one test can interfere with another test's setup
- **Hard to debug**: When a test fails, it's unclear if the failure is due to the test itself or residual state from a previous test
- **Parallel execution**: Tests cannot be run in parallel without interference

## Evidence

### Global function pointer
File: `third_party/libtropic/tests/functional/lt_test_common.c:17`
```c
lt_ret_t (*lt_test_cleanup_function)(void) = NULL;
```

### Tests setting the global cleanup function
Multiple test files set this global variable:

1. `lt_test_rev_mcounter.c:78`
```c
lt_test_cleanup_function = &lt_test_rev_mcounter_cleanup;
// ... test code ...
lt_test_cleanup_function = NULL;
```

2. `lt_test_rev_r_mem.c:143`
```c
lt_test_cleanup_function = &lt_test_rev_r_mem_cleanup;
// ... test code ...
lt_test_cleanup_function = NULL;
```

3. `lt_test_rev_ecc_key_generate.c:96`
```c
lt_test_cleanup_function = &lt_test_rev_ecc_key_generate_cleanup;
// ... test code ...
lt_test_cleanup_function = NULL;
```

4. `lt_test_rev_ecdsa_sign.c:108`
```c
lt_test_cleanup_function = &lt_test_rev_ecdsa_sign_cleanup;
// ... test code ...
lt_test_cleanup_function = NULL;
```

5. `lt_test_rev_eddsa_sign.c:108`
```c
lt_test_cleanup_function = &lt_test_rev_eddsa_sign_cleanup;
// ... test code ...
lt_test_cleanup_function = NULL;
```

### Global handle in cleanup functions
Multiple tests use `g_h` global variable:

File: `lt_test_rev_mcounter.c:18`
```c
// Shared with cleanup function.
lt_handle_t *g_h;
```

File: `lt_test_rev_r_mem.c:22`
```c
// Shared with cleanup function
lt_handle_t *g_h;
```

### Assertion failure handler uses global state
File: `lt_test_common.c:21-36`
```c
void lt_assert_fail_handler(void)
{
    if (NULL != lt_test_cleanup_function) {
        LT_LOG_INFO("Post-assert cleanup started.");
        lt_ret_t ret = lt_test_cleanup_function();
        if (LT_OK == ret) {
            LT_LOG_INFO("Post-assert cleanup successful!");
        }
        else {
            LT_LOG_ERROR("Post-assert cleanup failed, ret=%s.", lt_ret_verbose(ret));
        }
    }
    // ...
}
```

## Recommended Fix

### Option 1: Pass state explicitly
Modify the test framework to pass state explicitly instead of using globals:

```c
typedef struct {
    lt_handle_t *handle;
    lt_ret_t (*cleanup)(lt_handle_t *h);
} test_context_t;

void lt_assert_fail_handler(test_context_t *ctx);
```

### Option 2: Scope-based cleanup
Use a scope-based pattern where cleanup is tied to the test function:

```c
void lt_test_rev_mcounter(lt_handle_t *h)
{
    test_context_t ctx = {
        .handle = h,
        .cleanup = lt_test_rev_mcounter_cleanup
    };
    
    // Test code uses ctx instead of globals
    // Assertion handler receives ctx
}
```

### Option 3: Reset global state at start
At minimum, ensure global state is reset at the start of each test:

```c
void lt_test_rev_mcounter(lt_handle_t *h)
{
    // Reset globals at start
    lt_test_cleanup_function = NULL;
    g_h = h;
    
    // ... rest of test
}
```

## References
- [Test isolation best practices](https://www.testim.io/blog/test-isolation-a-quick-guide-with-examples/)
- [Global state in unit tests](https://stackoverflow.com/questions/7556589/global-variables-in-unit-tests)
