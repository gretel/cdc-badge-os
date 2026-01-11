#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "feature_flags.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_BADGE_NAME_MAX 32
#define BLE_BADGE_SLOGAN_MAX 32

typedef struct {
    char name[BLE_BADGE_NAME_MAX];
    char slogan[BLE_BADGE_SLOGAN_MAX];
    int8_t rssi;
    uint8_t addr[6];
    bool exchange_ready;
} ble_badge_peer_t;

#if FEATURE_BLE_BADGE
typedef enum {
    BLE_BADGE_PAIR_NONE = 0,
    BLE_BADGE_PAIR_NUMERIC,
    BLE_BADGE_PAIR_PASSKEY_DISPLAY,
    BLE_BADGE_PAIR_PASSKEY_INPUT,
} ble_badge_pair_event_t;
#endif

#if FEATURE_BLE_BADGE
bool ble_badge_init(void);
void ble_badge_deinit(void);

void ble_badge_set_adv_enabled(bool enabled);
bool ble_badge_is_adv_enabled(void);
bool ble_badge_is_adv_active(void);

void ble_badge_set_scan_enabled(bool enabled);
bool ble_badge_is_scan_enabled(void);
bool ble_badge_is_scan_active(void);

void ble_badge_set_receive_enabled(bool enabled);
bool ble_badge_is_receive_enabled(void);

void ble_badge_set_exchange_enabled(bool enabled);
bool ble_badge_is_exchange_enabled(void);

void ble_badge_set_scan_interval(uint32_t scan_ms, uint32_t pause_ms);
void ble_badge_set_adv_interval(uint32_t interval_ms);
void ble_badge_set_rssi_threshold(int8_t rssi);
uint32_t ble_badge_get_adv_interval(void);
uint32_t ble_badge_get_scan_interval(void);

uint16_t ble_badge_get_peers(ble_badge_peer_t *out, uint16_t max_peers);

bool ble_badge_exchange_with(const uint8_t addr[6]);
bool ble_badge_exchange_cancel(void);
bool ble_badge_poll_exchange_result(bool *success);
bool ble_badge_exchange_in_progress(void);

bool ble_badge_poll_nearby(ble_badge_peer_t *out);
bool ble_badge_poll_pairing_event(ble_badge_pair_event_t *type, uint32_t *passkey);
void ble_badge_confirm_pairing(bool accept);
void ble_badge_reply_passkey(bool accept, uint32_t passkey);
#else
static inline bool ble_badge_init(void) { return false; }
static inline void ble_badge_deinit(void) {}
static inline void ble_badge_set_adv_enabled(bool enabled) { (void)enabled; }
static inline bool ble_badge_is_adv_enabled(void) { return false; }
static inline bool ble_badge_is_adv_active(void) { return false; }
static inline void ble_badge_set_scan_enabled(bool enabled) { (void)enabled; }
static inline bool ble_badge_is_scan_enabled(void) { return false; }
static inline bool ble_badge_is_scan_active(void) { return false; }
static inline void ble_badge_set_receive_enabled(bool enabled) { (void)enabled; }
static inline bool ble_badge_is_receive_enabled(void) { return false; }
static inline void ble_badge_set_exchange_enabled(bool enabled) { (void)enabled; }
static inline bool ble_badge_is_exchange_enabled(void) { return false; }
static inline void ble_badge_set_scan_interval(uint32_t scan_ms, uint32_t pause_ms) {
    (void)scan_ms; (void)pause_ms;
}
static inline void ble_badge_set_adv_interval(uint32_t interval_ms) { (void)interval_ms; }
static inline void ble_badge_set_rssi_threshold(int8_t rssi) { (void)rssi; }
static inline uint32_t ble_badge_get_adv_interval(void) { return 0; }
static inline uint32_t ble_badge_get_scan_interval(void) { return 0; }
static inline uint16_t ble_badge_get_peers(ble_badge_peer_t *out, uint16_t max_peers) {
    (void)out; (void)max_peers; return 0;
}
static inline bool ble_badge_exchange_with(const uint8_t addr[6]) { (void)addr; return false; }
static inline bool ble_badge_exchange_cancel(void) { return false; }
static inline bool ble_badge_poll_exchange_result(bool *success) { (void)success; return false; }
static inline bool ble_badge_exchange_in_progress(void) { return false; }
static inline bool ble_badge_poll_nearby(ble_badge_peer_t *out) { (void)out; return false; }
static inline bool ble_badge_poll_pairing_event(ble_badge_pair_event_t *type, uint32_t *passkey) {
    (void)type; (void)passkey; return false;
}
static inline void ble_badge_confirm_pairing(bool accept) { (void)accept; }
static inline void ble_badge_reply_passkey(bool accept, uint32_t passkey) { (void)accept; (void)passkey; }
#endif

#ifdef __cplusplus
}
#endif
