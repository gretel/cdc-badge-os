---
title: "[MEDIUM] Missing high-level system architecture diagram"
severity: MEDIUM
domain: architecture
lens: architecture-docs
labels:
  - "audit:documentation/architecture-docs"
---

## Summary
The CDC Badge OS repository lacks a high-level system architecture diagram that visually shows the relationship between components, their boundaries, and how they communicate. While ASCII art diagrams exist in the README.md (lines 86-104), there is no comprehensive visual diagram showing:

- Component layers (HAL, Core, UI, Modules)
- Data flow between components
- Communication patterns (EventBus, ServiceRegistry, direct calls)
- External interfaces (USB, BLE, SPI, I2C)

**Evidence:**
- `README.md:86-104` - Contains only a basic ASCII box diagram
- `docs/README.md` - No diagrams beyond text
- `docs/UI_FLOWS.md` - Shows UI states but not system architecture
- No `.drawio`, `.svg`, or Mermaid diagram files in `/docs/` directory

## Impact
- New developers must reverse-engineer the architecture from code
- Hard to understand component interactions at a glance
- No visual reference for onboarding
- Makes it difficult to communicate design decisions to stakeholders
- The existing ASCII diagram is too simplified to show communication patterns

## Evidence
**Current state (from `README.md:86-104`):**
```
┌─────────────────────────────────────────────────────────────────┐
│                         Modules                                  │
│  mod_fido2 │ mod_totp │ mod_password │ mod_gpg │ mod_vcard │ ...│
├─────────────────────────────────────────────────────────────────┤
│                      OS UI (cdc_os_ui)                          │
│          Lock Screen │ Settings │ WiFi/BLE Menus                │
...
```

This shows hierarchy but NOT:
- How EventBus connects modules to core services
- How ServiceRegistry provides typed services (IKeyboardProvider, etc.)
- How TROPIC01 secure element is accessed through the HAL
- USB composite device (CDC/HID/CCID) architecture
- BLE protocol stack integration

## Recommended Fix
Create a system architecture diagram using one of these approaches:

**Option 1 (Recommended):** Add a Mermaid diagram to `docs/architecture.md`:
```mermaid
graph TB
    subgraph "Hardware Layer"
        ESP32[ESP32-S3 MCU]
        TROPIC[TROPIC01 Secure Element]
        EPD[E-Paper Display]
        KEYPAD[12-Button Keypad]
        USB[USB-C]
        BLE[BLE Radio]
    end

    subgraph "HAL Layer (cdc_hal)"
        IDisplay[IDisplay]
        IKeypad[IKeypad]
        ISecureElement[ISecureElement]
        IPower[IPowerManager]
        IUSB[IUSBController]
        IBLE[IBluetoothController]
    end

    subgraph "Core Services (cdc_core)"
        EventBus[EventBus]
        Registry[ServiceRegistry]
        ModuleReg[ModuleRegistry]
        PinMgr[PinManager]
    end

    subgraph "UI Framework"
        cdc_ui[cdc_ui]
        cdc_views[cdc_views]
        cdc_os_ui[cdc_os_ui]
    end

    subgraph "Modules"
        FIDO2[mod_fido2]
        TOTP[mod_totp]
        PASS[mod_password]
        GPG[mod_gpg]
        VCARD[mod_vcard]
    end

    ESP32 --> IDisplay
    ESP32 --> IKeypad
    TROPIC --> ISecureElement
    USB --> IUSB
    BLE --> IBLE

    IDisplay --> cdc_ui
    IKeypad --> EventBus
    ISecureElement --> Registry

    EventBus --> cdc_os_ui
    Registry --> FIDO2
    Registry --> TOTP
    Registry --> PASS
    Registry --> GPG
    Registry --> VCARD

    cdc_os_ui --> FIDO2
    cdc_os_ui --> TOTP
```

**Option 2:** Create a Draw.io file (`docs/architecture.drawio`) with layered C4-style container diagram.

**Option 3:** Generate from code using Doxygen with `\dotfile` directives.

The diagram should cover:
1. Component hierarchy (layers)
2. Key interfaces and their relationships
3. Data flow paths (events, services, direct calls)
4. Hardware interfaces (SPI, I2C, USB, BLE)

## References
- C4 Model: https://c4model.com/
- Mermaid.js: https://mermaid.js.org/
- Existing architecture described in `components/cdc_core/include/cdc_core/` headers
- Module interface: `components/cdc_core/include/cdc_core/IModule.h`
