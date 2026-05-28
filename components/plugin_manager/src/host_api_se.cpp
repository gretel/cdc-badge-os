/**
 * \file host_api_se.cpp
 * \brief TROPIC01 SecureElement host API with per-call slot capability checks.
 */

#include "cdc_hal/ISecureElement.h"
#include "plugin_manager/host_api.h"
#include "plugin_manager/Plugin.h"
#include "plugin_manager/PluginManager.h"

#include <cstring>
#include <string>

extern "C" void* plg_get_active_plugin(void);

using cdc::hal::ISecureElement;
using cdc::hal::SeResult;
using cdc::hal::EccCurve;
using cdc::hal::getSecureElementInstance;

namespace {

// Plugin rmem pool and pool tag. Must match RMEM_SLOT_MOD_PLUGINS_START/END
// and MODULE_ID_PLUGIN_POOL in main/tropic_slot_map.h.
constexpr uint16_t PLG_RMEM_POOL_START      = 501;
constexpr uint16_t PLG_RMEM_POOL_END        = 511;
constexpr uint8_t  PLG_RMEM_POOL_MODULE_ID  = 7;
constexpr uint8_t  RMEM_HEADER_MAGIC        = 0xCD;
constexpr uint8_t  RMEM_NAME_FIELD_LEN      = ISecureElement::RMEM_NAME_LEN;

ISecureElement* se() { return getSecureElementInstance(); }

cdc::plugin_manager::Plugin* active() {
    return static_cast<cdc::plugin_manager::Plugin*>(plg_get_active_plugin());
}

bool active_declares_rmem(const std::string& name) {
    auto* p = active();
    if (!p) return false;
    for (const std::string& n : p->manifest().capabilities.rmem) {
        if (n == name) return true;
    }
    return false;
}

bool any_installed_declares_rmem(const std::string& name) {
    auto& mgr = cdc::plugin_manager::PluginManager::instance();
    for (const std::string& id : mgr.listInstalledIds()) {
        auto man = mgr.getManifest(id);
        if (!man) continue;
        for (const std::string& n : man->capabilities.rmem) {
            if (n == name) return true;
        }
    }
    return false;
}

// Scan the plugin pool. Output `out_existing` is the slot holding `name`, or
// -1 if none. Output `out_reclaimable` is a slot that is empty or stale (not
// claimed by any installed plugin), or -1 if pool is fully populated by
// live names.
void scan_plugin_pool(const std::string& name, int& out_existing, int& out_reclaimable) {
    out_existing = -1;
    out_reclaimable = -1;
    auto* s = se();
    if (!s) return;

    for (uint16_t slot = PLG_RMEM_POOL_START; slot <= PLG_RMEM_POOL_END; ++slot) {
        ISecureElement::RMemHeader hdr{};
        SeResult rr = s->rmemReadWithHeader(slot, &hdr, nullptr, 0, nullptr);
        if (rr != SeResult::OK || hdr.magic != RMEM_HEADER_MAGIC) {
            if (out_reclaimable < 0) out_reclaimable = slot;
            continue;
        }
        std::string hdr_name(hdr.name, ::strnlen(hdr.name, RMEM_NAME_FIELD_LEN));
        if (hdr_name == name) {
            out_existing = slot;
            return;
        }
        if (out_reclaimable < 0 && !any_installed_declares_rmem(hdr_name)) {
            out_reclaimable = slot;
        }
    }
}

bool ecc_allowed(uint8_t slot) {
    auto* p = active();
    if (!p) return false;
    if (slot == 0) return false;  // attestation slot is system-only
    for (uint8_t s : p->manifest().capabilities.ecc_slots) {
        if (s == slot) return true;
    }
    return false;
}

int se_rc(SeResult r) {
    switch (r) {
        case SeResult::OK:               return HOST_OK;
        case SeResult::INVALID_PARAM:    return HOST_ERR_INVALID_ARG;
        case SeResult::SLOT_EMPTY:       return HOST_ERR_NOT_FOUND;
        case SeResult::SESSION_REQUIRED: return HOST_ERR_BUSY;
        case SeResult::NOT_SUPPORTED:    return HOST_ERR_NOT_SUPPORTED;
        default:                         return HOST_ERR_GENERIC;
    }
}

EccCurve curve_for(uint8_t curve) {
    return curve == ECC_CURVE_ED25519 ? EccCurve::ED25519 : EccCurve::P256;
}

}  // namespace

