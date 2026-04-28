---
title: "[HIGH] Missing Architecture Decision Records (ADRs)"
severity: HIGH
domain: architecture
lens: architecture-docs
labels:
  - "audit:documentation/architecture-docs"
---

## Summary
The repository has no Architecture Decision Records (ADR) directory or system for documenting significant technology choices. Key architectural decisions lack recorded rationale:

1. **ESP32-S3 selection** - Why ESP32-S3 over other MCUs (RP2040, nRF52, STM32)?
2. **TROPIC01 secure element** - Why TROPIC01 over ATECC608B, SE050, or other options?
3. **Modular plugin architecture** - Rationale for runtime module registration vs compile-time config
4. **EventBus vs direct coupling** - Why event-driven architecture?
5. **E-Paper display choice** - Why E-Paper over OLED/LCD?
6. **PlatformIO/ESP-IDF** - Why this build system?
7. **ServiceRegistry pattern** - Why typed service discovery?

**Evidence:**
- No `docs/adr/` or `architecture/decisions/` directory
- `README.md` and `docs/README.md` contain no decision rationale
- `docs/plans/` contains implementation plans but not decision records
- `plan.md` - Contains project plan but not architectural decisions
- No ADR files (typically named `0001-<topic>.md`)

## Impact
- **Tribal knowledge**: Decisions only exist in developer's head or PR comments
- **Hard to onboard**: New developers can't understand why choices were made
- **Repeated decisions**: Same decisions may be revisited unnecessarily
- **Difficult to evaluate alternatives**: No record of options considered
- **Technical debt**: Hard to know what decisions might need revisiting

## Evidence
**Key decisions without documentation:**

**1. MCU Selection**
- Current: ESP32-S3-WROOM-1 (16MB Flash, PSRAM)
- Alternatives considered: Unknown
- Rationale: Unknown (WiFi/BLE needed? PSRAM for large buffers?)

**2. Secure Element**
- Current: TROPIC01 (32 ECC slots, 512 R-Memory slots)
- Alternatives: ATECC608B, SE050, ATECC608A
- Rationale: Unknown (Cost? Capacity? Feature set?)

**3. Architecture Pattern**
- Current: Modular plugin system with runtime registration
- Alternatives: Static config, feature flags, separate binaries
- Rationale: Unknown (Flexibility? Testing? User customization?)

**4. Event-Driven Communication**
- Current: EventBus for system-wide events
- Alternatives: Direct function calls, callbacks, message queues
- Rationale: Unknown (Decoupling? Testability?)

**5. Display Technology**
- Current: GDEY029T94 E-Paper (296x128)
- Alternatives: OLED, LCD, e-Ink
- Rationale: Unknown (Low power? Visibility? Cost?)

**6. Build System**
- Current: PlatformIO with ESP-IDF
- Alternatives: Pure ESP-IDF, Arduino framework
- Rationale: Unknown (Ecosystem? Tooling?)

## Recommended Fix
Create `docs/adr/` directory with initial ADRs:

**Directory structure:**
```
docs/adr/
├── 0001-mcu-selection.md
├── 0002-secure-element.md
├── 0003-modular-architecture.md
├── 0004-event-driven-communication.md
├── 0005-display-technology.md
└── 0006-build-system.md
```

**Template for each ADR:**
```markdown
# ADR 0001: MCU Selection

## Status
Proposed | Accepted | Deprecated | Superseded

## Context
What is the problem? What are the constraints?
- Need WiFi and BLE
- Need PSRAM for large buffers
- Cost constraints
- Pin count requirements

## Decision
ESP32-S3-WROOM-1 (16MB Flash, PSRAM)

## Rationale
- ESP32-S3 has native WiFi/BLE
- PSRAM available for large buffers (display, vCard cache)
- Good ESP-IDF support
- Reasonable cost

## Consequences
**Positive:**
- Single chip for all connectivity
- Large flash for multiple modules
- PSRAM for graphics buffers

**Negative:**
- Higher power than nRF52
- Larger footprint than RP2040

## Alternatives Considered
- **nRF52840**: Better BLE, no WiFi
- **RP2040**: Cheaper, no WiFi/BLE
- **ESP32-C3**: Cheaper, fewer pins

## References
- ESP32-S3 datasheet
- Comparison benchmarks
```

**Specific ADRs to create:**
1. `0001-mcu-selection.md` - ESP32-S3 choice
2. `0002-secure-element.md` - TROPIC01 choice
3. `0003-modular-architecture.md` - Plugin system design
4. `0004-event-driven-communication.md` - EventBus pattern
5. `0005-display-technology.md` - E-Paper choice
6. `0006-build-system.md` - PlatformIO/ESP-IDF
7. `0007-storage-allocation.md` - TROPIC01 slot allocation strategy

## References
- ADR format: https://adr.github.io/
- Example ADRs: https://github.com/joelparkerhenderson/architecture-decision-record
- C4 model for architecture docs: https://c4model.com/
