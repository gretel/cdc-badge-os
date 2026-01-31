#include "cdc_core/TropicSlotMap.h"
#include "tropic_slot_map.h"
#include "cdc_log.h"
#include <cstring>

static const char* TAG = "SlotMap";

namespace cdc::core {

struct SlotMapEntry {
    TropicSlotMap::SlotType type;
    const char* moduleName;
    uint8_t moduleId;
    uint16_t start;
    uint16_t end;
};

#define BUILD_ECC_ENTRY(name, id, start, end) \
    { TropicSlotMap::SlotType::ECC, name, static_cast<uint8_t>(id), \
      static_cast<uint16_t>(start), static_cast<uint16_t>(end) },
#define BUILD_RMEM_ENTRY(name, id, start, end) \
    { TropicSlotMap::SlotType::RMEM, name, static_cast<uint8_t>(id), \
      static_cast<uint16_t>(start), static_cast<uint16_t>(end) },

static const SlotMapEntry kSlotMap[] = {
    TROPIC_ECC_SLOT_MAP(BUILD_ECC_ENTRY)
    TROPIC_RMEM_SLOT_MAP(BUILD_RMEM_ENTRY)
};

static constexpr size_t kSlotMapCount = sizeof(kSlotMap) / sizeof(kSlotMap[0]);

TropicSlotMap& TropicSlotMap::instance() {
    static TropicSlotMap inst;
    return inst;
}

TropicSlotMap::TropicSlotMap() {
    validateOnce();
}

void TropicSlotMap::setError(const char* message) {
    if (!valid_) return;
    valid_ = false;
    errorMessage_ = message;
    LOG_E(TAG, "%s", message ? message : "slot map error");
}

void TropicSlotMap::validateOnce() {
    valid_ = true;
    errorMessage_ = nullptr;

    for (size_t i = 0; i < kSlotMapCount; i++) {
        const auto& a = kSlotMap[i];
        if (!a.moduleName || a.moduleName[0] == '\0') {
            setError("slot map entry with empty module name");
            return;
        }
        if (a.moduleId == MODULE_ID_UNKNOWN) {
            setError("slot map entry with unknown module id");
            return;
        }
        if (a.start > a.end) {
            setError("slot map range has start > end");
            return;
        }
        if (a.start == 0 || a.end == 0) {
            setError("slot map range uses reserved slot 0");
            return;
        }
        if (a.type == SlotType::ECC) {
            if (a.start < cdc::tropic_map::ECC_SLOT_MIN ||
                a.end > cdc::tropic_map::ECC_SLOT_MAX) {
                setError("slot map ECC range out of bounds");
                return;
            }
            if (a.start <= cdc::tropic_map::ECC_SLOT_RESERVED) {
                setError("slot map ECC range includes reserved slot");
                return;
            }
        } else {
            if (a.start < cdc::tropic_map::RMEM_SLOT_MIN ||
                a.end > cdc::tropic_map::RMEM_SLOT_MAX) {
                setError("slot map RMEM range out of bounds");
                return;
            }
            if (a.start < cdc::tropic_map::RMEM_SLOT_MIN_ALLOC ||
                a.end < cdc::tropic_map::RMEM_SLOT_MIN_ALLOC) {
                setError("slot map RMEM range below minimum allowed");
                return;
            }
        }

        for (size_t j = i + 1; j < kSlotMapCount; j++) {
            const auto& b = kSlotMap[j];
            if (!b.moduleName) continue;

            if (a.type == b.type) {
                bool overlap = !(a.end < b.start || b.end < a.start);
                if (overlap) {
                    setError("slot map overlap detected");
                    return;
                }
            }

            if (strcmp(a.moduleName, b.moduleName) == 0 && a.type == b.type) {
                setError("slot map duplicate module/type entry");
                return;
            }

            if (a.moduleId == b.moduleId &&
                strcmp(a.moduleName, b.moduleName) != 0) {
                setError("slot map module id used by multiple names");
                return;
            }
        }
    }
}

bool TropicSlotMap::getRangeByName(const char* moduleName, SlotType type, SlotRange* out) const {
    if (!moduleName || !out) return false;
    for (size_t i = 0; i < kSlotMapCount; i++) {
        const auto& entry = kSlotMap[i];
        if (entry.type != type) continue;
        if (strcmp(entry.moduleName, moduleName) != 0) continue;
        out->valid = true;
        out->type = entry.type;
        out->moduleName = entry.moduleName;
        out->moduleId = entry.moduleId;
        out->start = entry.start;
        out->end = entry.end;
        return true;
    }
    return false;
}

bool TropicSlotMap::getRangeByModuleId(uint8_t moduleId, SlotType type, SlotRange* out) const {
    if (!out) return false;
    for (size_t i = 0; i < kSlotMapCount; i++) {
        const auto& entry = kSlotMap[i];
        if (entry.type != type) continue;
        if (entry.moduleId != moduleId) continue;
        out->valid = true;
        out->type = entry.type;
        out->moduleName = entry.moduleName;
        out->moduleId = entry.moduleId;
        out->start = entry.start;
        out->end = entry.end;
        return true;
    }
    return false;
}

bool TropicSlotMap::isRmemAllowedForModuleId(uint16_t slot, uint8_t moduleId) const {
    SlotRange range;
    if (!getRangeByModuleId(moduleId, SlotType::RMEM, &range)) {
        return false;
    }
    return slot >= range.start && slot <= range.end;
}

uint16_t TropicSlotMap::rmemMax() const {
    return cdc::tropic_map::RMEM_SLOT_MAX;
}

uint32_t TropicSlotMap::computeMapSignature() const {
    uint32_t hash = 2166136261u;
    auto mix = [&hash](uint32_t v) {
        hash ^= v;
        hash *= 16777619u;
    };

    mix(cdc::tropic_map::ECC_SLOT_MIN);
    mix(cdc::tropic_map::ECC_SLOT_MAX);
    mix(cdc::tropic_map::ECC_SLOT_RESERVED);
    mix(cdc::tropic_map::RMEM_SLOT_MIN);
    mix(cdc::tropic_map::RMEM_SLOT_MAX);
    mix(cdc::tropic_map::RMEM_SLOT_RESERVED);
    mix(cdc::tropic_map::RMEM_SLOT_MIN_ALLOC);

    for (size_t i = 0; i < kSlotMapCount; i++) {
        const auto& entry = kSlotMap[i];
        mix(static_cast<uint32_t>(entry.type));
        mix(entry.moduleId);
        mix(entry.start);
        mix(entry.end);
    }

    return hash;
}

} // namespace cdc::core
