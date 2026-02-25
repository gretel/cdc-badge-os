/**
 * \file
 * \brief TROPIC01 secure-element HAL implementation with session-managed libtropic access.
 */

#include "cdc_hal/ISecureElement.h"
#include "cdc_hal/libtropic_port_esp32.h"
#include "cdc_hal/hw_config.h"
#include "cdc_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "psa/crypto.h"
#include <cstring>

/** \brief libtropic headers (already guarded for C/C++ linkage). */
#include "libtropic.h"
#include "libtropic_common.h"
#include "libtropic_l2.h"
#include "libtropic_l3.h"
#include "lt_l2_api_structs.h"
#include "libtropic_mbedtls_v4.h"

static const char* TAG = "TR01";
static constexpr uint8_t RMEM_HEADER_MAGIC = 0xCD;

/** \brief Pairing key material references (production slot 0). */
#define PAIRING_KEY_PRIV sh0priv_prod0
#define PAIRING_KEY_PUB sh0pub_prod0
#define PAIRING_KEY_SLOT TR01_PAIRING_KEY_SLOT_INDEX_0

namespace cdc::hal {

/**
 * \brief Secure-element implementation backed by libtropic.
 */
class Tropic01Element : public ISecureElement {
public:
    Tropic01Element() = default;

    // IService implementation
    bool init() override;
    bool start() override;
    void stop() override;
    core::ServiceState getState() const override { return state_; }
    const char* getName() const override { return "secure_element"; }

    // Session Management
    bool sessionStart() override;
    void sessionEnd() override;
    bool isSessionActive() const override { return sessionActive_; }
    void sleep() override;

    // ECC Operations
    SeResult eccGenerate(uint8_t slot, EccCurve curve) override;
    SeResult eccImport(uint8_t slot, const uint8_t* privKey, EccCurve curve) override;
    SeResult eccGetPublicKey(uint8_t slot, uint8_t* pubKey, EccCurve* curve) override;
    SeResult eccDelete(uint8_t slot) override;
    bool eccSlotUsed(uint8_t slot) const override;

    // Signing
    SeResult ecdsaSign(uint8_t slot, const uint8_t* hash, size_t hashLen,
                       uint8_t* sig, size_t* sigLen) override;
    SeResult eddsaSign(uint8_t slot, const uint8_t* msg, size_t msgLen,
                       uint8_t* sig) override;

    // R-Memory
    SeResult rmemRead(uint16_t slot, uint8_t* data, uint16_t maxLen,
                      uint16_t* actualLen) override;
    SeResult rmemWrite(uint16_t slot, const uint8_t* data, uint16_t len) override;
    SeResult rmemErase(uint16_t slot) override;
    bool rmemSlotUsed(uint16_t slot) const override;
    SeResult rmemWriteWithHeader(uint16_t slot, uint8_t moduleId,
                                 const char* name, uint8_t flags,
                                 const uint8_t* payload, uint16_t payloadLen) override;
    SeResult rmemReadWithHeader(uint16_t slot, RMemHeader* headerOut,
                                uint8_t* payloadOut, uint16_t payloadMax,
                                uint16_t* payloadLenOut) override;

    // Random
    bool getRandom(uint8_t* buffer, uint16_t size) override;

    // Diagnostics
    bool getChipId(uint8_t* serialNum, uint8_t size) override;
    bool getFwVersion(uint8_t* riscvVer, uint8_t* spectVer) override;

private:
    SeResult mapResult(lt_ret_t ret) const;
    bool ensureSession(const char* op);
    void handleSessionError(lt_ret_t ret);
    uint8_t computeHeaderChecksum(const RMemHeader& header) const;
    bool validateHeader(const RMemHeader& header) const;

    void lock() { if (mutex_) xSemaphoreTakeRecursive(mutex_, portMAX_DELAY); }
    void unlock() { if (mutex_) xSemaphoreGiveRecursive(mutex_); }

    core::ServiceState state_ = core::ServiceState::UNINITIALIZED;
    bool sessionActive_ = false;

    // libtropic handles (static like in legacy)
    lt_handle_t handle_ = {};
    lt_ctx_mbedtls_v4_t cryptoCtx_ = {};
    lt_dev_esp32_t device_ = {};

