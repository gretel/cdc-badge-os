// TROPIC01 Secure Element wrapper
// Provides session management with automatic sleep
//
// IMPORTANT: All write/erase operations automatically update the cache!
// Never access libtropic directly from other files.

#include "tropic01.h"
#include "tropic01_cache.h"
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

// Debug flag for verbose TR01 logging (0=off, 1=detailed session/operation logging)
#ifndef TR01_VERBOSE_DEBUG
#define TR01_VERBOSE_DEBUG 0
#endif

#if TR01_VERBOSE_DEBUG
#define TR01_DBG(fmt, ...) TR01_DBG( fmt, ##__VA_ARGS__)
#else
#define TR01_DBG(fmt, ...) ((void)0)
#endif

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
    TR01_DBG( "=== SESSION START BEGIN ===");

    Tr01Lock lock;
    if (!lock.ok()) {
        LOG_E("TR01-DBG", "Mutex lock failed in session_start!");
        return false;
    }

    LOG_I("TR01", "Starting secure session...");
    TR01_DBG( "Pairing key slot: %d", PAIRING_KEY_SLOT);
    TR01_DBG( "Handle: device=%p, buff=%p",
          tr01_handle.l2.device, tr01_handle.l2.buff);

    lt_ret_t ret = lt_verify_chip_and_start_secure_session(
        &tr01_handle, PAIRING_KEY_PRIV, PAIRING_KEY_PUB, PAIRING_KEY_SLOT);

    TR01_DBG( "lt_verify_chip_and_start_secure_session returned: %d (%s)",
          ret, lt_ret_verbose(ret));

    if (ret != LT_OK) {
        LOG_E("TR01", "Secure session failed (%s)", lt_ret_verbose(ret));

        // Debug: show what went wrong
        if (ret == LT_L1_CHIP_ALARM_MODE) {
            LOG_E("TR01-DBG", "ALARM MODE during session start!");
            LOG_E("TR01-DBG", "L2 buff[0] (CHIP_STATUS): 0x%02X", tr01_handle.l2.buff[0]);
        }

        session_active = false;
        TR01_DBG( "=== SESSION START FAILED ===");
        return false;
    }

    session_active = true;
    TR01_DBG( "Session established, session_status=%d",
          tr01_handle.l3.session_status);

    // Enable chip auto-sleep mode (chip sleeps automatically when idle)
    uint32_t sleep_cfg = 0;
    if (lt_r_config_read(&tr01_handle, TR01_CFG_SLEEP_MODE_ADDR, &sleep_cfg) == LT_OK) {
        TR01_DBG( "Current sleep_cfg: 0x%08lX", (unsigned long)sleep_cfg);
        if (!(sleep_cfg & 0x01)) {  // SLEEP_MODE_EN bit
            sleep_cfg |= 0x01;
            if (lt_r_config_write(&tr01_handle, TR01_CFG_SLEEP_MODE_ADDR, sleep_cfg) == LT_OK) {
                LOG_I("TR01", "Auto-sleep enabled");
            }
        }
    }

    LOG_I("TR01", "Secure session active");
    TR01_DBG( "=== SESSION START SUCCESS ===");
    return true;
}

bool tropic01_session_active(void) {
    return session_active;
}

void tropic01_sleep(void) {
    if (!session_active) return;

    Tr01Lock lock(100);  // 100ms timeout
    if (!lock.ok()) {
        LOG_W("TR01", "Sleep failed - chip busy (mutex timeout)");
        return;
    }

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

    // Automatically update cache after successful write
    tropic01_cache_rmem_update(slot, data, size);

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

    // Automatically invalidate cache after successful erase
    tropic01_cache_rmem_invalidate(slot);

    LOG_D("TR01", "R-Memory erase OK slot=%d", slot);
    return true;
}

void tropic01_session_abort(void) {
    Tr01Lock lock(0);
    if (!lock.ok()) return;

    if (session_active) {
        lt_session_abort(&tr01_handle);
        session_active = false;
        LOG_I("TR01", "Session aborted");
    }
}

