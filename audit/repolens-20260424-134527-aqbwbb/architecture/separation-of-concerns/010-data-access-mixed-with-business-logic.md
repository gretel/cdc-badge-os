---
title: "[MEDIUM] Data access (NVS) mixed with business logic in handlers"
severity: MEDIUM
domain: architecture
lens: separation-of-concerns
labels:
  - "audit:architecture/separation-of-concerns"
---

## Summary
The codebase mixes **data access logic** (NVS persistence) directly with **business logic** in handler classes. Instead of having a dedicated repository or data access layer, persistence code is scattered throughout business classes:

- `WifiHandlers` directly calls `nvs_open`, `nvs_get_str`, `nvs_set_str`, `nvs_commit`
- `SettingsHandlers` directly calls NVS functions
- `AttestationKeyService` directly calls NVS functions
- Each module manages its own NVS namespace and persistence

This violates separation of concerns because **data access should be abstracted** behind a repository pattern, not interleaved with domain logic.

**Location**: 
- `components/cdc_os_ui/src/WifiHandlers.cpp:68-120` - NVS mixed with WiFi configuration
- `components/cdc_os_ui/src/SettingsHandlers.cpp:275-280` - NVS mixed with badge text handling
- `components/cdc_core/src/AttestationKeyService.cpp:66-96` - NVS mixed with key management

## Impact
- **Maintainability**: Changing persistence mechanism (e.g., from NVS to SQLite) requires touching multiple business classes
- **Testability**: Business logic cannot be tested without NVS hardware or mocking NVS in each class
- **Consistency**: Each class implements its own NVS patterns (error handling, namespaces, etc.)
- **Reusability**: Data access logic cannot be reused or shared across modules
- **Transaction support**: No centralized way to batch updates or handle multi-step persistence

## Evidence

### Example 1: NVS mixed with WiFi business logic
```cpp
// components/cdc_os_ui/src/WifiHandlers.cpp:68-120
void WifiHandlers::loadConfig() {
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READONLY, &nvs) != ESP_OK) {  // Data access here
        config_.valid = false;
        return;
    }
    
    size_t len = sizeof(config_.ssid);
    if (nvs_get_str(nvs, "ssid", config_.ssid, &len) != ESP_OK || len <= 1) {  // Data access
        nvs_close(nvs);
        config_.valid = false;
        return;
    }
    
    // ... more NVS calls for password, security, etc.
    
    nvs_close(nvs);
    config_.valid = true;  // Business logic mixed with data access
}

void WifiHandlers::saveConfig() {
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READWRITE, &nvs) != ESP_OK) return;  // Data access
    
    nvs_set_str(nvs, "ssid", wizard_.ssid);  // Data access
    nvs_set_str(nvs, "pass", wizard_.password);  // Data access
    nvs_set_u8(nvs, "sec", static_cast<uint8_t>(wizard_.security));  // Data access
    
    nvs_commit(nvs);  // Data access
    nvs_close(nvs);
    
    loadConfig();  // Business logic calls data access
}
```

### Example 2: NVS mixed with settings business logic
```cpp
// components/cdc_os_ui/src/SettingsHandlers.cpp:275-280
void saveDisplayField(const char* key, const char* value) {
    if (!key) return;
    nvs_handle_t nvs;
    if (nvs_open("display", NVS_READWRITE, &nvs) != ESP_OK) return;  // Data access
    const char* safeValue = value ? value : "";
    nvs_set_str(nvs, key, safeValue);  // Data access
    nvs_commit(nvs);  // Data access
    nvs_close(nvs);
}
```

### Example 3: NVS mixed with secure element business logic
```cpp
// components/cdc_core/src/AttestationKeyService.cpp:66-96
bool AttestationKeyService::loadStoredHash(uint8_t* out, size_t outLen) {
    if (!out || outLen == 0) return false;
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {  // Data access
        return false;
    }
    size_t len = outLen;
    esp_err_t err = nvs_get_blob(nvs, NVS_KEY_PUBHASH, out, &len);  // Data access
    nvs_close(nvs);
    return err == ESP_OK && len == outLen;  // Business logic mixed with data access
}

bool AttestationKeyService::saveStoredHash(const uint8_t* data, size_t len) {
    if (!data || len == 0) return false;
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {  // Data access
        return false;
    }
    esp_err_t err = nvs_set_blob(nvs, NVS_KEY_PUBHASH, data, len);  // Data access
    if (err == ESP_OK) {
        err = nvs_commit(nvs);  // Data access
    }
    nvs_close(nvs);
    return err == ESP_OK;
}
```

