---
title: "[MEDIUM] TropicStorage class missing Doxygen documentation"
severity: MEDIUM
domain: documentation/code-docs
lens: code-docs
labels:
  - "audit:documentation/code-docs"
---

## Summary

The `TropicStorage` class in `components/cdc_core/include/cdc_core/TropicStorage.h` lacks Doxygen-style documentation for its public methods, structs, and constants. While the class header has a basic description, individual members are undocumented.

**File:** `components/cdc_core/include/cdc_core/TropicStorage.h:9-78`

## Impact

- Developers using this class must read implementation details to understand usage
- Makes onboarding new developers harder
- Increases risk of incorrect usage (e.g., calling methods in wrong order)
- Reduces discoverability of API capabilities

## Evidence

The following members lack documentation:

```cpp
// Line 9-78: No documentation for class members
class TropicStorage : public IService {
public:
    struct CacheEntry {           // No docs
        uint8_t moduleId;
        uint8_t flags;
        char name[...];
    };

    static constexpr uint8_t FLAG_USED = 0x01;  // No docs
    static constexpr uint16_t CHUNK_SLOTS = 64; // No docs

    using SlotCallback = void(*)(uint16_t slot, const CacheEntry& entry, void* ctx);
    using RebuildLogFn = void(*)(uint16_t slot, const char* message, void* ctx);

    static TropicStorage& instance();  // No docs

    // IService (no docs on overrides)
    bool init() override;
    bool start() override;
    void stop() override;
    ServiceState getState() const override;
    const char* getName() const override;

    void setSecureElement(...) { ... }  // No docs

    bool isCacheValid() const { ... }   // No docs

    // Iteration helpers - no docs
    bool forEachSlot(uint8_t moduleId, SlotCallback cb, void* ctx);
    bool forEachSlot(uint8_t moduleId, uint16_t fromSlot, uint16_t toSlot,
                     SlotCallback cb, void* ctx);
    bool getSlot(uint8_t moduleId, uint16_t index, SlotCallback cb, void* ctx);

    // NVS cache updates - no docs
    bool writeSlot(uint8_t moduleId, uint16_t slot, const char* name, uint8_t flags);
    bool eraseSlot(uint8_t moduleId, uint16_t slot);

    // Maintenance - no docs
    bool rebuild();
    bool rebuildVerbose(RebuildLogFn logFn, void* ctx);
    bool cleanup();
```

## Recommended Fix

Add Doxygen-style documentation to all public members:

```cpp
/**
 * \brief In-memory cache for TROPIC01 R-Memory slot metadata.
 *
 * Maintains a fast lookup table of slot usage (module ID, name, flags)
 * stored in NVS. Rebuilds from TROPIC01 when cache signature mismatch.
 */
class TropicStorage : public IService {
public:
    /**
     * \brief Cache entry structure for one R-Memory slot.
     */
    struct CacheEntry {
        uint8_t moduleId;         ///< Owning module ID
        uint8_t flags;            ///< Slot flags (see FLAG_USED)
        char name[...];           ///< Slot name (null-terminated)
    };

    static constexpr uint8_t FLAG_USED = 0x01;  ///< Entry is in use
    static constexpr uint16_t CHUNK_SLOTS = 64; ///< Entries per NVS chunk

    using SlotCallback = void(*)(uint16_t slot, const CacheEntry& entry, void* ctx);
    using RebuildLogFn = void(*)(uint16_t slot, const char* message, void* ctx);

    /**
     * \brief Get singleton instance.
     */
    static TropicStorage& instance();

    // IService
    bool init() override;    ///< \brief Initialize from NVS cache.
    bool start() override;   ///< \brief Start background rebuild if needed.
    void stop() override;    ///< \brief Stop background operations.
    ServiceState getState() const override;
    const char* getName() const override { return "tropic_storage"; }

    /**
     * \brief Set secure element reference.
     * \param se Pointer to ISecureElement instance.
     */
    void setSecureElement(cdc::hal::ISecureElement* se);

    /**
     * \brief Check if cache is valid and up-to-date.
     * \return true if cache matches TROPIC01 state.
     */
    bool isCacheValid() const;

    /**
     * \brief Iterate over all slots for a module.
     * \param moduleId Module ID to filter.
     * \param cb Callback function for each slot.
     * \param ctx User context passed to callback.
     * \return true if iteration completed.
     */
    bool forEachSlot(uint8_t moduleId, SlotCallback cb, void* ctx);

    // ... (document all remaining methods)
```

## References

- Project documentation style: `main/CMakeLists.txt` comments, `components/cdc_hal/include/cdc_hal/ISecureElement.h`
- Doxygen backslash convention: `\brief`, `\param`, `\return` (per project guidelines)
