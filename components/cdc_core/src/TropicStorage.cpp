#include "cdc_core/TropicStorage.h"
#include "cdc_core/TropicSlotMap.h"
#include "cdc_log.h"
#include "nvs_flash.h"
#include <cstring>

static const char* TAG = "TR01_STORE";

namespace cdc::core {

static constexpr uint8_t CACHE_VERSION = 1;
static constexpr const char* NVS_NAMESPACE = "tr01_meta";
static constexpr const char* NVS_KEY_HEADER = "hdr";

TropicStorage& TropicStorage::instance() {
    static TropicStorage inst;
    return inst;
}

bool TropicStorage::init() {
    if (state_ != ServiceState::UNINITIALIZED) {
        return state_ == ServiceState::INITIALIZED || state_ == ServiceState::STARTED;
    }

    header_.version = CACHE_VERSION;
    header_.chunkSlots = CHUNK_SLOTS;
    header_.entrySize = sizeof(CacheEntry);
    header_.mapSignature = computeMapSignature();

    cacheValid_ = loadHeader();

    if (!cacheValid_) {
        LOG_W(TAG, "Cache header missing or stale - rebuild required");
    }

    state_ = ServiceState::INITIALIZED;
    return true;
}

bool TropicStorage::start() {
    if (state_ == ServiceState::UNINITIALIZED) {
        if (!init()) return false;
    }
    state_ = ServiceState::STARTED;
    return true;
}

void TropicStorage::stop() {
    state_ = ServiceState::STOPPED;
}

bool TropicStorage::forEachSlot(uint8_t moduleId, SlotCallback cb, void* ctx) {
    return forEachSlot(moduleId, 0, 0xFFFF, cb, ctx);
}

bool TropicStorage::forEachSlot(uint8_t moduleId, uint16_t fromSlot, uint16_t toSlot,
                                SlotCallback cb, void* ctx) {
    if (!cb) return false;
    if (fromSlot > toSlot) return false;
    auto& slotMap = TropicSlotMap::instance();
    TropicSlotMap::SlotRange range = {};
    if (!slotMap.getRangeByModuleId(moduleId, TropicSlotMap::SlotType::RMEM, &range)) {
        return false;
    }
    if (fromSlot == 0 || toSlot == 0xFFFF) {
        fromSlot = range.start;
        toSlot = range.end;
    }
    if (fromSlot < range.start) fromSlot = range.start;
    if (toSlot > range.end) toSlot = range.end;

    uint16_t startChunk = fromSlot / CHUNK_SLOTS;
    uint16_t endChunk = toSlot / CHUNK_SLOTS;

    CacheEntry entries[CHUNK_SLOTS] = {};
    for (uint16_t chunk = startChunk; chunk <= endChunk; chunk++) {
        if (!loadChunk(chunk, entries)) {
            return false;
        }
        uint16_t slotBase = chunk * CHUNK_SLOTS;
        for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
            uint16_t slot = static_cast<uint16_t>(slotBase + i);
            if (slot < fromSlot || slot > toSlot) continue;
            const CacheEntry& entry = entries[i];
            if (!isEntryUsed(entry)) continue;
            if (!isEntryAllowed(slot, entry.moduleId)) continue;
            if (entry.moduleId != moduleId) continue;
            cb(slot, entry, ctx);
        }
    }
    return true;
}

bool TropicStorage::getSlot(uint8_t moduleId, uint16_t index, SlotCallback cb, void* ctx) {
    if (!cb) return false;

    uint16_t start = 0;
    uint16_t end = 0;

    TropicSlotMap::SlotRange range = {};
    if (!TropicSlotMap::instance().getRangeByModuleId(
            moduleId, TropicSlotMap::SlotType::RMEM, &range)) {
        return false;
    }
    start = range.start;
    end = range.end;

    uint32_t slot = static_cast<uint32_t>(start) + index;
    if (slot > end) return false;

    CacheEntry entry = {};
    if (!getEntry(static_cast<uint16_t>(slot), &entry)) return false;
    if (!isEntryUsed(entry)) return false;
    if (!isEntryAllowed(static_cast<uint16_t>(slot), entry.moduleId)) return false;
    if (entry.moduleId != moduleId) return false;

    cb(static_cast<uint16_t>(slot), entry, ctx);
    return true;
}

