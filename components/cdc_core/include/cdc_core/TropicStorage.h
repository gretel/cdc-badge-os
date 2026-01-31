#pragma once

#include "cdc_core/IService.h"
#include "cdc_hal/ISecureElement.h"
#include <cstdint>

namespace cdc::core {

class TropicStorage : public IService {
public:
    struct CacheEntry {
        uint8_t moduleId;
        uint8_t flags;
        char name[cdc::hal::ISecureElement::RMEM_NAME_LEN];
    } __attribute__((packed));

    static constexpr uint8_t FLAG_USED = 0x01;
    static constexpr uint16_t CHUNK_SLOTS = 64;

    using SlotCallback = void(*)(uint16_t slot, const CacheEntry& entry, void* ctx);
    using RebuildLogFn = void(*)(uint16_t slot, const char* message, void* ctx);

    static TropicStorage& instance();

    // IService
    bool init() override;
    bool start() override;
    void stop() override;
    ServiceState getState() const override { return state_; }
    const char* getName() const override { return "tropic_storage"; }

    void setSecureElement(cdc::hal::ISecureElement* se) { secureElement_ = se; }

    bool isCacheValid() const { return cacheValid_; }

    // Iteration helpers
    bool forEachSlot(uint8_t moduleId, SlotCallback cb, void* ctx);
    bool forEachSlot(uint8_t moduleId, uint16_t fromSlot, uint16_t toSlot,
                     SlotCallback cb, void* ctx);
    bool getSlot(uint8_t moduleId, uint16_t index, SlotCallback cb, void* ctx);

    // NVS cache updates (does not touch TROPIC01)
    bool writeSlot(uint8_t moduleId, uint16_t slot, const char* name, uint8_t flags);
    bool eraseSlot(uint8_t moduleId, uint16_t slot);

    // Maintenance
    bool rebuild();
    bool rebuildVerbose(RebuildLogFn logFn, void* ctx);
    bool cleanup();

private:
    TropicStorage() = default;

    struct CacheHeader {
        uint8_t version;
        uint8_t chunkSlots;
        uint16_t entrySize;
        uint32_t mapSignature;
    } __attribute__((packed));

    bool loadHeader();
    bool saveHeader();
    bool loadChunk(uint16_t chunkIndex, CacheEntry* entries);
    bool saveChunk(uint16_t chunkIndex, const CacheEntry* entries);
    uint32_t computeMapSignature() const;

    bool setEntry(uint16_t slot, const CacheEntry& entry);
    bool getEntry(uint16_t slot, CacheEntry* entry);
    bool isEntryUsed(const CacheEntry& entry) const;
    bool isEntryAllowed(uint16_t slot, uint8_t moduleId) const;

    ServiceState state_ = ServiceState::UNINITIALIZED;
    cdc::hal::ISecureElement* secureElement_ = nullptr;
    CacheHeader header_ = {};
    bool cacheValid_ = false;
};

} // namespace cdc::core
