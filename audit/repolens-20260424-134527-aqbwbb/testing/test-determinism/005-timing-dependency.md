---
title: "[LOW] Test timing depends on hardware response time variability"
severity: LOW
domain: testing
lens: test-determinism
labels:
  - "test-determinism"
  - "timing-dependency"
  - "hardware-dependency"
---

## Summary
The test `lt_test_rev_startup_req.c` uses fixed timing assumptions when waiting for chip state changes after reboot. It polls with a fixed delay (`LT_TR01_REBOOT_DELAY_MS = 250ms`) and a maximum of 10 attempts. This can cause tests to fail on slower hardware or when the chip takes longer than expected to respond.

## Impact
- **Flaky tests**: Tests may pass on fast hardware but fail on slower hardware or when the chip is under load
- **Environment-dependent**: Different development boards or chip revisions may have different response times
- **Hard to debug**: Timing-related failures may appear intermittent

## Evidence

File: `third_party/libtropic/tests/functional/lt_test_rev_startup_req.c:18-39`

```c
#define REBOOT_WAIT_ATTEMPTS 10

// ...

static enum lt_current_chip_state check_current_state(void)
{
    uint8_t spect_ver[TR01_L2_GET_INFO_SPECT_FW_SIZE];
    lt_ret_t ret;

    LT_LOG_INFO("Retrieving SPECT FW version...");
    for (int i = 0; i < REBOOT_WAIT_ATtempts; i++) {
        ret = lt_get_info_spect_fw_ver(g_h, spect_ver);
        if (LT_OK == ret) {
            break;
        }
        else if (LT_L1_CHIP_BUSY == ret) {
            LT_LOG_INFO("Chip busy, waiting and trying again...");
            LT_TEST_ASSERT(LT_OK, lt_l1_delay(&g_h->l2, LT_TR01_REBOOT_DELAY_MS));
        }
    }
    // ...
}
```

File: `third_party/libtropic/include/libtropic_common.h`
```c
#define LT_TR01_REBOOT_DELAY_MS 250
```

The test waits up to 2500ms (10 attempts × 250ms) for the chip to be ready after reboot. If the chip takes longer, the test fails.

## Recommended Fix

### 1. Increase timeout with exponential backoff
Use exponential backoff instead of fixed delays:

```c
static enum lt_current_chip_state check_current_state(void)
{
    uint8_t spect_ver[TR01_L2_GET_INFO_SPECT_FW_SIZE];
    lt_ret_t ret;
    uint32_t delay_ms = 100;  // Start with shorter delay
    uint32_t total_delay = 0;
    const uint32_t max_delay = 5000;  // 5 second total timeout

    LT_LOG_INFO("Retrieving SPECT FW version...");
    for (int i = 0; total_delay < max_delay; i++) {
        ret = lt_get_info_spect_fw_ver(g_h, spect_ver);
        if (LT_OK == ret) {
            break;
        }
        else if (LT_L1_CHIP_BUSY == ret) {
            LT_LOG_INFO("Chip busy, waiting %lu ms and trying again...", delay_ms);
            LT_TEST_ASSERT(LT_OK, lt_l1_delay(&g_h->l2, delay_ms));
            total_delay += delay_ms;
            delay_ms = delay_ms * 2;  // Exponential backoff
            if (delay_ms > 500) delay_ms = 500;  // Cap at 500ms
        }
    }
    // ...
}
```

### 2. Add debug logging for timing
Log the actual time taken for chip operations:

```c
static enum lt_current_chip_state check_current_state(void)
{
    uint32_t start_time = get_current_time_ms();
    // ... polling logic ...
    uint32_t elapsed = get_current_time_ms() - start_time;
    LT_LOG_INFO("Chip ready after %lu ms", elapsed);
}
```

### 3. Make timeout configurable
Allow timeout to be configured via build flags for different hardware:

```c
#ifndef LT_TEST_REBOOT_TIMEOUT_MS
#define LT_TEST_REBOOT_TIMEOUT_MS 5000
#endif
```

## References
- [Exponential backoff pattern](https://www.awsarchitectureblog.com/2015/03/backoff.html)
- [Timing-dependent tests](https://stackoverflow.com/questions/1915295/unit-testing-time-dependent-code)
