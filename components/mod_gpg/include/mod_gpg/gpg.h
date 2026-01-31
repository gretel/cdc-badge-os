#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CDC_CURVE_ED25519 0
#define CDC_CURVE_P256    1

#define GPG_USER_ID_MAX         64
#define GPG_FINGERPRINT_LEN     20
#define GPG_FINGERPRINT_V5_LEN  32
#define GPG_PUBKEY_MAX_LEN      64
#define GPG_SIGNATURE_MAX_LEN   64

#define GPG_METADATA_MAGIC      0x4750
#define GPG_METADATA_VERSION    2

typedef struct {
    bool initialized;
    uint8_t curve;
    char user_id[GPG_USER_ID_MAX];
    uint8_t fingerprint[GPG_FINGERPRINT_LEN];
    uint32_t created_at;
    uint32_t sign_count;
} gpg_status_t;

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t version;
    uint8_t curve;
    char user_id[GPG_USER_ID_MAX];
    uint32_t created_at;
    uint8_t fingerprint[GPG_FINGERPRINT_LEN];
    uint8_t pubkey[GPG_PUBKEY_MAX_LEN];
    uint8_t pubkey_len;
    uint32_t sign_count;
    uint8_t fingerprint_v5[GPG_FINGERPRINT_V5_LEN];
} gpg_metadata_t;

bool gpg_init(void);
bool gpg_is_initialized(void);
bool gpg_get_status(gpg_status_t *status);
bool gpg_set_pending_user_id(const char *user_id);
bool gpg_has_pending_user_id(void);
bool gpg_generate_key(uint8_t curve);
bool gpg_reset(void);
bool gpg_export_pubkey_pem(char *buf, size_t size, size_t *out_len);
bool gpg_export_pubkey_raw(uint8_t *pubkey, size_t *pubkey_len, uint8_t *curve);
bool gpg_get_fingerprint(uint8_t *fp_out);
bool gpg_get_fingerprint_v5(uint8_t *fp_out);
bool gpg_sign_hash(const uint8_t *hash, size_t hash_len,
                   uint8_t *sig_out, size_t *sig_len);

#ifdef __cplusplus
}
#endif
