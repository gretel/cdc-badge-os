#pragma once

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

#define VCARD_BLE_NAME_MAX   32
#define VCARD_BLE_SLOGAN_MAX 32

#ifdef __DOXYGEN__
namespace cdc::mod_vcard {
#endif

typedef struct {
    char name[VCARD_BLE_NAME_MAX];
    char slogan[VCARD_BLE_SLOGAN_MAX];
    int8_t rssi;
    uint8_t addr[6];
    uint8_t addr_type;
    bool exchange_ready;
} vcard_peer_t;

#ifdef __DOXYGEN__
} // namespace cdc::mod_vcard
#endif

// Exchange state machine
typedef enum {
    VCARD_EXCHANGE_IDLE = 0,
    VCARD_EXCHANGE_CONNECTING,
    VCARD_EXCHANGE_DISCOVERING,
    VCARD_EXCHANGE_AWAITING_REMOTE_CONSENT,
    VCARD_EXCHANGE_AWAITING_LOCAL_CONSENT,
    VCARD_EXCHANGE_READING_PEER_VCARD,
    VCARD_EXCHANGE_WRITING_OWN_VCARD,
    VCARD_EXCHANGE_COMPLETE,
    VCARD_EXCHANGE_ERROR,
    VCARD_EXCHANGE_CANCELLED,
    VCARD_EXCHANGE_DECLINED,
} vcard_exchange_state_t;

bool ble_vcard_init(void);
void ble_vcard_deinit(void);

void ble_vcard_set_adv_enabled(bool enabled);
bool ble_vcard_is_adv_enabled(void);
bool ble_vcard_is_adv_active(void);

void ble_vcard_set_scan_enabled(bool enabled);
bool ble_vcard_is_scan_enabled(void);
bool ble_vcard_is_scan_active(void);

void ble_vcard_set_receive_enabled(bool enabled);
bool ble_vcard_is_receive_enabled(void);

void ble_vcard_set_exchange_enabled(bool enabled);
bool ble_vcard_is_exchange_enabled(void);

void ble_vcard_set_scan_interval(uint32_t scan_ms, uint32_t pause_ms);
void ble_vcard_set_adv_interval(uint32_t interval_ms);
void ble_vcard_set_rssi_threshold(int8_t rssi);
uint32_t ble_vcard_get_adv_interval(void);
uint32_t ble_vcard_get_scan_interval(void);

uint16_t ble_vcard_get_peers(vcard_peer_t* out, uint16_t max_peers);

// Start exchange with peer (as initiator/client)
bool ble_vcard_exchange_with(const uint8_t addr[6], uint8_t addr_type);
bool ble_vcard_exchange_cancel(void);
bool ble_vcard_poll_exchange_result(bool* success);
bool ble_vcard_exchange_in_progress(void);

// Get current exchange state
vcard_exchange_state_t ble_vcard_get_exchange_state(void);
const char* ble_vcard_get_exchange_error(void);

// Consent handling (for server/responder side)
bool ble_vcard_has_pending_consent(void);
bool ble_vcard_get_consent_peer_name(char* out, size_t max_len);
void ble_vcard_respond_consent(bool accepted);

// Callbacks (must be set before exchange)
typedef void (*vcard_consent_callback_t)(const char* peer_name);
typedef void (*vcard_exchange_complete_callback_t)(bool success, const char* error);

void ble_vcard_set_consent_callback(vcard_consent_callback_t cb);
void ble_vcard_set_exchange_complete_callback(vcard_exchange_complete_callback_t cb);

// Tick function for timeout handling (call periodically from UI task)
void ble_vcard_tick(uint32_t now_ms);

bool ble_vcard_poll_nearby(vcard_peer_t* out);

#ifdef __cplusplus
}
#endif