// Debug helper to dump CHIP_STATUS interpretation
static void debug_chip_status(uint8_t status) {
    TR01_DBG( "CHIP_STATUS=0x%02X: READY=%d ALARM=%d STARTUP=%d",
          status,
          (status & 0x01) ? 1 : 0,  // READY bit
          (status & 0x02) ? 1 : 0,  // ALARM bit
          (status & 0x04) ? 1 : 0); // STARTUP bit

    if (status == 0xFF) {
        TR01_DBG( "STATUS=0xFF -> Likely SPI/MISO issue (all bits high)!");
    }
    if (status & 0x02) {
        TR01_DBG( "ALARM bit set -> Chip in alarm mode OR SPI communication failure");
    }
}

// Debug helper to get current chip status via GET_INFO
static void debug_query_chip_status(void) {
    // Read the buffer to get CHIP_STATUS
    uint8_t* buff = tr01_handle.l2.buff;
    if (buff) {
        TR01_DBG( "L2 buffer[0-7]: %02X %02X %02X %02X %02X %02X %02X %02X",
              buff[0], buff[1], buff[2], buff[3], buff[4], buff[5], buff[6], buff[7]);
    }
}

bool tropic01_ecc_key_generate(uint8_t slot, uint8_t curve) {
    TR01_DBG( "=== ECC KEY GENERATE START ===");
    TR01_DBG( "Requested: slot=%d, curve=%d (%s)",
          slot, curve, (curve == CDC_CURVE_ED25519) ? "Ed25519" : "P-256");

    Tr01Lock lock;
    if (!lock.ok()) {
        LOG_E("TR01-DBG", "Mutex lock failed!");
        return false;
    }
    TR01_DBG( "Mutex acquired");

    TR01_DBG( "Session state before ensure_session: %s",
          session_active ? "ACTIVE" : "INACTIVE");

    if (!ensure_session("ecc_key_generate")) {
        LOG_E("TR01-DBG", "ensure_session failed!");
        return false;
    }
    TR01_DBG( "Session state after ensure_session: %s",
          session_active ? "ACTIVE" : "INACTIVE");

    // Map our curve constants to libtropic enum
    lt_ecc_curve_type_t lt_curve = (curve == CDC_CURVE_ED25519)
        ? TR01_CURVE_ED25519 : TR01_CURVE_P256;

    TR01_DBG( "Calling lt_ecc_key_generate(slot=%d, curve=%d)...", slot, lt_curve);
    TR01_DBG( "Handle state: session_status=%d, buff=%p, buff_len=%d",
          tr01_handle.l3.session_status,
          tr01_handle.l3.buff,
          tr01_handle.l3.buff_len);

    // Debug: Query chip status before operation
    debug_query_chip_status();

    lt_ret_t ret = lt_ecc_key_generate(&tr01_handle, (lt_ecc_slot_t)slot, lt_curve);

    TR01_DBG( "lt_ecc_key_generate returned: %d (%s)", ret, lt_ret_verbose(ret));

    // Debug: Show buffer after operation
    debug_query_chip_status();

    if (ret != LT_OK) {
        LOG_E("TR01", "ECC key generate failed slot=%d (%s)", slot, lt_ret_verbose(ret));

        // Additional debug for ALARM mode
        if (ret == LT_L1_CHIP_ALARM_MODE) {
            LOG_E("TR01-DBG", "=== ALARM MODE ANALYSIS ===");
            LOG_E("TR01-DBG", "Error code 0x%02X = LT_L1_CHIP_ALARM_MODE", ret);
            LOG_E("TR01-DBG", "This can mean:");
            LOG_E("TR01-DBG", "  1. Actual security alarm (chip violated)");
            LOG_E("TR01-DBG", "  2. SPI MISO returning all 1s (connection issue)");
            debug_chip_status(tr01_handle.l2.buff[0]);
        }

        handle_session_error(ret);
        TR01_DBG( "=== ECC KEY GENERATE FAILED ===");
        return false;
    }

    // Read back public key and update cache
    uint8_t pubkey[64];
    lt_ecc_curve_type_t read_curve;
    lt_ecc_key_origin_t read_origin;
    TR01_DBG( "Reading back generated key...");
    ret = lt_ecc_key_read(&tr01_handle, (lt_ecc_slot_t)slot,
                          pubkey, sizeof(pubkey), &read_curve, &read_origin);
    if (ret == LT_OK) {
        tropic01_cache_ecc_update(slot, pubkey, curve);
        TR01_DBG( "Key read back OK, cache updated");
    } else {
        LOG_W("TR01", "Could not read back key for cache (slot=%d)", slot);
    }

    LOG_I("TR01", "ECC key generated slot=%d curve=%d", slot, curve);
    TR01_DBG( "=== ECC KEY GENERATE SUCCESS ===");
    return true;
}

