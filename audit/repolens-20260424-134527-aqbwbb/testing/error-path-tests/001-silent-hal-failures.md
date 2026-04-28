---
title: "[HIGH] System continues booting with critical HAL failures (I2C, Secure Element, Display)"
severity: HIGH
domain: error-path-tests
lens: hardware-initialization
labels:
  - "critical-hardware"
  - "boot-sequencing"
  - "error-handling"
---

## Summary
In `main/main.cpp` (lines 108-204), the system initialization continues even when critical hardware components fail to initialize. This includes I2C bus, Power Manager, Secure Element (TROPIC01), Display, and Keypad. Each initialization error is logged but doesn't stop the boot sequence, potentially leading to undefined behavior when modules try to use failed hardware.

**Files:**
- `main/main.cpp:108-204`

## Impact
1. **Security Risk**: TROPIC01 Secure Element failure means FIDO2, GPG, and SSH keys may not work, but system appears operational
2. **Data Loss**: NVS storage may be corrupted if I2C bus fails but system continues
3. **User Confusion**: System shows as "working" but key features are silently broken
4. **Debug Difficulty**: Hard to diagnose which hardware failed without checking serial logs
5. **State Corruption**: Modules may write to non-existent hardware causing undefined behavior

## Evidence
From `main/main.cpp`:

```cpp
// Lines 108-113: I2C bus init - continues on failure
I2cManager* i2cManager = new I2cManager();
if (!i2cManager->initialize()) {
    LOG_E("Main", "I2C bus initialization failed");
    // System continues booting!
}

// Lines 118-124: Power manager init - continues on failure
powerManager = new BQ25895Power();
if (!powerManager->initialize()) {
    LOG_E("Main", "Power manager initialization failed");
    // System continues booting!
}

// Lines 163-172: Secure element init - continues with session warning
tropicElement = new Tropic01Element();
if (!tropicElement->initialize()) {
    LOG_E("Main", "TROPIC01 initialization failed");
    // Or just warns about session:
    // LOG_W("Main", "TROPIC01 session error, will retry");
    // System continues booting!
}

// Lines 195-204: Display init - continues on failure
epd = new EpaperDisplay();
if (!epd->initialize()) {
    LOG_E("Main", "E-Paper display initialization failed");
    // System continues booting!
}
```

## Recommended Fix
Implement a hardware initialization health check that stops booting if critical components fail:

1. **Define critical vs. optional hardware** in `feature_flags.h` or a new config:
   ```cpp
   enum class HardwareCriticality {
       CRITICAL,    // Boot must stop if fails (I2C, Secure Element, NVS)
       IMPORTANT,   // Boot continues but features disabled (Display, Keypad)
       OPTIONAL     // Boot continues normally (WiFi, Bluetooth)
   };
   ```

2. **Create a hardware health check function**:
   ```cpp
   bool checkHardwareHealth() {
       bool criticalFailed = false;
       
       // Check I2C (critical for TROPIC01, Power, Keypad)
       if (!i2cManager->isInitialized()) {
           LOG_E("Main", "I2C bus not initialized - critical");
           criticalFailed = true;
       }
       
       // Check TROPIC01 (critical for security)
       if (!tropicElement->isInitialized()) {
           LOG_E("Main", "TROPIC01 not initialized - critical");
           criticalFailed = true;
       }
       
       // Check Power (important for battery management)
       if (!powerManager->isInitialized()) {
           LOG_W("Main", "Power manager not initialized - features limited");
       }
       
       return !criticalFailed;
   }
   ```

3. **Add check before module initialization**:
   ```cpp
   // After HAL initialization, before modules
   if (!checkHardwareHealth()) {
       LOG_E("Main", "Critical hardware failure - entering safe mode");
       // Show error on display if available
       // Wait for reset button
       while (true) {
           vTaskDelay(pdMS_TO_TICKS(1000));
       }
   }
   
   // Then proceed to module initialization
   ModuleRegistry::getInstance().initAll();
   ```

4. **Add health check methods** to all HAL classes:
   ```cpp
   // In IPowerManager, ISecureElement, IDisplay, etc.
   virtual bool isInitialized() const = 0;
   ```

## References
- ESP-IDF Hardware Initialization Best Practices: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html
- Embedded Systems Boot Sequencing: https://www.embedded.com/electronics-blogs/4001954/Embedded-boot-sequence-best-practices
- Fail-Fast Pattern: https://en.wikipedia.org/wiki/Fail-fast
- Hardware Abstraction Layer design: https://www.embedded.com/design/programming-and-development/4026249/HAL-design-patterns-for-embedded-systems

</content>