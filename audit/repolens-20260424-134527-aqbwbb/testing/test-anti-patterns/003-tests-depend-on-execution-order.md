---
title: "[HIGH] Tests depend on execution order via `lt_test_cleanup_function` global"
severity: HIGH
domain: testing
lens: test-anti-patterns
labels:
  - "audit:testing/test-anti-patterns"
---

## Summary
The functional tests use a global function pointer `lt_test_cleanup_function` that is set at the start of each test and cleared at the end. This creates a dependency on execution order - if tests run in a different sequence or if a test fails to clear the pointer, subsequent tests may execute the wrong cleanup function.

**Location:**
- Definition: `third_party/libtropic/tests/functional/lt_test_common.c:19`
- Usage: 12+ test files assign this pointer at the start of each test

## Impact
1. **Wrong cleanup function executes**: If test A sets the pointer, fails, and test B runs, test B's cleanup may use test A's cleanup function.
2. **Stale pointer on test failure**: If a test fails before clearing `lt_test_cleanup_function`, the next test inherits the previous cleanup.
3. **Non-deterministic behavior**: Tests may pass or fail depending on the order they are run.
4. **Hard to run tests in parallel**: The shared global makes parallel execution impossible without race conditions.

## Evidence
**Definition in `lt_test_common.c:19`:**
```c
lt_ret_t (*lt_test_cleanup_function)(void) = NULL;
```

**Assignment pattern in `lt_test_rev_ecc_key_store.c:108`:**
```c
void lt_test_rev_ecc_key_store(lt_handle_t *h)
{
    // Making the handle accessible to the cleanup function.
    g_h = h;

    // ... setup code ...

    lt_test_cleanup_function = &lt_test_rev_ecc_key_store_cleanup;  // Set global

    // ... test logic ...

    // Cleanup not needed anymore, all slots were erased
    lt_test_cleanup_function = NULL;  // Clear global

    // ... more code ...
}
```

**Usage in `lt_assert_fail_handler()` (lt_test_common.c:21-37):**
```c
void lt_assert_fail_handler(void)
{
    if (NULL != lt_test_cleanup_function) {
        LT_LOG_INFO("Post-assert cleanup started.");
        lt_ret_t ret = lt_test_cleanup_function();  // Calls whatever was last set
        // ...
    }
    // ...
    LT_FINISH_TEST();  // Test ends, but cleanup function may still be set
}
```

**Problem scenario:**
1. Test A starts, sets `lt_test_cleanup_function = &test_a_cleanup`
2. Test A fails at line 50, calls `lt_assert_fail_handler()`
3. `lt_assert_fail_handler()` calls `test_a_cleanup()` - good so far
4. But `lt_assert_fail_handler()` calls `LT_FINISH_TEST()` which doesn't clear the pointer
5. Test B starts, sets `lt_test_cleanup_function = &test_b_cleanup`
6. If Test B's cleanup function needs to access `g_h`, it may get Test A's handle if timing is off

## Recommended Fix
1. **Pass cleanup function as parameter** instead of using global:
```c
// Change test functions to accept cleanup function
void lt_test_rev_ecc_key_store(lt_handle_t *h, lt_ret_t (*cleanup)(lt_handle_t*));

// Or use a struct to bundle test state
typedef struct {
    lt_handle_t *h;
    lt_ret_t (*cleanup)(lt_handle_t*);
} test_context_t;
```

2. **Always clear the pointer on failure** in `lt_assert_fail_handler()`:
```c
void lt_assert_fail_handler(void)
{
    lt_ret_t (*current_cleanup)(void) = lt_test_cleanup_function;  // Save local
    lt_test_cleanup_function = NULL;  // Clear immediately

    if (NULL != current_cleanup) {
        current_cleanup();
    }
    LT_FINISH_TEST();
}
```

3. **Use a stack-based approach** for nested tests:
```c
typedef struct test_frame {
    lt_handle_t *h;
    lt_ret_t (*cleanup)(lt_handle_t*);
    struct test_frame *next;
} test_frame_t;

static test_frame_t *test_stack = NULL;

void lt_test_push(lt_handle_t *h, lt_ret_t (*cleanup)(lt_handle_t*)) {
    // Push to stack
}

void lt_test_pop(void) {
    // Pop and call cleanup
}
```

## References
- [Test order dependency anti-pattern](https://stackoverflow.com/questions/10396409/how-to-handle-tests-that-depend-on-execution-order)
- [Global state in tests](https://martinfowler.com/articles/practical-test-pyramid.html#KeepTestDataLocal)
