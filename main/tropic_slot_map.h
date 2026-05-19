#pragma once

#include <cstdint>

// Compile-time TROPIC01 slot map (edit before build)
//
// You have 32 ECC slots and 512 R-Memory slots.
// Slot 0 (ECC and RMEM) is reserved.
// RMEM slots below 32 are not allowed for module allocation.
//
// Syntax is always:
//   ECC_SLOT_<MODULENAME>_START / ECC_SLOT_<MODULENAME>_END
//   RMEM_SLOT_<MODULENAME>_START / RMEM_SLOT_<MODULENAME>_END
//
// Module names should match module runtime names (IModule::getName()).

namespace cdc::tropic_map {

static constexpr uint8_t ECC_SLOT_MIN = 0;
static constexpr uint8_t ECC_SLOT_MAX = 31;
static constexpr uint8_t ECC_SLOT_RESERVED = 0;

static constexpr uint16_t RMEM_SLOT_MIN = 0;
static constexpr uint16_t RMEM_SLOT_MAX = 511;
static constexpr uint16_t RMEM_SLOT_RESERVED = 0;
static constexpr uint16_t RMEM_SLOT_MIN_ALLOC = 32;

// Module IDs (must be unique, 0-254). 255 is reserved for UNKNOWN.
// MODULE_ID values are permanent on-device identifiers; never reassign them.
// Add new modules with new IDs only, never reuse a freed slot/ID.
#define MODULE_ID_MOD_SYSTEM 0
#define MODULE_ID_MOD_GPG 2
#define MODULE_ID_MOD_CA 3
#define MODULE_ID_MOD_FIDO2 4
#define MODULE_ID_MOD_TOTP 5
#define MODULE_ID_MOD_PASSWORD 6
#define MODULE_ID_MOD_HOMEASSISTANT 7
#define MODULE_ID_UNKNOWN 255

// ECC slot ranges
#define ECC_SLOT_MOD_GPG_START 1
#define ECC_SLOT_MOD_GPG_END 3
#define ECC_SLOT_MOD_CA_START 4
#define ECC_SLOT_MOD_CA_END 4
#define ECC_SLOT_MOD_FIDO2_START 5
#define ECC_SLOT_MOD_FIDO2_END 31

// RMEM slot ranges (must be >= RMEM_SLOT_MIN_ALLOC)
#define RMEM_SLOT_MOD_TOTP_START 32
#define RMEM_SLOT_MOD_TOTP_END 131
#define RMEM_SLOT_MOD_FIDO2_START 132
#define RMEM_SLOT_MOD_FIDO2_END 158
#define RMEM_SLOT_MOD_PASSWORD_START 159
#define RMEM_SLOT_MOD_PASSWORD_END 500
#define RMEM_SLOT_MOD_HOMEASSISTANT_START 501
#define RMEM_SLOT_MOD_HOMEASSISTANT_END 501
#define RMEM_SLOT_MOD_GPG_START 502
#define RMEM_SLOT_MOD_GPG_END 511

// Slot map entries (do not include reserved slots)
#define TROPIC_ECC_SLOT_MAP(X) \
    X("mod_gpg", MODULE_ID_MOD_GPG, ECC_SLOT_MOD_GPG_START, ECC_SLOT_MOD_GPG_END) \
    X("mod_ca", MODULE_ID_MOD_CA, ECC_SLOT_MOD_CA_START, ECC_SLOT_MOD_CA_END) \
    X("mod_fido2", MODULE_ID_MOD_FIDO2, ECC_SLOT_MOD_FIDO2_START, ECC_SLOT_MOD_FIDO2_END)

#define TROPIC_RMEM_SLOT_MAP(X) \
    X("mod_totp", MODULE_ID_MOD_TOTP, RMEM_SLOT_MOD_TOTP_START, RMEM_SLOT_MOD_TOTP_END) \
    X("mod_fido2", MODULE_ID_MOD_FIDO2, RMEM_SLOT_MOD_FIDO2_START, RMEM_SLOT_MOD_FIDO2_END) \
    X("mod_password", MODULE_ID_MOD_PASSWORD, RMEM_SLOT_MOD_PASSWORD_START, RMEM_SLOT_MOD_PASSWORD_END) \
    X("mod_homeassistant", MODULE_ID_MOD_HOMEASSISTANT, RMEM_SLOT_MOD_HOMEASSISTANT_START, RMEM_SLOT_MOD_HOMEASSISTANT_END) \
    X("mod_gpg", MODULE_ID_MOD_GPG, RMEM_SLOT_MOD_GPG_START, RMEM_SLOT_MOD_GPG_END)

} // namespace cdc::tropic_map