extern "C" {

int host_rmem_read_named(const char* name, uint8_t* buf, size_t* len)
{
    if (!name || !len) return HOST_ERR_INVALID_ARG;
    std::string name_s(name);
    if (!active_declares_rmem(name_s)) return HOST_ERR_NO_CAPABILITY;
    auto* s = se();
    if (!s) return HOST_ERR_NOT_FOUND;

    int existing = -1, reclaimable = -1;
    scan_plugin_pool(name_s, existing, reclaimable);
    if (existing < 0) return HOST_ERR_NOT_FOUND;

    ISecureElement::RMemHeader hdr{};
    uint16_t payload_len = 0;
    SeResult rr = s->rmemReadWithHeader(static_cast<uint16_t>(existing), &hdr,
                                         buf, static_cast<uint16_t>(*len), &payload_len);
    if (rr != SeResult::OK) return se_rc(rr);
    *len = payload_len;
    return HOST_OK;
}

int host_rmem_write_named(const char* name, const uint8_t* buf, size_t len)
{
    if (!name || (!buf && len > 0)) return HOST_ERR_INVALID_ARG;
    std::string name_s(name);
    if (name_s.empty() || name_s.size() > HOST_RMEM_NAME_MAX) return HOST_ERR_INVALID_ARG;
    if (!active_declares_rmem(name_s)) return HOST_ERR_NO_CAPABILITY;
    auto* s = se();
    if (!s) return HOST_ERR_NOT_FOUND;

    int existing = -1, reclaimable = -1;
    scan_plugin_pool(name_s, existing, reclaimable);
    int target = (existing >= 0) ? existing : reclaimable;
    if (target < 0) return HOST_ERR_RMEM_FULL;

    return se_rc(s->rmemWriteWithHeader(static_cast<uint16_t>(target),
                                        PLG_RMEM_POOL_MODULE_ID,
                                        name_s.c_str(),
                                        /*flags=*/0,
                                        buf, static_cast<uint16_t>(len)));
}

int host_rmem_erase_named(const char* name)
{
    if (!name) return HOST_ERR_INVALID_ARG;
    std::string name_s(name);
    if (!active_declares_rmem(name_s)) return HOST_ERR_NO_CAPABILITY;
    auto* s = se();
    if (!s) return HOST_ERR_NOT_FOUND;

    int existing = -1, reclaimable = -1;
    scan_plugin_pool(name_s, existing, reclaimable);
    if (existing < 0) return HOST_ERR_NOT_FOUND;

    return se_rc(s->rmemErase(static_cast<uint16_t>(existing)));
}

bool host_rmem_name_used(const char* name)
{
    if (!name) return false;
    std::string name_s(name);
    if (!active_declares_rmem(name_s)) return false;
    int existing = -1, reclaimable = -1;
    scan_plugin_pool(name_s, existing, reclaimable);
    return existing >= 0;
}

uint16_t host_rmem_slot_size(void)
{
    auto* s = se();
    return s ? s->getRmemSlotSize() : 0;
}

int host_ecc_generate(uint8_t slot, uint8_t curve)
{
    if (!ecc_allowed(slot)) return HOST_ERR_NO_CAPABILITY;
    auto* s = se();
    if (!s) return HOST_ERR_NOT_FOUND;
    return se_rc(s->eccGenerate(slot, curve_for(curve)));
}

int host_ecc_import(uint8_t /*slot*/, const uint8_t* /*priv*/, uint8_t /*curve*/)
{
    return HOST_ERR_NOT_SUPPORTED;  // intentionally not exposed
}

int host_ecc_pubkey(uint8_t slot, uint8_t* pub, uint8_t /*curve*/)
{
    if (!pub) return HOST_ERR_INVALID_ARG;
    if (!ecc_allowed(slot)) return HOST_ERR_NO_CAPABILITY;
    auto* s = se();
    if (!s) return HOST_ERR_NOT_FOUND;
    return se_rc(s->eccGetPublicKey(slot, pub, nullptr));
}

int host_ecc_delete(uint8_t slot)
{
    if (!ecc_allowed(slot)) return HOST_ERR_NO_CAPABILITY;
    auto* s = se();
    if (!s) return HOST_ERR_NOT_FOUND;
    return se_rc(s->eccDelete(slot));
}

bool host_ecc_slot_used(uint8_t slot)
{
    if (!ecc_allowed(slot)) return false;
    auto* s = se();
    return s ? s->eccSlotUsed(slot) : false;
}

int host_ecdsa_sign(uint8_t slot, const uint8_t* msg, size_t len, uint8_t sig[64])
{
    if (!msg || !sig) return HOST_ERR_INVALID_ARG;
    if (!ecc_allowed(slot)) return HOST_ERR_NO_CAPABILITY;
    auto* s = se();
    if (!s) return HOST_ERR_NOT_FOUND;
    size_t sig_len = 64;
    return se_rc(s->ecdsaSign(slot, msg, len, sig, &sig_len));
}

int host_eddsa_sign(uint8_t slot, const uint8_t* msg, size_t len, uint8_t sig[64])
{
    if (!msg || !sig) return HOST_ERR_INVALID_ARG;
    if (!ecc_allowed(slot)) return HOST_ERR_NO_CAPABILITY;
    auto* s = se();
    if (!s) return HOST_ERR_NOT_FOUND;
    return se_rc(s->eddsaSign(slot, msg, len, sig));
}

int host_se_chip_id(uint8_t* serial, size_t* len)
{
    if (!serial || !len) return HOST_ERR_INVALID_ARG;
    auto* s = se();
    if (!s) return HOST_ERR_NOT_FOUND;
    return s->getChipId(serial, static_cast<uint8_t>(*len)) ? HOST_OK : HOST_ERR_GENERIC;
}

int host_se_fw_version(uint8_t* riscv, uint8_t* spect)
{
    if (!riscv || !spect) return HOST_ERR_INVALID_ARG;
    auto* s = se();
    if (!s) return HOST_ERR_NOT_FOUND;
    return s->getFwVersion(riscv, spect) ? HOST_OK : HOST_ERR_GENERIC;
}

}  // extern "C"
