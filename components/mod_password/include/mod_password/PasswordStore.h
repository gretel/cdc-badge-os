#pragma once

#include "cdc_hal/ISecureElement.h"
#include <cstdint>
#include <cstddef>

namespace cdc::mod_password {

constexpr uint8_t PASSWORD_TITLE_LEN = 24;
constexpr uint8_t PASSWORD_USERNAME_LEN = 16;
constexpr uint8_t PASSWORD_PASSWORD_LEN = 64;
constexpr uint8_t PASSWORD_URL_LEN = 64;

constexpr size_t PASSWORD_PAYLOAD_MAX =
    cdc::hal::ISecureElement::RMEM_SLOT_SIZE - sizeof(cdc::hal::ISecureElement::RMemHeader);
constexpr size_t PASSWORD_FIXED_PAYLOAD =
    PASSWORD_TITLE_LEN + PASSWORD_USERNAME_LEN + PASSWORD_PASSWORD_LEN + PASSWORD_URL_LEN + sizeof(uint8_t);
constexpr size_t PASSWORD_NOTES_LEN = PASSWORD_PAYLOAD_MAX - PASSWORD_FIXED_PAYLOAD;
static_assert(PASSWORD_NOTES_LEN > 0, "Password notes length must be positive");

struct PasswordEntry {
    char title[PASSWORD_TITLE_LEN + 1];
    char username[PASSWORD_USERNAME_LEN + 1];
    char password[PASSWORD_PASSWORD_LEN + 1];
    char url[PASSWORD_URL_LEN + 1];
    uint8_t totpSlot;
    char notes[PASSWORD_NOTES_LEN + 1];
};

class PasswordStore {
public:
    static constexpr uint8_t TITLE_LEN = PASSWORD_TITLE_LEN;
    static constexpr uint8_t USERNAME_LEN = PASSWORD_USERNAME_LEN;
    static constexpr uint8_t PASSWORD_LEN = PASSWORD_PASSWORD_LEN;
    static constexpr uint8_t URL_LEN = PASSWORD_URL_LEN;
    static constexpr uint8_t TOTP_SLOT_NONE = 0xFF;

    static constexpr size_t PAYLOAD_MAX = PASSWORD_PAYLOAD_MAX;
    static constexpr size_t FIXED_PAYLOAD = PASSWORD_FIXED_PAYLOAD;
    static constexpr size_t NOTES_LEN = PASSWORD_NOTES_LEN;

    struct EntryIndex {
        char title[TITLE_LEN + 1];
        uint16_t slot;
    };

    static PasswordStore& instance();

    bool readEntry(uint16_t slot, PasswordEntry* out) const;
    bool addEntry(const PasswordEntry& entry);
    bool updateEntry(uint16_t slot, const PasswordEntry& entry);
    bool deleteEntry(uint16_t slot);

    bool listEntriesSorted(EntryIndex* entries, uint16_t maxEntries, uint16_t* countOut) const;

    void setSlotRange(uint16_t start, uint16_t end, uint8_t moduleId);
    uint16_t capacity() const;
    bool toPhysicalSlot(uint16_t logicalIndex, uint16_t* slotOut) const;
    bool toLogicalSlot(uint16_t slot, uint16_t* logicalIndexOut) const;
    bool hasSlotRange() const { return hasSlotRange_; }
    uint8_t moduleId() const { return moduleId_; }
    uint16_t rmemStart() const { return rmemStart_; }
    uint16_t rmemEnd() const { return rmemEnd_; }

private:
    PasswordStore() = default;

    bool findFreeSlot(uint16_t* slotOut) const;
    static int compareTitles(const char* a, const char* b);

    bool hasSlotRange_ = false;
    uint16_t rmemStart_ = 0;
    uint16_t rmemEnd_ = 0;
    uint8_t moduleId_ = 0;
};

} // namespace cdc::mod_password