bool TropicStorage::writeSlot(uint8_t moduleId, uint16_t slot, const char* name, uint8_t flags) {
    if (!isEntryAllowed(slot, moduleId)) {
        return false;
    }

    CacheEntry entry = {};
    entry.moduleId = moduleId;
    entry.flags = static_cast<uint8_t>(flags | FLAG_USED);
    if (name) {
        strncpy(entry.name, name, sizeof(entry.name) - 1);
        entry.name[sizeof(entry.name) - 1] = '\0';
    }

    if (!saveHeader()) {
        return false;
    }

    return setEntry(slot, entry);
}

bool TropicStorage::eraseSlot(uint8_t moduleId, uint16_t slot) {
    if (!isEntryAllowed(slot, moduleId)) {
        return false;
    }

    CacheEntry entry = {};
    return setEntry(slot, entry);
}

bool TropicStorage::rebuild() {
    return rebuildVerbose(nullptr, nullptr);
}

bool TropicStorage::rebuildVerbose(RebuildLogFn logFn, void* ctx) {
    if (!secureElement_) {
        LOG_E(TAG, "No secure element set");
        return false;
    }
    if (!secureElement_->isSessionActive()) {
        if (!secureElement_->sessionStart()) {
            if (logFn) logFn(0xFFFF, "session start failed", ctx);
            return false;
        }
    }

    CacheEntry chunk[CHUNK_SLOTS] = {};
    uint16_t totalChunks =
        static_cast<uint16_t>((TropicSlotMap::instance().rmemMax() + 1u) / CHUNK_SLOTS);

    for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
        memset(chunk, 0, sizeof(chunk));
        uint16_t slotBase = chunkIndex * CHUNK_SLOTS;
        for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
            uint16_t slot = static_cast<uint16_t>(slotBase + i);
            if (slot == 0) continue;

            cdc::hal::ISecureElement::RMemHeader header = {};
            uint16_t payloadLen = 0;
            auto res = secureElement_->rmemReadWithHeader(slot, &header, nullptr, 0, &payloadLen);
            if (res == cdc::hal::SeResult::OK) {
                if (!isEntryAllowed(slot, header.moduleId)) {
                    if (logFn) logFn(slot, "mismatched module", ctx);
                    continue;
                }

                CacheEntry& entry = chunk[i];
                entry.moduleId = header.moduleId;
                entry.flags = static_cast<uint8_t>(header.flags | FLAG_USED);
                strncpy(entry.name, header.name, sizeof(entry.name) - 1);
                entry.name[sizeof(entry.name) - 1] = '\0';
                if (logFn) logFn(slot, entry.name, ctx);
                continue;
            }

            uint8_t temp[4];
            uint16_t readLen = 0;
            auto rawRes = secureElement_->rmemRead(slot, temp, sizeof(temp), &readLen);
            if (rawRes == cdc::hal::SeResult::OK && readLen > 0) {
                if (logFn) logFn(slot, "invalid header", ctx);
            } else if (rawRes != cdc::hal::SeResult::SLOT_EMPTY) {
                if (logFn) logFn(slot, "read failed", ctx);
            }
        }
        if (!saveChunk(chunkIndex, chunk)) {
            if (logFn) logFn(slotBase, "nvs write failed", ctx);
            return false;
        }
    }

    cacheValid_ = saveHeader();
    return cacheValid_;
}