bool tropic01_ecc_key_read(uint8_t slot, uint8_t *pubkey, uint8_t pubkey_size,
                           uint8_t *curve, uint8_t *origin) {
    Tr01Lock lock;
    if (!lock.ok()) return false;
    if (!ensure_session("ecc_key_read")) return false;

    lt_ecc_curve_type_t lt_curve;
    lt_ecc_key_origin_t lt_origin;

    lt_ret_t ret = lt_ecc_key_read(&tr01_handle, (lt_ecc_slot_t)slot,
                                   pubkey, pubkey_size, &lt_curve, &lt_origin);

    if (ret != LT_OK) {
        if (ret != LT_L3_INVALID_KEY) {
            LOG_E("TR01", "ECC key read failed slot=%d (%s)", slot, lt_ret_verbose(ret));
        }
        handle_session_error(ret);
        return false;
    }

    if (curve) *curve = (uint8_t)lt_curve;
    if (origin) *origin = (uint8_t)lt_origin;

    LOG_D("TR01", "ECC key read OK slot=%d", slot);
    return true;
}

bool tropic01_ecc_key_erase(uint8_t slot) {
    Tr01Lock lock;
    if (!lock.ok()) return false;
    if (!ensure_session("ecc_key_erase")) return false;

    lt_ret_t ret = lt_ecc_key_erase(&tr01_handle, (lt_ecc_slot_t)slot);

    if (ret != LT_OK) {
        LOG_E("TR01", "ECC key erase failed slot=%d (%s)", slot, lt_ret_verbose(ret));
        handle_session_error(ret);
        return false;
    }

    // Automatically invalidate cache after successful erase
    tropic01_cache_ecc_invalidate(slot);

    LOG_I("TR01", "ECC key erased slot=%d", slot);
    return true;
}

bool tropic01_ecdsa_sign(uint8_t slot, const uint8_t *hash, uint32_t hash_len,
                         uint8_t *signature) {
    Tr01Lock lock;
    if (!lock.ok()) return false;
    if (!ensure_session("ecdsa_sign")) return false;

    lt_ret_t ret = lt_ecc_ecdsa_sign(&tr01_handle, (lt_ecc_slot_t)slot,
                                     hash, hash_len, signature);

    if (ret != LT_OK) {
        LOG_E("TR01", "ECDSA sign failed slot=%d (%s)", slot, lt_ret_verbose(ret));
        handle_session_error(ret);
        return false;
    }

    LOG_D("TR01", "ECDSA sign OK slot=%d", slot);
    return true;
}

bool tropic01_eddsa_sign(uint8_t slot, const uint8_t *msg, uint16_t msg_len,
                         uint8_t *signature) {
    Tr01Lock lock;
    if (!lock.ok()) return false;
    if (!ensure_session("eddsa_sign")) return false;

    lt_ret_t ret = lt_ecc_eddsa_sign(&tr01_handle, (lt_ecc_slot_t)slot,
                                     msg, msg_len, signature);

    if (ret != LT_OK) {
        LOG_E("TR01", "EdDSA sign failed slot=%d (%s)", slot, lt_ret_verbose(ret));
        handle_session_error(ret);
        return false;
    }

    LOG_D("TR01", "EdDSA sign OK slot=%d", slot);
    return true;
}

bool tropic01_get_random(uint8_t *buffer, uint16_t size) {
    Tr01Lock lock;
    if (!lock.ok()) return false;
    if (!ensure_session("get_random")) return false;

    lt_ret_t ret = lt_random_value_get(&tr01_handle, buffer, size);

    if (ret != LT_OK) {
        LOG_E("TR01", "Random get failed (%s)", lt_ret_verbose(ret));
        handle_session_error(ret);
        return false;
    }

    LOG_D("TR01", "Random get OK size=%d", size);
    return true;
}

// ============================================================================
// Diagnostics API
// ============================================================================

