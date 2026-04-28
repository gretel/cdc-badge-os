---
title: "[MEDIUM] Missing Plugin Architecture for Data Export Formats"
severity: MEDIUM
domain: architecture/extensibility
lens: export-formats
labels:
  - "audit:architecture/extensibility"
---

## Summary
Data export functionality (GPG public keys, TOTP secrets, password vault) is hardcoded to specific formats (PEM, Base32, etc.). Adding new export formats (OpenPGP ASCII armor, QR codes, encrypted bundles) requires modifying existing module code.

**Files affected:**
- `components/mod_gpg/src/GpgModule.cpp` - hardcoded PEM export
- `components/mod_totp/src/TotpModule.cpp` - hardcoded Base32 encoding
- `components/mod_password/src/PasswordModule.cpp` - potential CSV export

## Impact
- **Format extensibility**: Adding support for new formats (e.g., OpenPGP ASCII armor, JSON export) requires modifying module code
- **Code duplication**: Each module implements its own export logic
- **Testing burden**: Each new format requires changes to multiple modules

## Evidence

### GPG Module - Hardcoded PEM Export
`components/mod_gpg/src/GpgModule.cpp:180-190`:
```cpp
static void cmd_gpg_export(const char* args) {
    (void)args;
    char pem_buf[2048];
    size_t out_len = 0;
    if (!gpg_export_pubkey_pem(pem_buf, sizeof(pem_buf), &out_len)) {
        cdc::serial::Console::printf("ERROR\r\n");
        return;
    }
    cdc::serial::Console::printf("%s\r\n", pem_buf);
}
```

### TOTP Module - Hardcoded Base32 Encoding
`components/mod_totp/src/TotpModule.cpp:620-650`:
```cpp
static void base32Encode(const uint8_t* data, size_t dataLen, char* out, size_t outMax) {
    static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    // ... hardcoded Base32 encoding logic
}
```

### No Strategy Pattern for Export
There is no interface like:
```cpp
// Missing - should exist:
class IExportFormat {
public:
    virtual const char* getExtension() const = 0;
    virtual const char* getMimeType() const = 0;
    virtual bool exportData(const void* data, size_t len, char* output, size_t outMax) const = 0;
};
```

## Recommended Fix

### Create Export Format Interface
Define a common export format interface:

```cpp
// components/cdc_core/include/cdc_core/IExportFormat.h
#pragma once
#include <cstddef>

namespace cdc::core {

class IExportFormat {
public:
    virtual ~IExportFormat() = default;
    virtual const char* getName() const = 0;
    virtual const char* getExtension() const = 0;
    virtual const char* getMimeType() const = 0;
    virtual bool encode(const void* data, size_t len, char* output, size_t outMax) const = 0;
};

class ExportRegistry {
public:
    static ExportRegistry& instance();
    void registerFormat(IExportFormat* format);
    IExportFormat* getFormatByName(const char* name);
    IExportFormat* getFormatByExtension(const char* ext);
};

// Built-in implementations
class PemFormat : public IExportFormat {
    const char* getName() const override { return "PEM"; }
    const char* getExtension() const override { return ".pem"; }
    const char* getMimeType() const override { return "application/x-pem-file"; }
    bool encode(const void* data, size_t len, char* output, size_t outMax) const override;
};

class Base32Format : public IExportFormat {
    const char* getName() const override { return "Base32"; }
    const char* getExtension() const override { return ".txt"; }
    const char* getMimeType() const override { return "text/plain"; }
    bool encode(const void* data, size_t len, char* output, size_t outMax) const override;
};

class Base64Format : public IExportFormat {
    const char* getName() const override { return "Base64"; }
    const char* getExtension() const override { return ".b64"; }
    const char* getMimeType() const override { return "application/base64"; }
    bool encode(const void* data, size_t len, char* output, size_t outMax) const override;
};

} // namespace cdc::core
```

### Refactor Modules to Use Registry
Update GPG module to use the registry:
```cpp
// components/mod_gpg/src/GpgModule.cpp
static void cmd_gpg_export(const char* args) {
    // Parse format argument (default: PEM)
    char formatBuf[16];
    const char* fmtName = "PEM";
    const char* p = args;
    while (p && *p && std::isspace(*p)) p++;
    if (p && *p) {
        size_t i = 0;
        while (p[i] && !isspace(p[i]) && i < 15) formatBuf[i++] = p[i];
        formatBuf[i] = '\0';
        fmtName = formatBuf;
    }
    
    auto& registry = core::ExportRegistry::instance();
    auto* format = registry.getFormatByName(fmtName);
    if (!format) {
        cdc::serial::Console::printf("ERROR: Unknown format '%s'\r\n", fmtName);
        return;
    }
    
    char encoded[2048];
    if (!format->encode(pubkey_data, pubkey_len, encoded, sizeof(encoded))) {
        cdc::serial::Console::printf("ERROR\r\n");
        return;
    }
    cdc::serial::Console::printf("%s\r\n", encoded);
}
```

### Add New Formats Without Modifying Modules
New formats can be added as separate components:
```cpp
// components/mod_export_json/src/JsonFormat.cpp
class JsonFormat : public core::IExportFormat {
    const char* getName() const override { return "JSON"; }
    const char* getExtension() const override { return ".json"; }
    const char* getMimeType() const override { return "application/json"; }
    bool encode(const void* data, size_t len, char* output, size_t outMax) const override {
        // JSON encoding logic
    }
};

// Register at startup
void mod_export_json_register() {
    core::ExportRegistry::instance().registerFormat(new JsonFormat());
}
```

## References
- Strategy Pattern: https://refactoring.guru/design-patterns/strategy
- Factory Pattern for format selection: https://refactoring.guru/design-patterns/factory-method
- Open/Closed Principle: https://en.wikipedia.org/wiki/Open%E2%80%93closed_principle

</content>