bool TropicStorage::cleanup() {
    if (!secureElement_) {
        LOG_E(TAG, "No secure element set");
        return false;
    }

    CacheEntry entries[CHUNK_SLOTS] = {};
    uint16_t totalChunks =
        static_cast<uint16_t>((TropicSlotMap::instance().rmemMax() + 1u) / CHUNK_SLOTS);

    for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
        if (!loadChunk(chunkIndex, entries)) {
            return false;
        }
        uint16_t slotBase = chunkIndex * CHUNK_SLOTS;
        bool changed = false;
        for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
            uint16_t slot = static_cast<uint16_t>(slotBase + i);
            CacheEntry& entry = entries[i];
            if (!isEntryUsed(entry)) continue;
            if (isEntryAllowed(slot, entry.moduleId)) continue;

            LOG_W(TAG, "Cleanup: slot %u has mismatched module %u", slot, entry.moduleId);
            secureElement_->rmemErase(slot);
            memset(&entry, 0, sizeof(entry));
            changed = true;
        }
        if (changed) {
            if (!saveChunk(chunkIndex, entries)) {
                return false;
            }
        }
    }

    return rebuild();
}

bool TropicStorage::loadHeader() {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return false;
    }
    size_t len = sizeof(CacheHeader);
    CacheHeader stored = {};
    esp_err_t err = nvs_get_blob(nvs, NVS_KEY_HEADER, &stored, &len);
    nvs_close(nvs);
    if (err != ESP_OK || len != sizeof(CacheHeader)) {
        return false;
    }

    if (stored.version != CACHE_VERSION || stored.chunkSlots != CHUNK_SLOTS ||
        stored.entrySize != sizeof(CacheEntry)) {
        return false;
    }
    if (stored.mapSignature != computeMapSignature()) {
        return false;
    }
    header_ = stored;
    return true;
}

bool TropicStorage::saveHeader() {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        return false;
    }
    esp_err_t err = nvs_set_blob(nvs, NVS_KEY_HEADER, &header_, sizeof(header_));
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err == ESP_OK;
}

bool TropicStorage::loadChunk(uint16_t chunkIndex, CacheEntry* entries) {
    if (!entries) return false;
    memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return true;
    }
    char key[12];
    snprintf(key, sizeof(key), "c%u", chunkIndex);
    size_t len = sizeof(CacheEntry) * CHUNK_SLOTS;
    esp_err_t err = nvs_get_blob(nvs, key, entries, &len);
    nvs_close(nvs);

    if (err != ESP_OK || len != sizeof(CacheEntry) * CHUNK_SLOTS) {
        memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);
        return true;
    }
    return true;
}

bool TropicStorage::saveChunk(uint16_t chunkIndex, const CacheEntry* entries) {
    if (!entries) return false;
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        return false;
    }
    char key[12];
    snprintf(key, sizeof(key), "c%u", chunkIndex);
    esp_err_t err = nvs_set_blob(nvs, key, entries, sizeof(CacheEntry) * CHUNK_SLOTS);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err == ESP_OK;
}

uint32_t TropicStorage::computeMapSignature() const {
    // FNV-1a 32-bit over map constants
    uint32_t hash = 2166136261u;
    auto mix = [&hash](uint32_t v) {
        hash ^= v;
        hash *= 16777619u;
    };

    mix(TropicSlotMap::instance().computeMapSignature());

    return hash;
}

bool TropicStorage::setEntry(uint16_t slot, const CacheEntry& entry) {
    uint16_t chunkIndex = slot / CHUNK_SLOTS;
    uint16_t offset = slot % CHUNK_SLOTS;
    CacheEntry entries[CHUNK_SLOTS] = {};
    if (!loadChunk(chunkIndex, entries)) return false;
    entries[offset] = entry;
    return saveChunk(chunkIndex, entries);
}

bool TropicStorage::getEntry(uint16_t slot, CacheEntry* entry) {
    if (!entry) return false;
    uint16_t chunkIndex = slot / CHUNK_SLOTS;
    uint16_t offset = slot % CHUNK_SLOTS;
    CacheEntry entries[CHUNK_SLOTS] = {};
    if (!loadChunk(chunkIndex, entries)) return false;
    *entry = entries[offset];
    return true;
}

bool TropicStorage::isEntryUsed(const CacheEntry& entry) const {
    return (entry.flags & FLAG_USED) != 0;
}

bool TropicStorage::isEntryAllowed(uint16_t slot, uint8_t moduleId) const {
    return TropicSlotMap::instance().isRmemAllowedForModuleId(slot, moduleId);
}

} // namespace cdc::core
