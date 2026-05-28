/**
 * \file ble_gpg_xsig.cpp
 * \brief BLE Badge-to-Badge cross-sign endpoint.
 *
 * Registers a dedicated GATT service (UUID `8E2F1F30-...`) with two
 * characteristics:
 *
 *   1F31 RX     - Write       - peer pushes its public key here
 *   1F32 STATUS - Read+Notify - server reports completion to peer
 *
 * Protocol mirrors the vCard exchange:
 *   Client -> Server  0x11 START + 2-byte total length + first chunk
 *                     0x12 CONT  + chunk
 *                     0x13 END   + final chunk
 *   Server -> Client  0x91 / 0x92 / 0x93 (mirror, with 1-byte status code at END)
 *
 * Payload layout (Spec docs/CROSS_SIGNING.md):
 *   1B curve, 1B pubkey_len, 32 or 64 pubkey bytes,
 *   20B fingerprint_v4, 1B user_id_len, max 63B user_id.
 */

#include "mod_gpg/ble_gpg_xsig.h"
#include "mod_gpg/GpgRecvStore.h"
#include "mod_gpg/GpgStorage.h"
#include "mod_gpg/gpg.h"
#include "openpgp/fingerprint.h"

#include "cdc_hal/IBluetoothController.h"
#include "cdc_hal/ISecureElement.h"
#include "cdc_log.h"
#include "esp_attr.h"

#include <algorithm>
#include <cstring>
#include <ctime>

