#include "mod_fido2/Fido2Module.h"
#include "cdc_core/ModuleRegistry.h"
#include "cdc_core/UsbManager.h"
#include "cdc_log.h"
#include "mod_fido2/Fido2Ui.h"
#include "mod_fido2/fido2.h"
#include "mod_fido2/fido2_storage.h"
#include "mod_fido2/ctaphid.h"
#include "usb_badge/usb_hid.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <string.h>

static const char* TAG = "FIDO2";

namespace cdc::mod_fido2 {

// FIDO U2F HID Report Descriptor (CTAPHID standard)
// See FIDO U2F HID Protocol Specification
static const uint8_t s_fido_report_desc[] = {
    0x06, 0xD0, 0xF1,  // Usage Page (FIDO Alliance)
    0x09, 0x01,        // Usage (U2F HID Authenticator Device)
    0xA1, 0x01,        // Collection (Application)
    0x09, 0x20,        //   Usage (Input Report Data)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, CTAPHID_PACKET_SIZE,  //   Report Count (64)
    0x81, 0x02,        //   Input (Data, Variable, Absolute)
    0x09, 0x21,        //   Usage (Output Report Data)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, CTAPHID_PACKET_SIZE,  //   Report Count (64)
    0x91, 0x02,        //   Output (Data, Variable, Absolute)
    0xC0               // End Collection
};

// Internal packet queue for incoming HID reports
static constexpr size_t FIDO_QUEUE_SIZE = 8;
static QueueHandle_t s_rx_queue = nullptr;

struct FidoPacket {
    uint8_t data[CTAPHID_PACKET_SIZE];
};

// HID instance index (set after registration)
static uint8_t s_hid_instance = 0;

// Callbacks for USB HID
static uint16_t onFidoGetReport(uint8_t report_id, uint8_t report_type,
                                 uint8_t* buffer, uint16_t reqlen) {
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;  // FIDO doesn't use GET_REPORT
}

static void onFidoSetReport(uint8_t report_id, uint8_t report_type,
                            uint8_t const* buffer, uint16_t bufsize) {
    (void)report_id;
    (void)report_type;

    if (!s_rx_queue || !buffer || bufsize != CTAPHID_PACKET_SIZE) {
        return;
    }

    FidoPacket pkt;
    memcpy(pkt.data, buffer, CTAPHID_PACKET_SIZE);

    // TinyUSB callbacks run in task context, not ISR
    if (xQueueSend(s_rx_queue, &pkt, 0) != pdTRUE) {
        LOG_W(TAG, "RX queue full, dropping packet");
    }
}

static void onFidoReportComplete(uint8_t const* report, uint16_t len) {
    (void)report;
    (void)len;
}

// Module implementation
Fido2Module& Fido2Module::instance() {
    static Fido2Module inst;
    return inst;
}

bool Fido2Module::init() {
    LOG_I(TAG, "Initializing FIDO2 module");

    // Create RX queue
    if (!s_rx_queue) {
        s_rx_queue = xQueueCreate(FIDO_QUEUE_SIZE, sizeof(FidoPacket));
        if (!s_rx_queue) {
            LOG_E(TAG, "Failed to create RX queue");
            return false;
        }
    }

    fido2_ui_init();
    core::ModuleRegistry::instance().registerModule(this);

    if (slotRange_.hasEcc && slotRange_.hasRmem) {
        uint16_t eccCount = static_cast<uint16_t>(slotRange_.eccEnd - slotRange_.eccStart + 1);
        uint16_t rmemCount = static_cast<uint16_t>(slotRange_.rmemEnd - slotRange_.rmemStart + 1);
        if (rmemCount < eccCount) {
            core::ModuleRegistry::instance().reportModuleError(
                getName(), "FIDO2 R-MEM range smaller than ECC range");
            state_ = core::ServiceState::ERROR;
            return false;
        }
        fido2_storage_set_slot_range(slotRange_.eccStart, slotRange_.eccEnd,
                                     slotRange_.rmemStart, slotRange_.rmemEnd);
        core::ModuleRegistry::instance().clearModuleErrorByName(getName());
    } else {
        core::ModuleRegistry::instance().reportModuleError(getName(), "FIDO2 slot range missing");
        state_ = core::ServiceState::ERROR;
        return false;
    }

    state_ = core::ServiceState::INITIALIZED;
    return true;
}

bool Fido2Module::start() {
    if (state_ != core::ServiceState::INITIALIZED &&
        state_ != core::ServiceState::STOPPED) {
        return false;
    }

    core::UsbInterfaceSpec spec;
    spec.cls = core::UsbInterfaceClass::Hid;
    spec.name = "FIDO2";
    spec.reportDesc = s_fido_report_desc;
    spec.reportDescLen = sizeof(s_fido_report_desc);
    spec.protocol = 0;  // HID_ITF_PROTOCOL_NONE
    spec.hasOut = true;
    spec.epInSize = CTAPHID_PACKET_SIZE;
    spec.epOutSize = CTAPHID_PACKET_SIZE;
    spec.callbacks.onGetReport = onFidoGetReport;
    spec.callbacks.onSetReport = onFidoSetReport;
    spec.callbacks.onReportComplete = onFidoReportComplete;

    if (!core::UsbManager::instance().registerInterface(core::UsbHidInterface::Fido, getName(), spec)) {
        LOG_W(TAG, "Failed to register FIDO HID interface");
        return false;
    }

    // FIDO is the first HID interface registered, so instance = 0
    s_hid_instance = 0;

    if (!fido2_is_initialized()) {
        if (!fido2_init()) {
            core::ModuleRegistry::instance().reportModuleError(getName(), "FIDO2 init failed");
            core::UsbManager::instance().unregisterInterface(core::UsbHidInterface::Fido, getName());
            state_ = core::ServiceState::ERROR;
            return false;
        }
    }
    fido2_set_user_presence_callback(fido2_ui_user_presence_callback);

    state_ = core::ServiceState::STARTED;
    return true;
}

void Fido2Module::stop() {
    core::UsbManager::instance().unregisterInterface(core::UsbHidInterface::Fido, getName());
    state_ = core::ServiceState::STOPPED;
}

void Fido2Module::setSlotRange(const core::IModule::SlotRange& range) {
    slotRange_ = range;
}

core::IModule::SlotRequest Fido2Module::getSlotRequest() const {
    core::IModule::SlotRequest req = {};
    req.mapName = getName();
    req.minEccSlots = 1;
    req.minRmemSlots = 1;
    return req;
}

uint8_t Fido2Module::getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;

