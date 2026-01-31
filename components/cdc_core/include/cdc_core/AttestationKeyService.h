#pragma once

#include "cdc_core/IService.h"
#include "cdc_hal/ISecureElement.h"
#include <cstdint>

namespace cdc::core {

class AttestationKeyService : public IService {
public:
    static constexpr uint8_t ATTESTATION_ECC_SLOT = 0;

    void setSecureElement(hal::ISecureElement* se) { secureElement_ = se; }

    bool init() override;
    bool start() override;
    void stop() override;
    ServiceState getState() const override { return state_; }
    const char* getName() const override { return "attestation_key"; }

    void onTick(uint32_t nowMs);

private:
    bool ensureKey();
    bool loadStoredHash(uint8_t* out, size_t outLen);
    bool saveStoredHash(const uint8_t* data, size_t len);

    hal::ISecureElement* secureElement_ = nullptr;
    ServiceState state_ = ServiceState::UNINITIALIZED;
    bool ready_ = false;
    uint32_t lastAttemptMs_ = 0;
};

} // namespace cdc::core
