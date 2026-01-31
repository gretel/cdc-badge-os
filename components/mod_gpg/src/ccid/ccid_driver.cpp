#include "mod_gpg/openpgp/ccid.h"
#include "mod_gpg/openpgp/openpgp.h"
#include "cdc_log.h"

extern "C" {
#include "tusb.h"
#include "device/usbd_pvt.h"
}

#include <string.h>
#include <stdio.h>

static const char* TAG = "CCID";

#define CCID_LOG(tag, fmt, ...) LOG_I(tag, fmt, ##__VA_ARGS__)
#define CCID_LOG_E(tag, fmt, ...) LOG_E(tag, fmt, ##__VA_ARGS__)
#define CCID_LOG_W(tag, fmt, ...) LOG_W(tag, fmt, ##__VA_ARGS__)

static struct {
    bool initialized;
    uint8_t itf_num;
    uint8_t ep_in;
    uint8_t ep_out;
    uint8_t rhport;

    uint8_t rx_buf[512];
    uint8_t tx_buf[512];
    uint16_t rx_len;
    bool rx_pending;

    uint32_t rx_count;
    uint32_t tx_count;
    uint32_t error_count;
} ccid_state;

static void ccid_driver_init(void) {
    CCID_LOG(TAG, "CCID driver init");
    memset(&ccid_state, 0, sizeof(ccid_state));
}

static void ccid_driver_reset(uint8_t rhport) {
    (void)rhport;
    ccid_state.rx_len = 0;
    ccid_state.rx_pending = false;
    ccid_state.initialized = false;
    ccid_state.rx_count = 0;
    ccid_state.tx_count = 0;
    ccid_state.error_count = 0;
}

static uint16_t ccid_driver_open(uint8_t rhport, tusb_desc_interface_t const* desc_itf, uint16_t max_len) {
    if (desc_itf->bInterfaceClass != TUSB_CLASS_SMART_CARD) {
        return 0;
    }

    ccid_state.itf_num = desc_itf->bInterfaceNumber;
    ccid_state.rhport = rhport;

    uint16_t drv_len = sizeof(tusb_desc_interface_t);
    uint8_t const* p_desc = (uint8_t const*)desc_itf + drv_len;

    while (drv_len < max_len) {
        uint8_t desc_len = p_desc[0];
        uint8_t desc_type = p_desc[1];
        if (desc_type == 0x21) {
            drv_len += desc_len;
            p_desc += desc_len;
        } else {
            break;
        }
    }

    uint8_t ep_count = 0;
    while (drv_len < max_len && ep_count < desc_itf->bNumEndpoints) {
        uint8_t desc_len = p_desc[0];
        uint8_t desc_type = p_desc[1];

        if (desc_type == TUSB_DESC_ENDPOINT) {
            tusb_desc_endpoint_t const* ep_desc = (tusb_desc_endpoint_t const*)p_desc;
            uint8_t ep_addr = ep_desc->bEndpointAddress;

            if (usbd_edpt_open(rhport, ep_desc)) {
                if (tu_edpt_dir(ep_addr) == TUSB_DIR_IN) {
                    ccid_state.ep_in = ep_addr;
                } else {
                    ccid_state.ep_out = ep_addr;
                }
                ep_count++;
            }
        }
        drv_len += desc_len;
        p_desc += desc_len;
    }

    if (ccid_state.ep_out) {
        bool ok = usbd_edpt_xfer(rhport, ccid_state.ep_out, ccid_state.rx_buf, sizeof(ccid_state.rx_buf));
        ccid_state.rx_pending = ok;
    }

    ccid_state.initialized = true;
    return drv_len;
}

static bool ccid_driver_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
    if (stage != CONTROL_STAGE_SETUP) return true;

    if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS &&
        request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE &&
        request->wIndex == ccid_state.itf_num) {

        switch (request->bRequest) {
            case 0x01:  // CCID_ABORT
            case 0x02:  // CCID_GET_CLOCK_FREQUENCIES
            case 0x03:  // CCID_GET_DATA_RATES
                return tud_control_status(rhport, request);
            default:
                return false;
        }
    }

    return false;
}

static bool ccid_driver_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
    if (result != XFER_RESULT_SUCCESS) {
        ccid_state.error_count++;
        return true;
    }

    if (ep_addr == ccid_state.ep_out) {
        ccid_state.rx_count++;
        ccid_state.rx_len = xferred_bytes;
        ccid_state.rx_pending = false;

        if (xferred_bytes >= 10) {
            uint32_t data_len = ccid_state.rx_buf[1] | (ccid_state.rx_buf[2] << 8) |
                               (ccid_state.rx_buf[3] << 16) | (ccid_state.rx_buf[4] << 24);
            uint32_t total_len = 10 + data_len;

            if (ccid_state.rx_len >= total_len) {
                int resp_len = ccid_process_message(ccid_state.rx_buf, total_len,
                                                    ccid_state.tx_buf, sizeof(ccid_state.tx_buf));
                if (resp_len > 0) {
                    ccid_state.tx_count++;
                    bool ok = usbd_edpt_xfer(rhport, ccid_state.ep_in, ccid_state.tx_buf, resp_len);
                    if (!ok) {
                        ccid_state.error_count++;
                    }
                } else if (resp_len < 0) {
                    ccid_state.error_count++;
                }
            }
        }

        bool rx_ok = usbd_edpt_xfer(rhport, ccid_state.ep_out, ccid_state.rx_buf, sizeof(ccid_state.rx_buf));
        if (!rx_ok) {
            ccid_state.error_count++;
        }
        ccid_state.rx_pending = rx_ok;
        return true;
    }

    if (ep_addr == ccid_state.ep_in) {
        return true;
    }

    return true;
}

static usbd_class_driver_t const ccid_driver = {
    .name             = "CCID",
    .init             = ccid_driver_init,
    .deinit           = NULL,
    .reset            = ccid_driver_reset,
    .open             = ccid_driver_open,
    .control_xfer_cb  = ccid_driver_control_xfer_cb,
    .xfer_cb          = ccid_driver_xfer_cb,
    .xfer_isr         = NULL,
    .sof              = NULL
};

extern "C" usbd_class_driver_t const* usbd_app_driver_get_cb(uint8_t* driver_count) {
    *driver_count = 1;
    return &ccid_driver;
}
