#include "mod_password/PasswordStore.h"
#include "cdc_core/TropicStorage.h"
#include "cdc_hal/ISecureElement.h"
#include "cdc_log.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>

static const char* TAG = "PASSWORD";

namespace cdc::mod_password {

#pragma pack(push, 1)
struct PasswordPayload {
    char title[PasswordStore::TITLE_LEN];
    char username[PasswordStore::USERNAME_LEN];
    char password[PasswordStore::PASSWORD_LEN];
    char url[PasswordStore::URL_LEN];
    uint8_t totpSlot;
    char notes[PasswordStore::NOTES_LEN];
};
#pragma pack(pop)

static_assert(sizeof(PasswordPayload) == PasswordStore::PAYLOAD_MAX, "Password payload size mismatch");

/**
 * \brief Copies text into bounded destination buffer.
 * \param dst Destination buffer.
 * \param dstSize Destination size.
 * \param src Source string.
 */
static void copyText(char* dst, size_t dstSize, const char* src) {
    if (!dst || dstSize == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

/**
 * \brief Returns singleton password store instance.
 * \return Store singleton reference.
 */
PasswordStore& PasswordStore::instance() {
    static PasswordStore inst;
    return inst;
}

/**
 * \brief Configures logical-to-physical slot mapping for password entries.
 * \param start First RMEM slot.
 * \param end Last RMEM slot.
 * \param moduleId Owning module identifier.
 */
void PasswordStore::setSlotRange(uint16_t start, uint16_t end, uint8_t moduleId) {
    if (start > end || start == 0 || end == 0) {
        hasSlotRange_ = false;
        rmemStart_ = 0;
        rmemEnd_ = 0;
        moduleId_ = 0;
        return;
    }
    hasSlotRange_ = true;
    rmemStart_ = start;
    rmemEnd_ = end;
    moduleId_ = moduleId;
}

/**
 * \brief Returns available entry capacity from configured slot range.
 * \return Number of addressable logical entries.
 */
uint16_t PasswordStore::capacity() const {
    if (!hasSlotRange_) return 0;
    return static_cast<uint16_t>(rmemEnd_ - rmemStart_ + 1);
}

/**
 * \brief Converts logical entry index to physical RMEM slot.
 * \param logicalIndex Logical index.
 * \param slotOut Output physical slot.
 * \return `true` on valid mapping.
 */
bool PasswordStore::toPhysicalSlot(uint16_t logicalIndex, uint16_t* slotOut) const {
    if (!slotOut) return false;
    if (!hasSlotRange_) return false;
    uint32_t slot = static_cast<uint32_t>(rmemStart_) + logicalIndex;
    if (slot > rmemEnd_) return false;
    *slotOut = static_cast<uint16_t>(slot);
    return true;
}

/**
 * \brief Converts physical RMEM slot to logical entry index.
 * \param slot Physical slot.
 * \param logicalIndexOut Output logical index.
 * \return `true` on valid mapping.
 */
bool PasswordStore::toLogicalSlot(uint16_t slot, uint16_t* logicalIndexOut) const {
    if (!logicalIndexOut) return false;
    if (!hasSlotRange_) return false;
    if (slot < rmemStart_ || slot > rmemEnd_) return false;
    *logicalIndexOut = static_cast<uint16_t>(slot - rmemStart_);
    return true;
}

/**
 * \brief Reads one password entry from secure-element storage.
 * \param slot Logical slot index.
 * \param out Output entry.
 * \return `true` on success.
 */
bool PasswordStore::readEntry(uint16_t slot, PasswordEntry* out) const {
    if (!out) return false;
    if (!hasSlotRange_) return false;
    uint16_t physSlot = 0;
    if (!toPhysicalSlot(slot, &physSlot)) return false;

    auto* se = cdc::hal::getSecureElementInstance();
    if (!se) return false;

    cdc::hal::ISecureElement::RMemHeader header = {};
    PasswordPayload payload = {};
    uint16_t payloadLen = 0;

    auto res = se->rmemReadWithHeader(physSlot, &header,
                                      reinterpret_cast<uint8_t*>(&payload),
                                      sizeof(payload), &payloadLen);
    if (res != cdc::hal::SeResult::OK) {
        return false;
    }

    if (header.moduleId != moduleId_) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    if (payload.title[0]) {
        copyText(out->title, sizeof(out->title), payload.title);
    } else {
        copyText(out->title, sizeof(out->title), header.name);
    }
    copyText(out->username, sizeof(out->username), payload.username);
    copyText(out->password, sizeof(out->password), payload.password);
    copyText(out->url, sizeof(out->url), payload.url);
    out->totpSlot = payload.totpSlot;
    copyText(out->notes, sizeof(out->notes), payload.notes);

    return true;
}

/**
 * \brief Finds first free physical slot in configured range.
 * \param slotOut Output physical slot.
 * \return `true` if a free slot exists.
 */
bool PasswordStore::findFreeSlot(uint16_t* slotOut) const {
    if (!slotOut) return false;
    if (!hasSlotRange_) return false;

    uint16_t cap = capacity();
    if (cap == 0) return false;
    auto used = std::unique_ptr<bool[]>(new (std::nothrow) bool[cap]);
    if (!used) return false;
    memset(used.get(), 0, cap * sizeof(bool));

    struct Ctx {
        bool* used;
        uint16_t base;
        uint16_t cap;
    } ctx = { used.get(), rmemStart_, cap };

    auto cb = [](uint16_t slot, const cdc::core::TropicStorage::CacheEntry&, void* user) {
        auto* c = static_cast<Ctx*>(user);
        if (slot < c->base) return;
        uint16_t idx = slot - c->base;
        if (idx < c->cap) {
            c->used[idx] = true;
        }
    };

    cdc::core::TropicStorage::instance().forEachSlot(
        moduleId_, rmemStart_, rmemEnd_, cb, &ctx);

    for (uint16_t i = 0; i < cap; i++) {
        if (!used[i]) {
            uint16_t candidate = static_cast<uint16_t>(rmemStart_ + i);
            if (candidate <= rmemEnd_) {
                *slotOut = candidate;
                return true;
            }
            return false;
        }
    }

    return false;
}

/**
 * \brief Adds a new password entry into first free slot.
 * \param entry Entry data.
 * \return `true` on successful write.
 */
bool PasswordStore::addEntry(const PasswordEntry& entry) {
    if (!hasSlotRange_) return false;
    uint16_t slot = 0;
    if (!findFreeSlot(&slot)) {
        LOG_W(TAG, "No free password slots");
        return false;
    }

    PasswordPayload payload = {};
    copyText(payload.title, sizeof(payload.title), entry.title);
    copyText(payload.username, sizeof(payload.username), entry.username);
    copyText(payload.password, sizeof(payload.password), entry.password);
    copyText(payload.url, sizeof(payload.url), entry.url);
    payload.totpSlot = entry.totpSlot;
    copyText(payload.notes, sizeof(payload.notes), entry.notes);

    char headerName[cdc::hal::ISecureElement::RMEM_NAME_LEN + 1] = {};
    if (entry.title[0]) {
        copyText(headerName, sizeof(headerName), entry.title);
    } else {
        copyText(headerName, sizeof(headerName), "Password");
    }

    auto* se = cdc::hal::getSecureElementInstance();
    if (!se) return false;

    auto res = se->rmemWriteWithHeader(
        slot,
        moduleId_,
        headerName,
        0,
        reinterpret_cast<const uint8_t*>(&payload),
        sizeof(payload)
    );

    if (res != cdc::hal::SeResult::OK) {
        LOG_E(TAG, "Failed to write slot %u", slot);
        return false;
    }

    cdc::core::TropicStorage::instance().writeSlot(moduleId_, slot, headerName, 0);

    return true;
}

/**
 * \brief Updates existing password entry.
 * \param slot Logical slot index.
 * \param entry New entry data.
 * \return `true` on successful write.
 */
bool PasswordStore::updateEntry(uint16_t slot, const PasswordEntry& entry) {
    if (!hasSlotRange_) return false;
    uint16_t physSlot = 0;
    if (!toPhysicalSlot(slot, &physSlot)) return false;

    PasswordPayload payload = {};
    copyText(payload.title, sizeof(payload.title), entry.title);
    copyText(payload.username, sizeof(payload.username), entry.username);
    copyText(payload.password, sizeof(payload.password), entry.password);
    copyText(payload.url, sizeof(payload.url), entry.url);
    payload.totpSlot = entry.totpSlot;
    copyText(payload.notes, sizeof(payload.notes), entry.notes);

    char headerName[cdc::hal::ISecureElement::RMEM_NAME_LEN + 1] = {};
    if (entry.title[0]) {
        copyText(headerName, sizeof(headerName), entry.title);
    } else {
        copyText(headerName, sizeof(headerName), "Password");
    }

    auto* se = cdc::hal::getSecureElementInstance();
    if (!se) return false;

    auto res = se->rmemWriteWithHeader(
        physSlot,
        moduleId_,
        headerName,
        0,
        reinterpret_cast<const uint8_t*>(&payload),
        sizeof(payload)
    );

    if (res != cdc::hal::SeResult::OK) {
        LOG_E(TAG, "Failed to write slot %u", slot);
        return false;
    }

    cdc::core::TropicStorage::instance().writeSlot(moduleId_, physSlot, headerName, 0);

    return true;
}

/**
 * \brief Deletes entry at logical slot index.
 * \param slot Logical slot index.
 * \return `true` on successful erase.
 */
bool PasswordStore::deleteEntry(uint16_t slot) {
    if (!hasSlotRange_) return false;
    uint16_t physSlot = 0;
    if (!toPhysicalSlot(slot, &physSlot)) return false;

    auto* se = cdc::hal::getSecureElementInstance();
    if (!se) return false;

    auto res = se->rmemErase(physSlot);
    if (res != cdc::hal::SeResult::OK) {
        return false;
    }

    cdc::core::TropicStorage::instance().eraseSlot(moduleId_, physSlot);
    return true;
}

/**
 * \brief Case-insensitive title comparison helper.
 * \param a First title.
 * \param b Second title.
 * \return Negative, zero, or positive compare result.
 */
int PasswordStore::compareTitles(const char* a, const char* b) {
    if (!a) return b ? -1 : 0;
    if (!b) return 1;
    while (*a || *b) {
        int ca = *a ? std::tolower(static_cast<unsigned char>(*a)) : 0;
        int cb = *b ? std::tolower(static_cast<unsigned char>(*b)) : 0;
        if (ca != cb) return ca - cb;
        if (*a) ++a;
        if (*b) ++b;
    }
    return 0;
}

/**
 * \brief Lists entries sorted alphabetically by title.
 * \param entries Output entry index array.
 * \param maxEntries Maximum writable entries.
 * \param countOut Output number of entries.
 * \return `true` on successful listing.
 */
bool PasswordStore::listEntriesSorted(EntryIndex* entries, uint16_t maxEntries, uint16_t* countOut) const {
    if (!entries || !countOut) return false;
    if (!hasSlotRange_) return false;

    struct Ctx {
        EntryIndex* entries;
        uint16_t* count;
        uint16_t max;
    } ctx = { entries, countOut, maxEntries };

    *countOut = 0;
    auto cb = [](uint16_t slot, const cdc::core::TropicStorage::CacheEntry& entry, void* user) {
        auto* ctx = static_cast<Ctx*>(user);
        if (!ctx || !ctx->entries || !ctx->count) return;
        if (*ctx->count >= ctx->max) return;

        uint16_t logical = 0;
        if (!PasswordStore::instance().toLogicalSlot(slot, &logical)) return;

        uint16_t idx = *ctx->count;
        copyText(ctx->entries[idx].title, sizeof(ctx->entries[idx].title), entry.name);
        ctx->entries[idx].slot = logical;
        (*ctx->count)++;
    };

    cdc::core::TropicStorage::instance().forEachSlot(
        moduleId_, rmemStart_, rmemEnd_, cb, &ctx);

    std::sort(entries, entries + *countOut, [](const EntryIndex& a, const EntryIndex& b) {
        return PasswordStore::compareTitles(a.title, b.title) < 0;
    });

    return true;
}

} // namespace cdc::mod_password
