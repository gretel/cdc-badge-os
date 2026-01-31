#pragma once

#include "cdc_core/IService.h"
#include <cstdint>

namespace cdc::core {

enum class UsbHidInterface : uint8_t {
    Fido = 0,
    Keyboard = 1,
    Ccid = 2,
};

enum class UsbInterfaceClass : uint8_t {
    Hid = 0,
    Ccid = 1,
};

struct UsbHidCallbacks {
    uint16_t (*onGetReport)(uint8_t report_id, uint8_t report_type,
                            uint8_t* buffer, uint16_t reqlen) = nullptr;
    void (*onSetReport)(uint8_t report_id, uint8_t report_type,
                        uint8_t const* buffer, uint16_t bufsize) = nullptr;
    void (*onReportComplete)(uint8_t const* report, uint16_t len) = nullptr;
};

struct UsbInterfaceSpec {
    UsbInterfaceClass cls = UsbInterfaceClass::Hid;
    const char* name = nullptr;  // Interface name for USB descriptor
    const uint8_t* reportDesc = nullptr;
    uint16_t reportDescLen = 0;
    uint8_t protocol = 0;  // HID protocol (0=none, 1=keyboard)
    bool hasOut = false;
    uint16_t epInSize = 64;
    uint16_t epOutSize = 64;
    UsbHidCallbacks callbacks = {};
};

/**
 * UsbManager - HID interface arbitration
 *
 * CDC is always enabled (handled by usb_badge). HID interfaces are optional and
 * registered by modules at runtime.
 */
class UsbManager : public IService {
public:
    static UsbManager& instance();

    const char* getName() const override { return "UsbManager"; }

    bool init() override;
    bool start() override;
    void stop() override;
    ServiceState getState() const override { return state_; }

    bool registerInterface(UsbHidInterface type, const char* moduleName,
                           const UsbInterfaceSpec& def);
    void unregisterInterface(UsbHidInterface type, const char* moduleName);

    uint8_t activeInterfaceMask() const { return activeMask_; }
    bool applyConfiguration();
    bool needsReplug() const { return needsReplug_; }

private:
    struct InterfaceEntry {
        bool active = false;
        const char* owner = nullptr;
        UsbInterfaceSpec def = {};
    };

    bool canActivate(UsbHidInterface type) const;
    static constexpr uint8_t MAX_ACTIVE_HID = 2;

    ServiceState state_ = ServiceState::STOPPED;
    uint8_t activeMask_ = 0;
    bool needsReplug_ = false;
    InterfaceEntry entries_[3] = {};
};

} // namespace cdc::core
