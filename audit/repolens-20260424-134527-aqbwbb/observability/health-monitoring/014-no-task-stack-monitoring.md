---
title: "[LOW] No task stack and CPU utilization monitoring"
severity: LOW
domain: observability
lens: health-monitoring
labels:
  - "audit:observability/health-monitoring"
---

## Summary

The system lacks comprehensive task monitoring capabilities. While ESP32-S3 provides FreeRTOS APIs for stack high-water marks and CPU utilization, these are not exposed via serial commands for health monitoring.

**What exists:**
- One-off stack check in `WifiController.cpp` using `uxTaskGetStackHighWaterMark(nullptr)`
- `STATUS` command shows heap memory but no task stats

**What's missing:**
- No command to list all tasks with their stack usage
- No CPU utilization tracking per-task or total
- No stack overflow detection/alerting
- No task state monitoring (Running, Ready, Blocked, Suspended)

## Impact

1. **Stack overflow risk**: Cannot proactively identify tasks running low on stack
2. **Debugging difficulty**: Hard to diagnose task-related issues (blocking, priority inversion)
3. **No performance baseline**: Cannot track CPU utilization trends over time
4. **Memory optimization**: Cannot identify tasks with excessive stack allocation

## Evidence

**Stack tracking exists but unused** (`components/cdc_hal/src/WifiController.cpp:150`):
```cpp
LOG_I(TAG, "Task: %s, stack free: %lu", pcTaskGetName(nullptr),
      (unsigned long)uxTaskGetStackHighWaterMark(nullptr));
```
Only used once, not exposed as a command.

**STATUS command** (`components/serial_cmd/src/SerialCmd.cpp:427-434`):
```cpp
static void cmdStatus(const char* args) {
    (void)args;
    Console::printf("=== System Status ===\r\n");
    Console::printf("Free heap: %lu bytes\r\n", (unsigned long)esp_get_free_heap_size());
    Console::printf("Min free heap: %lu bytes\r\n", (unsigned long)esp_get_minimum_free_heap_size());
    Console::printf("Uptime: %llu ms\r\n", esp_timer_get_time() / 1000ULL);
    Console::flush();
}
```
No task information included.

**Available ESP-IDF APIs** (not used):
- `uxTaskGetSystemState()` - Get all task states and stack high-water marks
- `vTaskGetRunTimeStats()` - Get CPU utilization per task
- `pcTaskGetTaskName()` - Get task names

## Recommended Fix

Add task monitoring command:

**Step 1: Add TASKS command:**
```cpp
static void cmdTasks(const char* args) {
    TaskStatus_t *start_array = NULL, *end_array = NULL;
    UBaseType_t tasks;
    configSTACK_DEPTH_TYPE stack_min, stack_max;
    char name[16];
    
    // Allocate array for task status
    tasks = uxTaskGetNumberOfTasks();
    start_array = (TaskStatus_t*)malloc(sizeof(TaskStatus_t) * tasks);
    if (start_array == NULL) {
        Console::printf("Failed to allocate task array\r\n");
        return;
    }
    
    // Get task states
    tasks = uxTaskGetSystemState(start_array, tasks, NULL);
    
    Console::printf("=== Task Status ===\r\n");
    Console::printf("%-16s %-10s %-10s %-10s\r\n", "Task", "State", "Stack Min", "Stack Max");
    
    for (uint32_t i = 0; i < tasks; i++) {
        pcTaskGetTaskName(start_array[i].xHandle, name, sizeof(name));
        Console::printf("%-16s %-10c %-10lu %-10lu\r\n",
            name,
            start_array[i].eCurrentState,
            (unsigned long)start_array[i].usStackHighWaterMark);
    }
    
    free(start_array);
}
```

**Step 2: Add RUNTIME command for CPU utilization:**
```cpp
static void cmdRuntime(const char* args) {
    char *buffer = (char*)malloc(512);
    if (buffer == NULL) return;
    
    vTaskGetRunTimeStats(buffer);
    Console::printf("=== CPU Utilization ===\r\n");
    Console::printf("%s\r\n", buffer);
    
    free(buffer);
}
```

**Step 3: Register commands:**
```cpp
reg.registerCommand({"TASKS", "Show task stack usage", cmdTasks, "system", false});
reg.registerCommand({"RUNTIME", "Show CPU utilization", cmdRuntime, "system", false});
```

**Expected output:**
```
TASKS
=== Task Status ===
Task             State      Stack Min  Stack Max 
main             R          42         120      
idle_0         S          85         150      
wifi             B          30         100      

RUNTIME
=== CPU Utilization ===
Task               Time (us)    % CPU    
main               123456       45       
idle_0           98765        35       
wifi             54321        20       
```

**Step 4: Add to STATUS command (optional):**
```cpp
Console::printf("Tasks: %u\r\n", uxTaskGetNumberOfTasks());
Console::printf("CPU util: %lu%%\r\n", (unsigned long)getAverageCpuUtil());
```

## References

- FreeRTOS task stats: https://www.freertos.org/uxTaskGetSystemState.html
- ESP-IDF task monitoring: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html#task
- CPU utilization: https://www.freertos.org/vTaskGetRunTimeStats.html

</content>