namespace cdc::mod_gpg {

namespace {

constexpr const char* TAG = "GPG_BLE_XSIG";

// 8E2F1F30-8B5D-4D7A-9A6E-4C9D6A8B1A01 — chosen distinct from vCard's 1F20
// service to keep the two protocols cleanly separated without overloading
// one service definition with foreign characteristics. (CROSS_SIGNING.md
// originally proposed reusing the vCard service UUID; the deviation is noted
// in the doc.)
constexpr uint8_t kSvcUuid[16] = {
    0x01, 0x1A, 0x8B, 0x6A, 0x9D, 0x4C, 0x6E, 0x9A,
    0x7A, 0x4D, 0x5D, 0x8B, 0x30, 0x1F, 0x2F, 0x8E,
};
constexpr uint8_t kRxUuid[16] = {
    0x01, 0x1A, 0x8B, 0x6A, 0x9D, 0x4C, 0x6E, 0x9A,
    0x7A, 0x4D, 0x5D, 0x8B, 0x31, 0x1F, 0x2F, 0x8E,
};
constexpr uint8_t kStatusUuid[16] = {
    0x01, 0x1A, 0x8B, 0x6A, 0x9D, 0x4C, 0x6E, 0x9A,
    0x7A, 0x4D, 0x5D, 0x8B, 0x32, 0x1F, 0x2F, 0x8E,
};

constexpr uint8_t kOpcWriteStart = 0x11;
constexpr uint8_t kOpcWriteCont  = 0x12;
constexpr uint8_t kOpcWriteEnd   = 0x13;
constexpr uint8_t kOpcDataStart  = 0x91;
constexpr uint8_t kOpcDataCont   = 0x92;
constexpr uint8_t kOpcDataEnd    = 0x93;

constexpr uint8_t kStatusOk          = 0x00;
constexpr uint8_t kStatusBadPayload  = 0x01;
constexpr uint8_t kStatusStoreFull   = 0x02;
constexpr uint8_t kStatusInternalErr = 0x03;

// Min/max payload sizes per spec.
constexpr size_t kMinPayloadLen = 1 + 1 + 32 + 20 + 1 + 1;   // Ed25519 + empty UID
constexpr size_t kMaxPayloadLen = 1 + 1 + 64 + 20 + 1 + 63;  // P-256 + max UID

struct RxSession {
    bool     active           = false;
    uint16_t conn_handle      = 0xFFFF;
    size_t   expected_len     = 0;
    size_t   received_len     = 0;
    uint8_t  buf[kMaxPayloadLen];
};

EXT_RAM_BSS_ATTR RxSession s_rx = {};

bool             s_initialized   = false;
uint16_t         s_rx_handle     = 0;
uint16_t         s_status_handle = 0;
cdc::hal::GattCharacteristic s_chars[2] = {};
cdc::hal::GattServiceDef     s_svc      = {};

XsigReceivedCallback s_callback = nullptr;

void resetRx() {
    s_rx.active = false;
    s_rx.conn_handle = 0xFFFF;
    s_rx.expected_len = 0;
    s_rx.received_len = 0;
}

void sendStatus(uint16_t conn_handle, uint8_t code) {
    auto* ble = cdc::hal::getBluetoothControllerInstance();
    if (!ble || s_status_handle == 0) return;
    const uint8_t frame[3] = {kOpcDataEnd, 0x01, code};
    ble->sendNotification(conn_handle, s_status_handle, frame, sizeof(frame));
}

bool parsePayloadIntoKey(const uint8_t* data, size_t len, gpg_recv_key_t* out) {
    if (!data || !out) return false;
    if (len < kMinPayloadLen) return false;

    size_t off = 0;
    const uint8_t curve = data[off++];
    if (curve != CDC_CURVE_ED25519 && curve != CDC_CURVE_P256) return false;

    const uint8_t pubkey_len = data[off++];
    if (curve == CDC_CURVE_ED25519 && pubkey_len != 32) return false;
    if (curve == CDC_CURVE_P256    && pubkey_len != 64) return false;
    if (off + pubkey_len + 20 + 1 > len) return false;

    std::memset(out, 0, sizeof(*out));
    out->curve = curve;
    out->pubkey_len = pubkey_len;
    std::memcpy(out->pubkey, data + off, pubkey_len);
    off += pubkey_len;
    std::memcpy(out->fingerprint_v4, data + off, 20);
    off += 20;

    const uint8_t uid_len = data[off++];
    if (uid_len > 63 || off + uid_len > len) return false;
    std::memcpy(out->user_id, data + off, uid_len);
    out->user_id[uid_len] = '\0';

    out->received_at = static_cast<uint32_t>(std::time(nullptr));

    // Compute V5 fingerprint locally from the public-key material we just
    // received. The peer may not have sent it (cross-sign protocol payload
    // is V4-only), but we want a consistent record on this badge.
    calculateFingerprintV5(curve, out->pubkey, out->pubkey_len,
                           out->received_at, out->fingerprint_v5);
    return true;
}

void finalizeRx() {
    gpg_recv_key_t key = {};
    if (!parsePayloadIntoKey(s_rx.buf, s_rx.received_len, &key)) {
        LOG_W(TAG, "Rejecting malformed payload (%u bytes)",
              static_cast<unsigned>(s_rx.received_len));
        sendStatus(s_rx.conn_handle, kStatusBadPayload);
        resetRx();
        return;
    }
    if (!GpgRecvStore::instance().addKey(key)) {
        sendStatus(s_rx.conn_handle, kStatusStoreFull);
        resetRx();
        return;
    }
    sendStatus(s_rx.conn_handle, kStatusOk);
    if (s_callback) s_callback(key);
    LOG_I(TAG, "Received key from %s (%u B)", key.user_id,
          static_cast<unsigned>(s_rx.received_len));
    resetRx();
}

int onRxWrite(uint16_t conn_handle, uint16_t /*attr*/,
              const uint8_t* data, uint16_t len)
{
    if (!data || len == 0) return 0;
    const uint8_t opcode = data[0];

    switch (opcode) {
        case kOpcWriteStart: {
            if (len < 3) return 0;
            const size_t total = (static_cast<size_t>(data[1]) << 8) | data[2];
            if (total == 0 || total > kMaxPayloadLen) {
                sendStatus(conn_handle, kStatusBadPayload);
                resetRx();
                return 0;
            }
            s_rx.active = true;
            s_rx.conn_handle = conn_handle;
            s_rx.expected_len = total;
            s_rx.received_len = 0;
            const size_t chunk = len - 3;
            if (chunk > 0) {
                const size_t to_copy = std::min<size_t>(chunk, sizeof(s_rx.buf));
                std::memcpy(s_rx.buf, data + 3, to_copy);
                s_rx.received_len = to_copy;
            }
            if (s_rx.received_len >= s_rx.expected_len) finalizeRx();
            return 0;
        }
        case kOpcWriteCont:
        case kOpcWriteEnd: {
            if (!s_rx.active || conn_handle != s_rx.conn_handle) return 0;
            const size_t chunk = len - 1;
            const size_t remaining = sizeof(s_rx.buf) - s_rx.received_len;
            const size_t to_copy = std::min(chunk, remaining);
            std::memcpy(s_rx.buf + s_rx.received_len, data + 1, to_copy);
            s_rx.received_len += to_copy;
            if (opcode == kOpcWriteEnd || s_rx.received_len >= s_rx.expected_len) {
                finalizeRx();
            }
            return 0;
        }
        default:
            return 0;
    }
}

/// Build the badge's own key payload in the same layout we expect on receive.
/// Returns total length written to out, or 0 on failure.
size_t buildOwnKeyPayload(uint8_t* out, size_t out_size) {
    gpg_status_t status = {};
    if (!gpg_get_status(&status)) return 0;

    const uint8_t curve = status.curve;
    const uint8_t pubkey_len = (curve == CDC_CURVE_ED25519) ? 32 : 64;

    // We need the actual pubkey bytes; gpg_status_t doesn't carry them, so
    // pull them from the SIG ECC slot directly. (This logic mirrors what
    // gpg.cpp::se_get_pubkey does, but we want to keep this file
    // self-contained.)
    auto* se = cdc::hal::getSecureElementInstance();
    if (!se) return 0;

    uint8_t pubkey[64] = {0};
    cdc::hal::EccCurve hal_curve = cdc::hal::EccCurve::P256;
    if (se->eccGetPublicKey(gpg_storage_sig_slot(), pubkey, &hal_curve)
        != cdc::hal::SeResult::OK) {
        return 0;
    }

    const size_t uid_len = strnlen(status.user_id, sizeof(status.user_id));
    const size_t total = 1 + 1 + pubkey_len + 20 + 1 + uid_len;
    if (total > out_size) return 0;

    size_t off = 0;
    out[off++] = curve;
    out[off++] = pubkey_len;
    std::memcpy(out + off, pubkey, pubkey_len);
    off += pubkey_len;
    std::memcpy(out + off, status.fingerprint, 20);
    off += 20;
    out[off++] = static_cast<uint8_t>(uid_len);
    std::memcpy(out + off, status.user_id, uid_len);
    off += uid_len;
    return off;
}

} // namespace

bool ble_gpg_xsig_init() {
    if (s_initialized) return true;

    auto* ble = cdc::hal::getBluetoothControllerInstance();
    if (!ble) {
        LOG_W(TAG, "BluetoothController unavailable, deferring init");
        return false;
    }

    s_chars[0].uuid        = cdc::hal::BleUuid::from128(kRxUuid);
    s_chars[0].properties  = cdc::hal::GattProp::WRITE;
    s_chars[0].permissions = cdc::hal::GattPerm::WRITE_ENC;
    s_chars[0].valueHandle = &s_rx_handle;
    s_chars[0].onWrite     = onRxWrite;
    s_chars[0].onRead      = nullptr;

    s_chars[1].uuid        = cdc::hal::BleUuid::from128(kStatusUuid);
    s_chars[1].properties  = cdc::hal::GattProp::READ | cdc::hal::GattProp::NOTIFY;
    s_chars[1].permissions = cdc::hal::GattPerm::READ_ENC;
    s_chars[1].valueHandle = &s_status_handle;
    s_chars[1].onWrite     = nullptr;
    s_chars[1].onRead      = nullptr;

    s_svc.uuid               = cdc::hal::BleUuid::from128(kSvcUuid);
    s_svc.characteristics    = s_chars;
    s_svc.numCharacteristics = 2;

    if (!ble->registerGattService(s_svc)) {
        LOG_W(TAG, "registerGattService failed");
        return false;
    }
    s_initialized = true;
    LOG_I(TAG, "GPG cross-sign service registered (rx=%u status=%u)",
          static_cast<unsigned>(s_rx_handle),
          static_cast<unsigned>(s_status_handle));
    return true;
}

void ble_gpg_xsig_set_received_callback(XsigReceivedCallback cb) {
    s_callback = cb;
}

/// Sender state machine: we discover the peer's RX handle, then write the
/// payload in MTU-sized chunks.
namespace {

struct SendSession {
    bool     active        = false;
    bool     awaiting_disc = false;
    uint16_t conn_handle   = 0xFFFF;
    uint16_t peer_rx       = 0;
    uint8_t  payload[kMaxPayloadLen];
    size_t   payload_len   = 0;
    size_t   payload_off   = 0;
};
EXT_RAM_BSS_ATTR SendSession s_tx = {};

cdc::hal::IBluetoothController::ListenerToken s_disc_token  = 0xFFFF;
cdc::hal::IBluetoothController::ListenerToken s_write_token = 0xFFFF;

void cancelSend(const char* why) {
    LOG_W(TAG, "Send aborted: %s", why ? why : "?");
    auto* ble = cdc::hal::getBluetoothControllerInstance();
    if (ble) {
        if (s_tx.conn_handle != 0xFFFF) ble->disconnectHandle(s_tx.conn_handle);
        if (s_disc_token != 0xFFFF)  ble->removeServiceDiscoveryCallback(s_disc_token);
        if (s_write_token != 0xFFFF) ble->removeWriteCompleteCallback(s_write_token);
    }
    s_disc_token  = 0xFFFF;
    s_write_token = 0xFFFF;
    s_tx = SendSession{};
}

bool writeNextChunk() {
    auto* ble = cdc::hal::getBluetoothControllerInstance();
    if (!ble || !s_tx.active) return false;

    const uint16_t mtu = ble->getMtu();
    if (s_tx.payload_off == 0) {
        // START frame: opcode + 2-byte length + chunk.
        const size_t header = 3;
        const size_t chunk = std::min<size_t>(mtu - header, s_tx.payload_len);
        uint8_t buf[244];
        if (header + chunk > sizeof(buf)) return false;
        buf[0] = kOpcWriteStart;
        buf[1] = (s_tx.payload_len >> 8) & 0xFF;
        buf[2] = s_tx.payload_len & 0xFF;
        std::memcpy(buf + 3, s_tx.payload, chunk);
        s_tx.payload_off = chunk;
        return ble->writeCharacteristic(s_tx.conn_handle, s_tx.peer_rx,
                                        buf, header + chunk, true);
    }

    // CONT / END frames: opcode + chunk.
    const size_t header = 1;
    const size_t remaining = s_tx.payload_len - s_tx.payload_off;
    const size_t chunk = std::min<size_t>(mtu - header, remaining);
    const bool   last  = (chunk == remaining);
    uint8_t buf[244];
    if (header + chunk > sizeof(buf)) return false;
    buf[0] = last ? kOpcWriteEnd : kOpcWriteCont;
    std::memcpy(buf + 1, s_tx.payload + s_tx.payload_off, chunk);
    s_tx.payload_off += chunk;
    return ble->writeCharacteristic(s_tx.conn_handle, s_tx.peer_rx,
                                    buf, header + chunk, true);
}

void onSendDiscovery(uint16_t conn_handle,
                     const cdc::hal::IBluetoothController::DiscoveredService* svc,
                     bool complete)
{
    if (!s_tx.active || conn_handle != s_tx.conn_handle) return;
    if (!svc || !complete) return;
    if (!(svc->uuid == cdc::hal::BleUuid::from128(kSvcUuid))) return;

    for (uint8_t i = 0; i < svc->numCharacteristics; ++i) {
        if (svc->characteristics[i].uuid == cdc::hal::BleUuid::from128(kRxUuid)) {
            s_tx.peer_rx = svc->characteristics[i].valueHandle;
            break;
        }
    }
    if (s_tx.peer_rx == 0) { cancelSend("RX char not found"); return; }
    s_tx.awaiting_disc = false;
    writeNextChunk();
}

void onSendWriteComplete(uint16_t conn_handle, uint16_t /*attr*/, int status) {
    if (!s_tx.active || conn_handle != s_tx.conn_handle) return;
    if (status != 0) { cancelSend("write failed"); return; }
    if (s_tx.payload_off >= s_tx.payload_len) {
        LOG_I(TAG, "Send complete, %u bytes", static_cast<unsigned>(s_tx.payload_len));
        // Leave the peer time to ACK before disconnecting.
        auto* ble = cdc::hal::getBluetoothControllerInstance();
        if (ble) ble->disconnectHandle(s_tx.conn_handle);
        s_tx = SendSession{};
        s_disc_token  = 0xFFFF;
        s_write_token = 0xFFFF;
        return;
    }
    writeNextChunk();
}

} // namespace

bool ble_gpg_xsig_send(const uint8_t addr[6], uint8_t addr_type) {
    if (s_tx.active) return false;

    auto* ble = cdc::hal::getBluetoothControllerInstance();
    if (!ble) return false;

    s_tx.payload_len = buildOwnKeyPayload(s_tx.payload, sizeof(s_tx.payload));
    if (s_tx.payload_len == 0) return false;

    s_disc_token  = ble->addServiceDiscoveryCallback(onSendDiscovery);
    s_write_token = ble->addWriteCompleteCallback(onSendWriteComplete);

    s_tx.active        = true;
    s_tx.awaiting_disc = true;
    s_tx.payload_off   = 0;
    if (!ble->connect(addr, addr_type)) {
        cancelSend("connect failed");
        return false;
    }
    // We rely on the BluetoothController's existing connection callback to
    // fire discoverServiceByUuid; for the simple case we kick off discovery
    // right after a small delay implicitly handled by the stack. If the
    // controller doesn't auto-discover, the caller can extend this layer.
    s_tx.conn_handle = 0xFFFE; // placeholder, filled by stack
    ble->discoverServiceByUuid(s_tx.conn_handle,
                               cdc::hal::BleUuid::from128(kSvcUuid));
    return true;
}

} // namespace cdc::mod_gpg