bool tropic01_get_chip_id(uint8_t *serial_num, uint8_t serial_size) {
    Tr01Lock lock;
    if (!lock.ok()) return false;
    if (!ensure_session("get_chip_id")) return false;

    struct lt_chip_id_t chip_id = {};
    lt_ret_t ret = lt_get_info_chip_id(&tr01_handle, &chip_id);

    if (ret != LT_OK) {
        LOG_E("TR01", "Get chip ID failed (%s)", lt_ret_verbose(ret));
        handle_session_error(ret);
        return false;
    }

    // Copy serial number (8 bytes)
    if (serial_num && serial_size >= 8) {
        memcpy(serial_num, &chip_id.ser_num, 8);
    }

    LOG_D("TR01", "Chip ID retrieved");
    return true;
}

bool tropic01_get_fw_version(uint8_t *riscv_ver, uint8_t *spect_ver) {
    Tr01Lock lock;
    if (!lock.ok()) return false;
    if (!ensure_session("get_fw_version")) return false;

    if (riscv_ver) {
        lt_ret_t ret = lt_get_info_riscv_fw_ver(&tr01_handle, riscv_ver);
        if (ret != LT_OK) {
            LOG_E("TR01", "Get RISC-V FW version failed (%s)", lt_ret_verbose(ret));
            handle_session_error(ret);
            return false;
        }
    }

    if (spect_ver) {
        lt_ret_t ret = lt_get_info_spect_fw_ver(&tr01_handle, spect_ver);
        if (ret != LT_OK) {
            LOG_E("TR01", "Get SPECT FW version failed (%s)", lt_ret_verbose(ret));
            handle_session_error(ret);
            return false;
        }
    }

    LOG_D("TR01", "FW version retrieved");
    return true;
}

bool tropic01_get_ecc_status(tropic01_ecc_status_t *status) {
    if (!status) return false;

    memset(status, 0, sizeof(tropic01_ecc_status_t));

    // Use cache for slot status (no TROPIC01 access!)
    for (int i = 0; i < TR01_ECC_SLOT_COUNT; i++) {
        if (tropic01_cache_ecc_exists(i)) {
            status->slot_used[i] = true;
            tropic01_cache_ecc_get_pubkey(i, nullptr, &status->slot_curve[i]);
        }
    }

    return true;
}

// ============================================================================
// R-Config Operations (Reversible Configuration)
// ============================================================================

bool tropic01_rconfig_read(uint8_t addr, uint32_t *value) {
    if (!value) return false;

    Tr01Lock lock;
    if (!lock.ok()) return false;
    if (!ensure_session("rconfig_read")) return false;

    lt_ret_t ret = lt_r_config_read(&tr01_handle, (lt_config_obj_addr_t)addr, value);

    if (ret != LT_OK) {
        LOG_E("TR01", "R-Config read failed addr=%d (%s)", addr, lt_ret_verbose(ret));
        handle_session_error(ret);
        return false;
    }

    LOG_D("TR01", "R-Config read OK addr=%d value=0x%08lX", addr, (unsigned long)*value);
    return true;
}

bool tropic01_rconfig_write(uint8_t addr, uint32_t value) {
    Tr01Lock lock;
    if (!lock.ok()) return false;
    if (!ensure_session("rconfig_write")) return false;

    lt_ret_t ret = lt_r_config_write(&tr01_handle, (lt_config_obj_addr_t)addr, value);

    if (ret != LT_OK) {
        LOG_E("TR01", "R-Config write failed addr=%d (%s)", addr, lt_ret_verbose(ret));
        handle_session_error(ret);
        return false;
    }

    LOG_I("TR01", "R-Config write OK addr=%d value=0x%08lX", addr, (unsigned long)value);
    return true;
}

bool tropic01_rconfig_erase(void) {
    Tr01Lock lock;
    if (!lock.ok()) return false;
    if (!ensure_session("rconfig_erase")) return false;

    lt_ret_t ret = lt_r_config_erase(&tr01_handle);

    if (ret != LT_OK) {
        LOG_E("TR01", "R-Config erase failed (%s)", lt_ret_verbose(ret));
        handle_session_error(ret);
        return false;
    }

    LOG_I("TR01", "R-Config erased");
    return true;
}
