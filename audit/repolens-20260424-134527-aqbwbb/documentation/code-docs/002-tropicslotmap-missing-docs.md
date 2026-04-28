---
title: "[MEDIUM] TropicSlotMap class missing documentation"
severity: MEDIUM
domain: documentation/code-docs
lens: code-docs
labels:
  - "audit:documentation/code-docs"
---

## Summary

The `TropicSlotMap` class in `components/cdc_core/include/cdc_core/TropicSlotMap.h` lacks documentation. This class manages the compile-time slot allocation map for TROPIC01 ECC and R-Memory slots, which is critical for understanding module memory allocation.

**File:** `components/cdc_core/include/cdc_core/TropicSlotMap.h:7-46`

## Impact

- Developers cannot understand slot allocation without reading implementation
- Makes debugging slot conflicts harder
- New contributors may misunderstand the memory map structure

## Evidence

```cpp
// Line 7-46: Complete lack of documentation
class TropicSlotMap {
public:
    enum class SlotType : uint8_t {  ///< No docs
        ECC,
        RMEM
    };

    struct SlotRange {         ///< No docs
        bool valid = false;
        SlotType type = SlotType::ECC;
        const char* moduleName = nullptr;
        uint8_t moduleId = 0;
        uint16_t start = 0;
        uint16_t end = 0;
    };

    static TropicSlotMap& instance();  ///< No docs

    bool isValid() const { ... }       ///< No docs
    const char* errorMessage() const { ... }  ///< No docs

    bool getRangeByName(const char* moduleName, SlotType type, SlotRange* out) const;
    bool getRangeByModuleId(uint8_t moduleId, SlotType type, SlotRange* out) const;
    bool isRmemAllowedForModuleId(uint16_t slot, uint8_t moduleId) const;

    uint16_t rmemMax() const;
    uint32_t computeMapSignature() const;
```

## Recommended Fix

Add comprehensive documentation:

```cpp
/**
 * \brief TROPIC01 slot allocation map.
 *
 * Contains compile-time slot assignments for each module:
 * - ECC slots (0-31) for key storage
 * - R-Memory slots (0-511) for metadata
 *
 * Validated at boot; errors reported if slot map is inconsistent.
 * Signature computed from map to detect runtime changes.
 */
class TropicSlotMap {
public:
    /**
     * \brief Slot type enumeration.
     */
    enum class SlotType : uint8_t {
        ECC,    ///< ECC key slots (0-31)
        RMEM    ///< R-Memory slots (0-511)
    };

    /**
     * \brief Slot range descriptor.
     */
    struct SlotRange {
        bool valid;               ///< Range is valid
        SlotType type;            ///< ECC or RMEM
        const char* moduleName;   ///< Owning module
        uint8_t moduleId;         ///< Module ID
        uint16_t start;           ///< Start slot (inclusive)
        uint16_t end;             ///< End slot (inclusive)
    };

    /**
     * \brief Get singleton instance.
     */
    static TropicSlotMap& instance();

    /**
     * \brief Check if slot map is valid.
     * \return true if map passed validation.
     */
    bool isValid() const;

    /**
     * \brief Get error message if map is invalid.
     * \return Error string or nullptr.
     */
    const char* errorMessage() const;

    /**
     * \brief Get slot range by module name.
     * \param moduleName Module name.
     * \param type Slot type (ECC or RMEM).
     * \param out Output range structure.
     * \return true if found.
     */
    bool getRangeByName(const char* moduleName, SlotType type, SlotRange* out) const;

    /**
     * \brief Get slot range by module ID.
     * \param moduleId Module ID.
     * \param type Slot type.
     * \param out Output range structure.
     * \return true if found.
     */
    bool getRangeByModuleId(uint8_t moduleId, SlotType type, SlotRange* out) const;

    /**
     * \brief Check if R-Memory slot is allowed for module.
     * \param slot Slot number.
     * \param moduleId Module ID.
     * \return true if module owns this slot.
     */
    bool isRmemAllowedForModuleId(uint16_t slot, uint8_t moduleId) const;

    /**
     * \brief Get total R-Memory slot count.
     * \return Number of slots (typically 512).
     */
    uint16_t rmemMax() const;

    /**
     * \brief Compute map signature for validation.
     * \return Hash of slot map data.
     */
    uint32_t computeMapSignature() const;
```

## References

- Related: `TropicStorage.h` (uses slot map)
- See: `components/cdc_core/include/cdc_core/IModule.h` for SlotRequest/SlotRange usage