### Example 4: Multiple modules with scattered NVS namespaces
```cpp
// Different modules use different NVS namespaces without centralization:
components/cdc_os_ui/src/WifiHandlers.cpp:    nvs_open("wifi", ...)
components/cdc_os_ui/src/SettingsHandlers.cpp:    nvs_open("display", ...)
components/cdc_core/src/AttestationKeyService.cpp:    nvs_open("attest", ...)
components/mod_gpg/src/gpg.cpp:    nvs_open("mod_gpg", ...)
components/mod_fido2/src/fido2_storage.cpp:    nvs_open("fido2", ...)
```

## Recommended Fix

### 1. Create a centralized NVS repository layer

```cpp
// components/cdc_core/include/cdc_core/NvsRepository.h
namespace cdc::core {

class NvsRepository {
public:
    static NvsRepository& instance();
    
    void init();
    
    // Generic blob operations
    bool getBlob(const char* ns, const char* key, void* out, size_t* len);
    bool setBlob(const char* ns, const char* key, const void* data, size_t len);
    
    // String operations
    bool getString(const char* ns, const char* key, char* out, size_t* len);
    bool setString(const char* ns, const char* key, const char* value);
    
    // Primitive operations
    bool getU8(const char* ns, const char* key, uint8_t* val);
    bool setU8(const char* ns, const char* key, uint8_t val);
    bool getU32(const char* ns, const char* key, uint32_t* val);
    bool setU32(const char* ns, const char* key, uint32_t val);
    
    // Transaction support
    bool beginTransaction(const char* ns);
    bool commitTransaction(const char* ns);
    bool rollbackTransaction(const char* ns);
};

// Convenience template for typed repositories
template<typename T>
class TypedRepository {
public:
    TypedRepository(const char* ns) : ns_(ns) {}
    
    bool load(T* data) {
        return NvsRepository::instance().getBlob(ns_, "data", data, 
                                                  reinterpret_cast<size_t*>(sizeof(T)));
    }
    
    bool save(const T& data) {
        return NvsRepository::instance().setBlob(ns_, "data", &data, sizeof(T));
    }
    
private:
    const char* ns_;
};

} // namespace cdc::core
```

### 2. Refactor WiFi handlers to use repository

```cpp
// Before:
void WifiHandlers::loadConfig() {
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READONLY, &nvs) != ESP_OK) { ... }
    // ... direct NVS calls
}

// After:
void WifiHandlers::loadConfig() {
    auto& nvs = core::NvsRepository::instance();
    
    nvs.getString("wifi", "ssid", config_.ssid, &len);
    nvs.getString("wifi", "pass", config_.password, &len);
    nvs.getU8("wifi", "sec", &config_.security);
    // ... use repository API
}
```

### 3. Create domain-specific repositories

```cpp
// components/cdc_core/include/cdc_core/WifiRepository.h
namespace cdc::core {

class WifiRepository {
public:
    static WifiRepository& instance();
    
    WifiConfig load();
    void save(const WifiConfig& config);
    void clear();
    
private:
    static constexpr const char* NS = "wifi";
};

} // namespace cdc::core
```

### 4. Benefits of separation
- **Single source of truth**: All NVS access goes through one layer
- **Easy to swap**: Change persistence backend without touching business logic
- **Transaction support**: Batch updates atomically
- **Centralized error handling**: Consistent error handling across all data access
- **Testing**: Mock repository for unit tests

## References
- [Repository pattern (Wikipedia)](https://en.wikipedia.org/wiki/Repository_pattern)
- [Data access object (Wikipedia)](https://en.wikipedia.org/wiki/Data_access_object)
- Related issue #006 covers NVS persistence scattering from a different angle
