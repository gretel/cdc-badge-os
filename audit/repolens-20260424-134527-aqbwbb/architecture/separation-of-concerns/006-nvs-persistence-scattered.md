---
title: "[MEDIUM] Cross-cutting concern (NVS persistence) scattered across modules"
severity: MEDIUM
domain: architecture
lens: separation-of-concerns
labels:
  - "audit:architecture/separation-of-concerns"
---

## Summary
NVS (non-volatile storage) persistence logic is duplicated across multiple modules with no centralized abstraction. Each module implements its own NVS read/write patterns, mixing data persistence concerns with business logic.

**Location**: 
- `components/grove_led/src/GroveLedModule.cpp:558-628` (loadSettings/saveSettings)
- `components/mod_totp/src/TotpStore.cpp` (NVStorage via TropicStorage)
- `components/mod_password/src/PasswordStore.cpp` (NVStorage via TropicStorage)

## Impact
- **Code duplication**: Similar NVS patterns repeated in each module
- **Inconsistent handling**: Different modules may handle errors differently
- **Hard to change**: Storage backend changes require modifying all modules
- **No transaction support**: Each module manages its own storage independently

## Evidence
```cpp
// GroveLedModule.cpp:558-628 - NVS persistence mixed with business logic
void GroveLedModule::loadSettings() {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        uint8_t val = 0;
        if (nvs_get_u8(handle, NVS_KEY_ENABLED, &val) == ESP_OK) {
            enabled_ = (val != 0);
        }
        if (nvs_get_u8(handle, NVS_KEY_LED_COUNT, &val) == ESP_OK) {
            ledCount_ = (val >= 1 && val <= MAX_LEDS) ? val : DEFAULT_LED_COUNT;
        }
        // ... 10 more NVS reads ...
        nvs_close(handle);
    }
}

void GroveLedModule::saveSettings() {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, NVS_KEY_ENABLED, enabled_ ? 1 : 0);
        nvs_set_u8(handle, NVS_KEY_LED_COUNT, ledCount_);
        // ... 5 more NVS writes ...
        nvs_commit(handle);
        nvs_close(handle);
    }
}

// Called from business methods - persistence mixed with logic
void GroveLedModule::setBrightness(uint8_t brightness) {
    brightness_ = brightness;
    saveSettings();  // Persistence as side effect
}
```

## Recommended Fix
1. **Create a centralized configuration service**:
   ```cpp
   namespace cdc::core {
       class ConfigService {
       public:
           void loadModuleSettings(const char* module, void* target, size_t size);
           void saveModuleSettings(const char* module, const void* data, size_t size);
           
           template<typename T>
           T get(const char* module, const char* key, T default_value);
           void set(const char* module, const char* key, const char* value);
       };
   }
   ```

2. **Use JSON or binary serialization for complex structures**:
   ```cpp
   struct LedSettings {
       uint8_t enabled;
       uint8_t led_count;
       uint8_t brightness;
       uint8_t color_r, color_g, color_b;
       uint8_t effect;
   };
   
   void GroveLedModule::loadSettings() {
       LedSettings settings = config_.getStruct("grove_led", "settings", default_settings);
       enabled_ = settings.enabled;
       ledCount_ = settings.led_count;
       // ...
   }
   ```

3. **Add transaction support**:
   ```cpp
   void GroveLedModule::saveSettings() {
       config_.beginTransaction("grove_led");
       config_.set("enabled", enabled_);
       config_.set("led_count", ledCount_);
       // ...
       config_.commit();
   }
   ```

## References
- [Configuration management](https://en.wikipedia.org/wiki/Configuration_management)
- [Repository Pattern](https://en.wikipedia.org/wiki/Repository_pattern)
