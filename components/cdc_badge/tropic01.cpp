// TROPIC01 Secure Element wrapper
// Provides session management with automatic sleep

#include "tropic01.h"
#include "hw_config.h"
#include "libtropic_port_esp32.h"
#include "cdc_log.h"

#include "libtropic.h"
#include "libtropic_common.h"
#include "libtropic_mbedtls_v4.h"
#include "psa/crypto.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

// Pairing keys (production slot 0)
#define PAIRING_KEY_PRIV sh0priv_prod0
#define PAIRING_KEY_PUB sh0pub_prod0
#define PAIRING_KEY_SLOT TR01_PAIRING_KEY_SLOT_INDEX_0

// Static handles
static lt_handle_t tr01_handle = {};
static lt_ctx_mbedtls_v4_t tr01_crypto_ctx = {};
static lt_dev_esp32_t tr01_device = {};
static bool session_active = false;
static SemaphoreHandle_t tr01_mutex = nullptr;

// Scoped lock for thread safety
class Tr01Lock {
public:
    explicit Tr01Lock(uint32_t timeout_ms = 2000) {
        locked_ = tr01_mutex &&
                  xSemaphoreTakeRecursive(tr01_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
        if (!locked_) {
            LOG_E("TR01", "Mutex lock failed");
        }
    }

    ~Tr01Lock() {
        if (locked_ && tr01_mutex) {
            xSemaphoreGiveRecursive(tr01_mutex);
        }
    }

    bool ok() const { return locked_; }

private:
    bool locked_ = false;
};

// Handle session errors
static void handle_session_error(lt_ret_t ret) {
    switch (ret) {
        case LT_L1_CHIP_ALARM_MODE:
        case LT_L2_HSK_ERR:
        case LT_L2_NO_SESSION:
        case LT_L2_TAG_ERR:
            session_active = false;
            break;
        default:
            break;
    }
}

bool tropic01_init(void) {
    LOG_I("TR01", "tropic01_init()");

    // Initialize MbedTLS PSA Crypto
    LOG_I("TR01", "Calling psa_crypto_init()...");
    psa_status_t psa_status = psa_crypto_init();
    LOG_I("TR01", "psa_crypto_init() returned %ld", (long)psa_status);
    if (psa_status != PSA_SUCCESS) {
        LOG_E("TR01", "PSA crypto init failed (status=%ld)", (long)psa_status);
        return false;
    }
    LOG_I("TR01", "PSA crypto initialized OK");

    // Setup device context
    memset(&tr01_handle, 0, sizeof(tr01_handle));
    tr01_device.cs_pin = static_cast<gpio_num_t>(TR01_CS_PIN);
    tr01_device.spi = nullptr;
    tr01_handle.l2.device = &tr01_device;
    tr01_handle.l3.crypto_ctx = &tr01_crypto_ctx;

    // Initialize libtropic
    LOG_I("TR01", "Calling lt_init()...");
    LOG_I("TR01", "  crypto_ctx=%p, buff=%p, buff_len=%d",
          tr01_handle.l3.crypto_ctx, tr01_handle.l3.buff, tr01_handle.l3.buff_len);
    lt_ret_t ret = lt_init(&tr01_handle);
    LOG_I("TR01", "lt_init() returned %d (%s)", ret, lt_ret_verbose(ret));
    if (ret != LT_OK) {
        LOG_E("TR01", "lt_init failed (%s)", lt_ret_verbose(ret));
        return false;
    }
    LOG_I("TR01", "  After init: buff=%p, buff_len=%d", tr01_handle.l3.buff, tr01_handle.l3.buff_len);

    // Create mutex for thread safety
    if (!tr01_mutex) {
        tr01_mutex = xSemaphoreCreateRecursiveMutex();
        if (!tr01_mutex) {
            LOG_E("TR01", "Mutex creation failed");
            return false;
        }
    }

    session_active = false;
    LOG_I("TR01", "Initialized");
    return true;
}

bool tropic01_session_start(void) {
    Tr01Lock lock;
    if (!lock.ok()) return false;

    LOG_I("TR01", "Starting secure session...");

    lt_ret_t ret = lt_verify_chip_and_start_secure_session(
        &tr01_handle, PAIRING_KEY_PRIV, PAIRING_KEY_PUB, PAIRING_KEY_SLOT);

    if (ret != LT_OK) {
        LOG_E("TR01", "Secure session failed (%s)", lt_ret_verbose(ret));
        session_active = false;
        return false;
    }

    session_active = true;
    LOG_I("TR01", "Secure session active");
    return true;
}

bool tropic01_session_active(void) {
    return session_active;
}

void tropic01_sleep(void) {
    if (!session_active) return;

    Tr01Lock lock(0);  // Non-blocking
    if (!lock.ok()) return;

    lt_ret_t ret = lt_sleep(&tr01_handle, TR01_L2_SLEEP_KIND_SLEEP);
    if (ret != LT_OK) {
        LOG_E("TR01", "Sleep failed (%s)", lt_ret_verbose(ret));
        handle_session_error(ret);
        return;
    }

    session_active = false;
    LOG_I("TR01", "Entered sleep mode");
}

// Ensure session is active before operations
static bool ensure_session(const char *op) {
    if (session_active) return true;
    LOG_W("TR01", "Session inactive for %s - restarting", op);
    return tropic01_session_start();
}

bool tropic01_rmem_read(uint16_t slot, uint8_t *data, uint16_t max_size, uint16_t *read_size) {
    Tr01Lock lock;
    if (!lock.ok()) return false;
    if (!ensure_session("rmem_read")) return false;

    uint16_t bytes_read = 0;
    lt_ret_t ret = lt_r_mem_data_read(&tr01_handle, slot, data, max_size, &bytes_read);

    if (ret != LT_OK) {
        LOG_E("TR01", "R-Memory read failed slot=%d (%s)", slot, lt_ret_verbose(ret));
        handle_session_error(ret);
        return false;
    }

    if (read_size) *read_size = bytes_read;
    LOG_D("TR01", "R-Memory read OK slot=%d size=%d", slot, bytes_read);
    return true;
}

bool tropic01_rmem_write(uint16_t slot, const uint8_t *data, uint16_t size) {
    Tr01Lock lock;
    if (!lock.ok()) return false;
    if (!ensure_session("rmem_write")) return false;

    lt_ret_t ret = lt_r_mem_data_write(&tr01_handle, slot, data, size);

    if (ret != LT_OK) {
        LOG_E("TR01", "R-Memory write failed slot=%d (%s)", slot, lt_ret_verbose(ret));
        handle_session_error(ret);
        return false;
    }

    LOG_D("TR01", "R-Memory write OK slot=%d size=%d", slot, size);
    return true;
}

bool tropic01_rmem_erase(uint16_t slot) {
    Tr01Lock lock;
    if (!lock.ok()) return false;
    if (!ensure_session("rmem_erase")) return false;

    lt_ret_t ret = lt_r_mem_data_erase(&tr01_handle, slot);

    if (ret != LT_OK) {
        LOG_E("TR01", "R-Memory erase failed slot=%d (%s)", slot, lt_ret_verbose(ret));
        handle_session_error(ret);
        return false;
    }

    LOG_D("TR01", "R-Memory erase OK slot=%d", slot);
    return true;
}