    // Thread safety
    SemaphoreHandle_t mutex_ = nullptr;

    // Cache for slot usage
    mutable uint32_t eccSlotCache_ = 0;
    mutable bool eccCacheValid_ = false;
};

/**
 * \brief Initializes PSA crypto, libtropic device context, and synchronization state.
 * \return `true` on successful initialization, otherwise `false`.
 */
bool Tropic01Element::init() {
    if (state_ != core::ServiceState::UNINITIALIZED) {
        return state_ == core::ServiceState::INITIALIZED ||
               state_ == core::ServiceState::STARTED;
    }

    LOG_I(TAG, "Initializing TROPIC01...");

    // Initialize MbedTLS PSA Crypto
    psa_status_t psaStatus = psa_crypto_init();
    if (psaStatus != PSA_SUCCESS && psaStatus != PSA_ERROR_BAD_STATE) {
        LOG_E(TAG, "PSA crypto init failed (status=%ld)", (long)psaStatus);
        state_ = core::ServiceState::ERROR;
        return false;
    }
    LOG_I(TAG, "PSA crypto initialized");

    // Create mutex
    mutex_ = xSemaphoreCreateRecursiveMutex();
    if (!mutex_) {
        LOG_E(TAG, "Failed to create mutex");
        state_ = core::ServiceState::ERROR;
        return false;
    }

    // Setup device context (SPI bus init is called internally by lt_port_init)
    memset(&handle_, 0, sizeof(handle_));
    device_.cs_pin = static_cast<gpio_num_t>(TR01_CS_PIN);
    device_.spi = nullptr;
    handle_.l2.device = &device_;
    handle_.l3.crypto_ctx = &cryptoCtx_;

    // Initialize libtropic
    lt_ret_t ret = lt_init(&handle_);
    if (ret != LT_OK) {
        LOG_E(TAG, "lt_init failed (%s)", lt_ret_verbose(ret));
        state_ = core::ServiceState::ERROR;
        return false;
    }

    state_ = core::ServiceState::INITIALIZED;
    LOG_I(TAG, "TROPIC01 initialized (CS=GPIO%d)", TR01_CS_PIN);
    return true;
}

/**
 * \brief Starts secure-element service when initialized.
 * \return `true` when started or already started, otherwise `false`.
 */
bool Tropic01Element::start() {
    if (state_ == core::ServiceState::INITIALIZED ||
        state_ == core::ServiceState::STOPPED) {
        state_ = core::ServiceState::STARTED;
        return true;
    }
    return state_ == core::ServiceState::STARTED;
}

/**
 * \brief Stops secure-element service and closes active session.
 */
void Tropic01Element::stop() {
    if (state_ == core::ServiceState::STARTED) {
        lock();
        if (sessionActive_) {
            sessionEnd();
        }
        unlock();
        state_ = core::ServiceState::STOPPED;
    }
}

/**
 * \brief Opens a secure session with the TROPIC01 chip.
 * \return `true` on success, otherwise `false`.
 */
bool Tropic01Element::sessionStart() {
    lock();

    if (sessionActive_) {
        unlock();
        return true;
    }

    LOG_I(TAG, "Starting secure session...");

    lt_ret_t ret = lt_verify_chip_and_start_secure_session(
        &handle_, PAIRING_KEY_PRIV, PAIRING_KEY_PUB, PAIRING_KEY_SLOT);

    if (ret != LT_OK) {
        LOG_E(TAG, "Secure session failed (%s)", lt_ret_verbose(ret));
        sessionActive_ = false;
        unlock();
        return false;
    }

    sessionActive_ = true;
    eccCacheValid_ = false;

    // Enable chip auto-sleep mode
    uint32_t sleepCfg = 0;
    if (lt_r_config_read(&handle_, TR01_CFG_SLEEP_MODE_ADDR, &sleepCfg) == LT_OK) {
        if (!(sleepCfg & 0x01)) {
            sleepCfg |= 0x01;
            if (lt_r_config_write(&handle_, TR01_CFG_SLEEP_MODE_ADDR, sleepCfg) == LT_OK) {
                LOG_I(TAG, "Auto-sleep enabled");
            }
        }
    }

    LOG_I(TAG, "Secure session active");
    unlock();
    return true;
}

/**
 * \brief Aborts active secure session.
 */
void Tropic01Element::sessionEnd() {
    lock();

    if (!sessionActive_) {
        unlock();
        return;
    }

    lt_session_abort(&handle_);
    sessionActive_ = false;
    LOG_I(TAG, "Session ended");
    unlock();
}

/**
 * \brief Requests secure-element sleep mode and marks session inactive.
 */
void Tropic01Element::sleep() {
    lock();

    if (!sessionActive_) {
        unlock();
        return;
    }

    lt_ret_t ret = lt_sleep(&handle_, TR01_L2_SLEEP_KIND_SLEEP);
    if (ret != LT_OK) {
        LOG_E(TAG, "Sleep failed (%s)", lt_ret_verbose(ret));
        handleSessionError(ret);
    } else {
        sessionActive_ = false;
        LOG_I(TAG, "Entered sleep mode");
    }

    unlock();
}

/**
 * \brief Ensures an active secure session for an operation.
 * \param op Operation name used for logs.
 * \return `true` when session is active, otherwise `false`.
 */
bool Tropic01Element::ensureSession(const char* op) {
    if (sessionActive_) return true;
    LOG_W(TAG, "Session inactive for %s - restarting", op);
    return sessionStart();
}

/**
 * \brief Invalidates session state for session-related libtropic failures.
 * \param ret libtropic return code.
 */
void Tropic01Element::handleSessionError(lt_ret_t ret) {
    switch (ret) {
        case LT_L1_CHIP_ALARM_MODE:
        case LT_L2_HSK_ERR:
        case LT_L2_NO_SESSION:
        case LT_L2_TAG_ERR:
            sessionActive_ = false;
            break;
        default:
            break;
    }
}

/**
 * \brief Maps libtropic return codes to generic secure-element results.
 * \param ret libtropic return code.
 * \return Mapped `SeResult`.
 */
SeResult Tropic01Element::mapResult(lt_ret_t ret) const {
    switch (ret) {
        case LT_OK:
            return SeResult::OK;
        case LT_PARAM_ERR:
            return SeResult::INVALID_PARAM;
        case LT_L3_R_MEM_DATA_READ_SLOT_EMPTY:
        case LT_L3_SLOT_EMPTY:
            return SeResult::SLOT_EMPTY;
        case LT_HOST_NO_SESSION:
        case LT_L2_NO_SESSION:
            return SeResult::SESSION_REQUIRED;
        case LT_L1_CHIP_ALARM_MODE:
            return SeResult::ALARM_MODE;
        default:
            return SeResult::ERROR;
    }
}

/**
 * \brief Generates an ECC key pair in the requested slot.
 * \param slot ECC slot index.
 * \param curve Curve type to generate.
 * \return Operation result.
 */
SeResult Tropic01Element::eccGenerate(uint8_t slot, EccCurve curve) {
    if (slot >= ECC_SLOT_COUNT) {
        return SeResult::INVALID_PARAM;
    }

    lock();

    if (!ensureSession("eccGenerate")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    lt_ecc_curve_type_t ltCurve = (curve == EccCurve::ED25519) ?
                                   TR01_CURVE_ED25519 : TR01_CURVE_P256;

    lt_ret_t ret = lt_ecc_key_generate(&handle_, static_cast<lt_ecc_slot_t>(slot), ltCurve);
    if (ret == LT_OK) {
        eccSlotCache_ |= (1u << slot);
    }
    handleSessionError(ret);

    unlock();
    return mapResult(ret);
}

/**
 * \brief Imports an ECC private key into the requested slot.
 * \param slot ECC slot index.
 * \param privKey Private key bytes.
 * \param curve Curve type of the key.
 * \return Operation result.
 */
SeResult Tropic01Element::eccImport(uint8_t slot, const uint8_t* privKey, EccCurve curve) {
    if (slot >= ECC_SLOT_COUNT || !privKey) {
        return SeResult::INVALID_PARAM;
    }

    lock();

    if (!ensureSession("eccImport")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    lt_ecc_curve_type_t ltCurve = (curve == EccCurve::ED25519) ?
                                   TR01_CURVE_ED25519 : TR01_CURVE_P256;

    lt_ret_t ret = lt_ecc_key_store(&handle_, static_cast<lt_ecc_slot_t>(slot), ltCurve, privKey);
    if (ret == LT_OK) {
        eccSlotCache_ |= (1u << slot);
    }
    handleSessionError(ret);

    unlock();
    return mapResult(ret);
}

/**
 * \brief Reads public key from ECC slot.
 * \param slot ECC slot index.
 * \param pubKey Destination buffer for public key bytes.
 * \param curve Optional destination for detected curve type.
 * \return Operation result.
 */
SeResult Tropic01Element::eccGetPublicKey(uint8_t slot, uint8_t* pubKey, EccCurve* curve) {
    if (slot >= ECC_SLOT_COUNT || !pubKey) {
        return SeResult::INVALID_PARAM;
    }

    lock();

    if (!ensureSession("eccGetPublicKey")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    lt_ecc_curve_type_t ltCurve;
    lt_ecc_key_origin_t ltOrigin;

    // lt_ecc_key_read: (handle, slot, key_buffer, key_max_size, &curve, &origin)
    lt_ret_t ret = lt_ecc_key_read(&handle_, static_cast<lt_ecc_slot_t>(slot),
                                    pubKey, 64, &ltCurve, &ltOrigin);

    if (ret == LT_OK && curve) {
        *curve = (ltCurve == TR01_CURVE_ED25519) ? EccCurve::ED25519 : EccCurve::P256;
    }
    handleSessionError(ret);

    unlock();

    if (ret == LT_L3_INVALID_KEY) {
        return SeResult::SLOT_EMPTY;
    }

    return mapResult(ret);
}

/**
 * \brief Erases ECC key material from slot.
 * \param slot ECC slot index.
 * \return Operation result.
 */
SeResult Tropic01Element::eccDelete(uint8_t slot) {
    if (slot >= ECC_SLOT_COUNT) {
        return SeResult::INVALID_PARAM;
    }

    lock();

    if (!ensureSession("eccDelete")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    lt_ret_t ret = lt_ecc_key_erase(&handle_, static_cast<lt_ecc_slot_t>(slot));
    if (ret == LT_OK) {
        eccSlotCache_ &= ~(1u << slot);
    }
    handleSessionError(ret);

    unlock();
    return mapResult(ret);
}

/**
 * \brief Checks whether ECC slot currently contains a key.
 * \param slot ECC slot index.
 * \return `true` when slot appears populated, otherwise `false`.
 */
bool Tropic01Element::eccSlotUsed(uint8_t slot) const {
    if (slot >= ECC_SLOT_COUNT) {
        return false;
    }

    if (eccCacheValid_) {
        return (eccSlotCache_ & (1u << slot)) != 0;
    }

    auto* self = const_cast<Tropic01Element*>(this);
    uint8_t tempKey[65];
    SeResult res = self->eccGetPublicKey(slot, tempKey, nullptr);

    return (res == SeResult::OK);
}

/**
 * \brief Signs a 32-byte hash using ECDSA key in slot.
 * \param slot ECC slot index.
 * \param hash 32-byte message digest.
 * \param hashLen Digest length (must be 32).
 * \param sig Destination buffer for 64-byte raw `(R,S)` signature.
 * \param sigLen Output signature length.
 * \return Operation result.
 */
SeResult Tropic01Element::ecdsaSign(uint8_t slot, const uint8_t* hash, size_t hashLen,
                                     uint8_t* sig, size_t* sigLen) {
    if (slot >= ECC_SLOT_COUNT || !hash || hashLen != 32 || !sig || !sigLen) {
        return SeResult::INVALID_PARAM;
    }

    lock();

    if (!ensureSession("ecdsaSign")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    // lt_ecc_ecdsa_sign: (handle, slot, msg, msg_len, rs_output_64bytes)
    lt_ret_t ret = lt_ecc_ecdsa_sign(&handle_, static_cast<lt_ecc_slot_t>(slot),
                                      hash, static_cast<uint32_t>(hashLen), sig);

    if (ret == LT_OK) {
        *sigLen = TR01_ECDSA_EDDSA_SIGNATURE_LENGTH;  // Always 64 bytes (R,S)
    }
    handleSessionError(ret);

    unlock();
    return mapResult(ret);
}

/**
 * \brief Signs message using EdDSA key in slot.
 * \param slot ECC slot index.
 * \param msg Message buffer.
 * \param msgLen Message length in bytes.
 * \param sig Destination buffer for signature.
 * \return Operation result.
 */
SeResult Tropic01Element::eddsaSign(uint8_t slot, const uint8_t* msg, size_t msgLen,
                                     uint8_t* sig) {
    if (slot >= ECC_SLOT_COUNT || !msg || msgLen == 0 || !sig) {
        return SeResult::INVALID_PARAM;
    }

    lock();

    if (!ensureSession("eddsaSign")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    lt_ret_t ret = lt_ecc_eddsa_sign(&handle_, static_cast<lt_ecc_slot_t>(slot),
                                      msg, static_cast<uint16_t>(msgLen), sig);
    handleSessionError(ret);

    unlock();
    return mapResult(ret);
}

/**
 * \brief Reads raw R-memory slot data.
 * \param slot R-memory slot index.
 * \param data Destination buffer.
 * \param maxLen Size of `data` in bytes.
 * \param actualLen Optional destination for bytes read.
 * \return Operation result.
 */
SeResult Tropic01Element::rmemRead(uint16_t slot, uint8_t* data, uint16_t maxLen,
                                    uint16_t* actualLen) {
    if (slot >= RMEM_SLOT_COUNT || !data || maxLen == 0) {
        return SeResult::INVALID_PARAM;
    }

    lock();

    if (!ensureSession("rmemRead")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    uint16_t bytesRead = 0;
    lt_ret_t ret = lt_r_mem_data_read(&handle_, slot, data, maxLen, &bytesRead);

    if (ret == LT_OK && actualLen) {
        *actualLen = bytesRead;
    }
    handleSessionError(ret);

    unlock();

    if (ret == LT_L3_R_MEM_DATA_READ_SLOT_EMPTY) {
        if (actualLen) *actualLen = 0;
        return SeResult::SLOT_EMPTY;
    }

    return mapResult(ret);
}

/**
 * \brief Writes raw data to an R-memory slot.
 * \param slot R-memory slot index.
 * \param data Source data buffer.
 * \param len Number of bytes to write.
 * \return Operation result.
 */
SeResult Tropic01Element::rmemWrite(uint16_t slot, const uint8_t* data, uint16_t len) {
    if (slot >= RMEM_SLOT_COUNT || !data || len == 0 || len > RMEM_SLOT_SIZE) {
        return SeResult::INVALID_PARAM;
    }

    lock();

    if (!ensureSession("rmemWrite")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    lt_ret_t ret = lt_r_mem_data_write(&handle_, slot, data, len);
    handleSessionError(ret);

    unlock();
    return mapResult(ret);
}

/**
 * \brief Erases one R-memory slot.
 * \param slot R-memory slot index.
 * \return Operation result.
 */
SeResult Tropic01Element::rmemErase(uint16_t slot) {
    if (slot >= RMEM_SLOT_COUNT) {
        return SeResult::INVALID_PARAM;
    }

    lock();

    if (!ensureSession("rmemErase")) {
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    lt_ret_t ret = lt_r_mem_data_erase(&handle_, slot);
    handleSessionError(ret);

    unlock();
    return mapResult(ret);
}

/**
 * \brief Checks whether R-memory slot contains data.
 * \param slot R-memory slot index.
 * \return `true` when slot contains data, otherwise `false`.
 */
bool Tropic01Element::rmemSlotUsed(uint16_t slot) const {
    if (slot >= RMEM_SLOT_COUNT) {
        return false;
    }

    auto* self = const_cast<Tropic01Element*>(this);
    uint8_t tempBuf[4];
    uint16_t actualLen = 0;
    SeResult res = self->rmemRead(slot, tempBuf, sizeof(tempBuf), &actualLen);

    return (res == SeResult::OK && actualLen > 0);
}

/**
 * \brief Computes header checksum for structured R-memory payload.
 * \param header Header to checksum.
 * \return 8-bit checksum value.
 */
uint8_t Tropic01Element::computeHeaderChecksum(const RMemHeader& header) const {
    uint16_t sum = 0;
    sum += header.moduleId;
    sum += header.flags;
    for (size_t i = 0; i < sizeof(header.name); i++) {
        sum += static_cast<uint8_t>(header.name[i]);
    }
    sum += static_cast<uint8_t>(header.payloadLen & 0xFF);
    sum += static_cast<uint8_t>((header.payloadLen >> 8) & 0xFF);
    return static_cast<uint8_t>(sum & 0xFF);
}

/**
 * \brief Validates header magic and checksum.
 * \param header Header to validate.
 * \return `true` when valid, otherwise `false`.
 */
bool Tropic01Element::validateHeader(const RMemHeader& header) const {
    if (header.magic != RMEM_HEADER_MAGIC) {
        return false;
    }
    return header.checksum == computeHeaderChecksum(header);
}

/**
 * \brief Writes payload to R-memory slot with metadata header.
 * \param slot R-memory slot index.
 * \param moduleId Owning module identifier.
 * \param name Optional short record name.
 * \param flags Record flags.
 * \param payload Payload data pointer.
 * \param payloadLen Payload length in bytes.
 * \return Operation result.
 */
SeResult Tropic01Element::rmemWriteWithHeader(uint16_t slot, uint8_t moduleId,
                                              const char* name, uint8_t flags,
                                              const uint8_t* payload, uint16_t payloadLen) {
    if (slot >= RMEM_SLOT_COUNT) {
        return SeResult::INVALID_PARAM;
    }
    if (payloadLen > (RMEM_SLOT_SIZE - sizeof(RMemHeader))) {
        return SeResult::INVALID_PARAM;
    }

    // R-Memory requires erase before write
    SeResult eraseRes = rmemErase(slot);
    if (eraseRes != SeResult::OK && eraseRes != SeResult::SLOT_EMPTY) {
        return eraseRes;
    }

    RMemHeader header = {};
    header.magic = RMEM_HEADER_MAGIC;
    header.moduleId = moduleId;
    header.flags = flags;
    header.payloadLen = payloadLen;
    if (name) {
        strncpy(header.name, name, sizeof(header.name) - 1);
        header.name[sizeof(header.name) - 1] = '\0';
    }
    header.checksum = computeHeaderChecksum(header);

    uint8_t buffer[RMEM_SLOT_SIZE] = {};
    memcpy(buffer, &header, sizeof(header));
    if (payloadLen > 0 && payload) {
        memcpy(buffer + sizeof(header), payload, payloadLen);
    }

    return rmemWrite(slot, buffer, static_cast<uint16_t>(sizeof(header) + payloadLen));
}

/**
 * \brief Reads and validates headered R-memory record.
 * \param slot R-memory slot index.
 * \param headerOut Optional destination for parsed header.
 * \param payloadOut Optional destination for payload bytes.
 * \param payloadMax Capacity of `payloadOut`.
 * \param payloadLenOut Optional destination for payload length.
 * \return Operation result.
 */
SeResult Tropic01Element::rmemReadWithHeader(uint16_t slot, RMemHeader* headerOut,
                                             uint8_t* payloadOut, uint16_t payloadMax,
                                             uint16_t* payloadLenOut) {
    if (slot >= RMEM_SLOT_COUNT) {
        return SeResult::INVALID_PARAM;
    }

    uint8_t buffer[RMEM_SLOT_SIZE] = {};
    uint16_t actualLen = 0;
    SeResult res = rmemRead(slot, buffer, sizeof(buffer), &actualLen);
    if (res != SeResult::OK) {
        return res;
    }
    if (actualLen < sizeof(RMemHeader)) {
        return SeResult::ERROR;
    }

    RMemHeader header = {};
    memcpy(&header, buffer, sizeof(header));
    if (!validateHeader(header)) {
        return SeResult::ERROR;
    }
    if (header.payloadLen > (RMEM_SLOT_SIZE - sizeof(RMemHeader))) {
        return SeResult::ERROR;
    }
    if (actualLen < static_cast<uint16_t>(sizeof(RMemHeader) + header.payloadLen)) {
        return SeResult::ERROR;
    }

    if (headerOut) {
        *headerOut = header;
    }
    if (payloadLenOut) {
        *payloadLenOut = header.payloadLen;
    }
    if (payloadOut && header.payloadLen > 0) {
        if (payloadMax < header.payloadLen) {
            return SeResult::INVALID_PARAM;
        }
        memcpy(payloadOut, buffer + sizeof(RMemHeader), header.payloadLen);
    }

    return SeResult::OK;
}

/**
 * \brief Fills buffer with random bytes from TROPIC TRNG with ESP fallback.
 * \param buffer Destination buffer.
 * \param size Number of random bytes requested.
 * \return Always `true` when parameters are valid.
 */
bool Tropic01Element::getRandom(uint8_t* buffer, uint16_t size) {
    if (!buffer || size == 0) {
        return false;
    }

    lock();

    if (!ensureSession("getRandom")) {
        // Fallback to ESP32 TRNG
        LOG_W(TAG, "Using ESP32 TRNG as fallback");
        esp_fill_random(buffer, size);
        unlock();
        return true;
    }

    lt_ret_t ret = lt_random_value_get(&handle_, buffer, size);
    handleSessionError(ret);

    unlock();

    if (ret != LT_OK) {
        LOG_W(TAG, "TROPIC01 TRNG failed, using ESP32 TRNG");
        esp_fill_random(buffer, size);
    }

    return true;
}

/**
 * \brief Reads chip serial identifier.
 * \param serialNum Destination buffer.
 * \param size Size of `serialNum` buffer.
 * \return `true` on success, otherwise `false`.
 */
bool Tropic01Element::getChipId(uint8_t* serialNum, uint8_t size) {
    if (!serialNum || size < 8) {
        return false;
    }

    lock();

    if (!ensureSession("getChipId")) {
        unlock();
        return false;
    }

    struct lt_chip_id_t chipId;
    memset(&chipId, 0, sizeof(chipId));
    lt_ret_t ret = lt_get_info_chip_id(&handle_, &chipId);

    if (ret == LT_OK) {
        // Copy serial number bytes from ser_num struct
        uint8_t copyLen = (size < sizeof(chipId.ser_num)) ? size : sizeof(chipId.ser_num);
        memcpy(serialNum, &chipId.ser_num, copyLen);
    }
    handleSessionError(ret);

    unlock();
    return ret == LT_OK;
}

/**
 * \brief Reads RISC-V and SPECT firmware major version bytes.
 * \param riscvVer Destination for RISC-V firmware version.
 * \param spectVer Destination for SPECT firmware version.
 * \return `true` on success, otherwise `false`.
 */
bool Tropic01Element::getFwVersion(uint8_t* riscvVer, uint8_t* spectVer) {
    if (!riscvVer || !spectVer) {
        return false;
    }

    lock();

    if (!ensureSession("getFwVersion")) {
        unlock();
        return false;
    }

    uint8_t riscvFw[TR01_L2_GET_INFO_RISCV_FW_SIZE] = {0};
    lt_ret_t ret = lt_get_info_riscv_fw_ver(&handle_, riscvFw);

    if (ret == LT_OK) {
        *riscvVer = riscvFw[0];
    }

    uint8_t spectFw[TR01_L2_GET_INFO_SPECT_FW_SIZE] = {0};
    ret = lt_get_info_spect_fw_ver(&handle_, spectFw);

    if (ret == LT_OK) {
        *spectVer = spectFw[0];
    }
    handleSessionError(ret);

    unlock();
    return ret == LT_OK;
}

/** \brief Global singleton instance of TROPIC secure-element implementation. */
static Tropic01Element g_secureElement;

/**
 * \brief Returns the singleton secure element service instance.
 * \return Pointer to the global `ISecureElement` implementation.
 */
ISecureElement* getSecureElementInstance() {
    return &g_secureElement;
}

} // namespace cdc::hal
