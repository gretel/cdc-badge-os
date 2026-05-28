/**
 * \file host_api_stubs.cpp
 * \brief Stubs for host API families that are not implemented yet.
 *
 * Each stub writes one log line and returns the type-appropriate "missing"
 * sentinel: HOST_ERR_NOT_SUPPORTED for int returns, `false` for bool, `0` for
 * uint16_t (display dimensions). When Phase 3.x lands the real implementation,
 * the corresponding declarations here get deleted and a new
 * host_api_<family>.cpp owns them.
 *
 * Already implemented elsewhere:
 *   host_log_           - host_api_log.cpp
 *   host_uptime_ms      - host_api_time.cpp
 *   host_battery_*      - host_api_power.cpp
 *   host_ui_push_toast/message/info - host_api_ui.cpp
 *   host_i18n_*         - host_api_i18n.cpp
 *   host_nvs_*          - host_api_nvs.cpp
 *   host_event_*        - host_api_event.cpp
 *   host_gpio_*         - host_api_gpio.cpp
 *   host_wifi_*         - host_api_wifi.cpp
 *   host_http_*         - host_api_http.cpp
 *   host_random/sha256/hmac/aes_gcm/base32/base64/hex - host_api_crypto.cpp
 *   host_rmem_/ecc_/ecdsa_sign/eddsa_sign/se_           - host_api_se.cpp
 */

#include "plugin_manager/host_api.h"
#include "cdc_log.h"

#define STUB_TAG "PLG_API"
#define STUB_LOG(fn) LOG_W(STUB_TAG, "%s: not implemented yet", fn)
#define STUB_RET_INT(fn)  do { STUB_LOG(fn); return HOST_ERR_NOT_SUPPORTED; } while (0)
#define STUB_RET_BOOL(fn) do { STUB_LOG(fn); return false;                  } while (0)
#define STUB_RET_U16(fn)  do { STUB_LOG(fn); return 0;                      } while (0)

extern "C" {

/* UI - all push_ + pop + info + acquire + release + inactivity real in host_api_ui*.cpp */

/* Low-level GFX - opt-in via capability "display_lowlevel" */
uint16_t host_display_width    (void)                                      { STUB_RET_U16("host_display_width"); }
uint16_t host_display_height   (void)                                      { STUB_RET_U16("host_display_height"); }
int      host_display_clear    (void)                                      { STUB_RET_INT("host_display_clear"); }
int      host_display_draw_pixel(int16_t, int16_t, uint16_t)               { STUB_RET_INT("host_display_draw_pixel"); }
int      host_display_draw_line (int16_t, int16_t, int16_t, int16_t, uint16_t) { STUB_RET_INT("host_display_draw_line"); }
int      host_display_draw_rect (int16_t, int16_t, int16_t, int16_t, uint16_t) { STUB_RET_INT("host_display_draw_rect"); }
int      host_display_fill_rect (int16_t, int16_t, int16_t, int16_t, uint16_t) { STUB_RET_INT("host_display_fill_rect"); }
int      host_display_draw_text (int16_t, int16_t, const char*, uint8_t, uint16_t) { STUB_RET_INT("host_display_draw_text"); }
int      host_display_flush     (uint8_t)                                  { STUB_RET_INT("host_display_flush"); }
bool     host_display_is_busy   (void)                                     { STUB_RET_BOOL("host_display_is_busy"); }

/* BLE - read-only impls in host_api_ble.cpp; GATT-server/client still TODO */
int    host_ble_register_service(const ble_service_def_t*, uint32_t*)      { STUB_RET_INT("host_ble_register_service"); }
int    host_ble_send_notification(uint32_t, const uint8_t*, size_t)        { STUB_RET_INT("host_ble_send_notification"); }
int    host_ble_send_indication  (uint32_t, const uint8_t*, size_t)        { STUB_RET_INT("host_ble_send_indication"); }
int    host_ble_unregister_service(uint32_t)                               { STUB_RET_INT("host_ble_unregister_service"); }
int    host_ble_scan_start (void)                                          { STUB_RET_INT("host_ble_scan_start"); }
int    host_ble_scan_results(ble_scan_result_t*, size_t*)                  { STUB_RET_INT("host_ble_scan_results"); }
int    host_ble_connect    (const uint8_t*)                                { STUB_RET_INT("host_ble_connect"); }
int    host_ble_read_char  (uint32_t, const uint8_t*, uint8_t*, size_t*)   { STUB_RET_INT("host_ble_read_char"); }
int    host_ble_write_char (uint32_t, const uint8_t*, const uint8_t*, size_t) { STUB_RET_INT("host_ble_write_char"); }
int    host_ble_subscribe  (uint32_t, const uint8_t*, uint32_t)            { STUB_RET_INT("host_ble_subscribe"); }
int    host_ble_disconnect (uint32_t)                                      { STUB_RET_INT("host_ble_disconnect"); }

/* Keypad direct access - primary path is plugin_on_button */
bool host_key_pressed     (uint8_t)                                       { STUB_RET_BOOL("host_key_pressed"); }
int  host_key_consume_next(uint8_t*)                                      { STUB_RET_INT("host_key_consume_next"); }

/* USB CDC write - intentionally locked down */
int host_usb_cdc_write(const uint8_t*, size_t)                            { STUB_RET_INT("host_usb_cdc_write"); }

/* host_feature_enabled / host_get_firmware_version / host_get_build_profile
   live in host_api_sysinfo.cpp with real implementations. */

}  // extern "C"