    items[0] = {fido2_ui_get_label(), 50, []() -> ui::IView* {
        return fido2_ui_get_list_view();
    }, nullptr, getName(), core::MenuLocation::MAIN_MENU};

    return 1;
}

// USB transport functions for fido2.cpp
bool fido2_usb_available() {
    if (!s_rx_queue) return false;
    return uxQueueMessagesWaiting(s_rx_queue) > 0;
}

bool fido2_usb_ready() {
    return usb_hid_instance_ready(s_hid_instance);
}

uint16_t fido2_usb_read(uint8_t* buffer) {
    if (!s_rx_queue || !buffer) return 0;

    FidoPacket pkt;
    if (xQueueReceive(s_rx_queue, &pkt, 0) == pdTRUE) {
        memcpy(buffer, pkt.data, CTAPHID_PACKET_SIZE);
        return CTAPHID_PACKET_SIZE;
    }
    return 0;
}

bool fido2_usb_write(const uint8_t* buffer) {
    if (!buffer) return false;
    return usb_hid_send_report(s_hid_instance, 0, buffer, CTAPHID_PACKET_SIZE);
}

} // namespace cdc::mod_fido2

extern "C" void mod_fido2_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& module = cdc::mod_fido2::Fido2Module::instance();
        if (module.init()) {
            module.start();
        }
    });
}
