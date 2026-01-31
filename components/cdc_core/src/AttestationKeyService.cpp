#include "cdc_core/AttestationKeyService.h"
#include "cdc_log.h"
#include <mbedtls/sha256.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <cstring>

static const char* TAG = "AttestKey";
static constexpr const char* NVS_NAMESPACE = "attest";
static constexpr const char* NVS_KEY_PUBHASH = "pubhash";
static constexpr uint32_t RETRY_INTERVAL_MS = 3000;

namespace cdc::core {

bool AttestationKeyService::init() {
    if (state_ != ServiceState::UNINITIALIZED) {
        return state_ == ServiceState::INITIALIZED || state_ == ServiceState::STARTED;
    }
    state_ = ServiceState::INITIALIZED;
    return true;
}

bool AttestationKeyService::start() {
    if (state_ == ServiceState::UNINITIALIZED) {
        if (!init()) return false;
    }
    state_ = ServiceState::STARTED;
    return true;
}

void AttestationKeyService::stop() {
    state_ = ServiceState::STOPPED;
}

void AttestationKeyService::onTick(uint32_t nowMs) {
    if (state_ != ServiceState::STARTED || ready_) return;
    if (nowMs - lastAttemptMs_ < RETRY_INTERVAL_MS) return;
    lastAttemptMs_ = nowMs;
    if (ensureKey()) {
        ready_ = true;
        LOG_I(TAG, "Attestation key ready");
    }
}

bool AttestationKeyService::loadStoredHash(uint8_t* out, size_t outLen) {
    if (!out || outLen == 0) return false;
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return false;
    }
    size_t len = outLen;
    esp_err_t err = nvs_get_blob(nvs, NVS_KEY_PUBHASH, out, &len);
    nvs_close(nvs);
    return err == ESP_OK && len == outLen;
}

bool AttestationKeyService::saveStoredHash(const uint8_t* data, size_t len) {
    if (!data || len == 0) return false;
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        return false;
    }
    esp_err_t err = nvs_set_blob(nvs, NVS_KEY_PUBHASH, data, len);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err == ESP_OK;
}

bool AttestationKeyService::ensureKey() {
    if (!secureElement_) {
        LOG_W(TAG, "Secure element not set");
        return false;
    }
    if (!secureElement_->isSessionActive()) {
        if (!secureElement_->sessionStart()) {
            LOG_W(TAG, "Secure element session not active");
            return false;
        }
    }

    uint8_t pubkey[64] = {};
    hal::EccCurve curve = hal::EccCurve::P256;
    hal::SeResult res = secureElement_->eccGetPublicKey(ATTESTATION_ECC_SLOT, pubkey, &curve);

    if (res == hal::SeResult::SLOT_EMPTY) {
        LOG_I(TAG, "Attestation slot empty, generating key");
        if (secureElement_->eccGenerate(ATTESTATION_ECC_SLOT, hal::EccCurve::P256) !=
            hal::SeResult::OK) {
            LOG_E(TAG, "Failed to generate attestation key");
            return false;
        }
        res = secureElement_->eccGetPublicKey(ATTESTATION_ECC_SLOT, pubkey, &curve);
    }

    if (res != hal::SeResult::OK) {
        LOG_W(TAG, "Attestation key read failed: %d", static_cast<int>(res));
        return false;
    }

    if (curve != hal::EccCurve::P256) {
        LOG_W(TAG, "Attestation key wrong curve, regenerating");
        secureElement_->eccDelete(ATTESTATION_ECC_SLOT);
        if (secureElement_->eccGenerate(ATTESTATION_ECC_SLOT, hal::EccCurve::P256) !=
            hal::SeResult::OK) {
            LOG_E(TAG, "Failed to regenerate attestation key");
            return false;
        }
        res = secureElement_->eccGetPublicKey(ATTESTATION_ECC_SLOT, pubkey, &curve);
        if (res != hal::SeResult::OK) return false;
    }

    uint8_t hash[32] = {};
    mbedtls_sha256(pubkey, sizeof(pubkey), hash, 0);

    uint8_t stored[32] = {};
    if (loadStoredHash(stored, sizeof(stored))) {
        if (memcmp(stored, hash, sizeof(hash)) == 0) {
            return true;
        }
        LOG_W(TAG, "Attestation key mismatch, regenerating");
        secureElement_->eccDelete(ATTESTATION_ECC_SLOT);
        if (secureElement_->eccGenerate(ATTESTATION_ECC_SLOT, hal::EccCurve::P256) !=
            hal::SeResult::OK) {
            LOG_E(TAG, "Failed to regenerate attestation key");
            return false;
        }
        res = secureElement_->eccGetPublicKey(ATTESTATION_ECC_SLOT, pubkey, &curve);
        if (res != hal::SeResult::OK) return false;
        mbedtls_sha256(pubkey, sizeof(pubkey), hash, 0);
    }

    if (!saveStoredHash(hash, sizeof(hash))) {
        LOG_W(TAG, "Failed to store attestation key hash");
    }

    return true;
}

} // namespace cdc::core
