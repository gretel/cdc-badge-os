/**
 * ECDH P-256 Implementation for OpenPGP PSO:DECIPHER
 *
 * SECURITY NOTE:
 * The TROPIC01 secure element does NOT support native ECDH operations.
 * This module provides software-based ECDH using MbedTLS with the private key
 * temporarily loaded from encrypted R-Memory storage.
 *
 * See docs/GPG_ECDH_SECURITY.md for security analysis and trade-offs.
 */

#include "ecdh.h"
#include <mbedtls/ecdh.h>
#include <mbedtls/ecp.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/platform_util.h>
#include <esp_random.h>
#include <string.h>

/**
 * \brief MbedTLS 3.x/4.x compatibility macros for ECP point field access.
 */
#if defined(MBEDTLS_PRIVATE)
#define ECP_POINT_X(P) (P).MBEDTLS_PRIVATE(X)
#define ECP_POINT_Y(P) (P).MBEDTLS_PRIVATE(Y)
#define ECP_POINT_Z(P) (P).MBEDTLS_PRIVATE(Z)
#else
#define ECP_POINT_X(P) (P).X
#define ECP_POINT_Y(P) (P).Y
#define ECP_POINT_Z(P) (P).Z
#endif

/**
 * \brief Hardware RNG callback used by MbedTLS.
 * \param ctx Unused callback context.
 * \param buf Output buffer to fill with random bytes.
 * \param len Number of random bytes requested.
 * \return `0` on success.
 */
static int hw_random(void* ctx, unsigned char* buf, size_t len) {
    (void)ctx;
    esp_fill_random(buf, len);
    return 0;
}

/**
 * \brief Securely clears sensitive memory using platform zeroize.
 * \param ptr Memory region start.
 * \param size Number of bytes to clear.
 */
void ecdh_secure_clear(void* ptr, size_t size) {
    mbedtls_platform_zeroize(ptr, size);
}

/**
 * \brief Computes ECDH shared secret on P-256 using local private key and peer public key.
 * \param privkey In/out private key buffer (cleared on completion/failure).
 * \param peer_pubkey Peer uncompressed public key (`0x04 || X || Y`).
 * \param shared_out Output 32-byte shared secret (`X` coordinate).
 * \return `true` if shared secret derivation succeeded.
 */
bool ecdh_p256_compute_shared_secret(
    uint8_t* privkey,
    const uint8_t* peer_pubkey,
    uint8_t* shared_out
) {
    if (!privkey || !peer_pubkey || !shared_out) {
        return false;
    }

    // Verify uncompressed point format (0x04 prefix)
    if (peer_pubkey[0] != 0x04) {
        ecdh_secure_clear(privkey, 32);
        return false;
    }

    mbedtls_ecp_group grp;
    mbedtls_mpi d;       // Private key scalar
    mbedtls_ecp_point Q; // Peer public key point
    mbedtls_ecp_point S; // Shared secret point

    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_ecp_point_init(&Q);
    mbedtls_ecp_point_init(&S);

    bool success = false;
    int ret;

    // Load P-256 curve parameters
    ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) {
        goto cleanup;
    }

    // Import private key scalar
    ret = mbedtls_mpi_read_binary(&d, privkey, 32);
    if (ret != 0) {
        goto cleanup;
    }

    // Import peer public key point (skip 0x04 prefix)
    ret = mbedtls_mpi_read_binary(&ECP_POINT_X(Q), peer_pubkey + 1, 32);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_mpi_read_binary(&ECP_POINT_Y(Q), peer_pubkey + 33, 32);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_mpi_lset(&ECP_POINT_Z(Q), 1);
    if (ret != 0) {
        goto cleanup;
    }

    // Validate peer public key is on curve
    ret = mbedtls_ecp_check_pubkey(&grp, &Q);
    if (ret != 0) {
        goto cleanup;
    }

    // Compute ECDH: S = d * Q
    // Using constant-time scalar multiplication
    ret = mbedtls_ecp_mul(&grp, &S, &d, &Q, hw_random, NULL);
    if (ret != 0) {
        goto cleanup;
    }

    // Extract X coordinate as shared secret (32 bytes)
    ret = mbedtls_mpi_write_binary(&ECP_POINT_X(S), shared_out, 32);
    if (ret != 0) {
        goto cleanup;
    }

    success = true;

cleanup:
    // Securely clear all sensitive data
    mbedtls_ecp_group_free(&grp);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_point_free(&Q);
    mbedtls_ecp_point_free(&S);

    // ALWAYS clear the private key from caller's buffer
    ecdh_secure_clear(privkey, 32);

    return success;
}

bool ecdh_p256_generate_keypair(uint8_t* privkey_out, uint8_t* pubkey_out) {
    if (!privkey_out || !pubkey_out) {
        return false;
    }

    mbedtls_ecp_group grp;
    mbedtls_mpi d;
    mbedtls_ecp_point Q;

    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_ecp_point_init(&Q);

    bool success = false;
    int ret;

    // Load P-256 curve
    ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) {
        goto cleanup;
    }

    // Generate key pair
    ret = mbedtls_ecp_gen_keypair(&grp, &d, &Q, hw_random, NULL);
    if (ret != 0) {
        goto cleanup;
    }

    // Export private key
    ret = mbedtls_mpi_write_binary(&d, privkey_out, 32);
    if (ret != 0) {
        goto cleanup;
    }

    // Export public key in uncompressed format (04 || X || Y)
    pubkey_out[0] = 0x04;
    ret = mbedtls_mpi_write_binary(&ECP_POINT_X(Q), pubkey_out + 1, 32);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_mpi_write_binary(&ECP_POINT_Y(Q), pubkey_out + 33, 32);
    if (ret != 0) {
        goto cleanup;
    }

    success = true;

cleanup:
    mbedtls_ecp_group_free(&grp);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_point_free(&Q);

    if (!success) {
        ecdh_secure_clear(privkey_out, 32);
        ecdh_secure_clear(pubkey_out, 65);
    }

    return success;
}

bool ecdh_p256_derive_pubkey(const uint8_t* privkey, uint8_t* pubkey_out) {
    if (!privkey || !pubkey_out) {
        return false;
    }

    mbedtls_ecp_group grp;
    mbedtls_mpi d;
    mbedtls_ecp_point Q;

    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_ecp_point_init(&Q);

    bool success = false;
    int ret;

    // Load P-256 curve
    ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) {
        goto cleanup;
    }

    // Import private key
    ret = mbedtls_mpi_read_binary(&d, privkey, 32);
    if (ret != 0) {
        goto cleanup;
    }

    // Compute public key: Q = d * G
    ret = mbedtls_ecp_mul(&grp, &Q, &d, &grp.G, hw_random, NULL);
    if (ret != 0) {
        goto cleanup;
    }

    // Export in uncompressed format
    pubkey_out[0] = 0x04;
    ret = mbedtls_mpi_write_binary(&ECP_POINT_X(Q), pubkey_out + 1, 32);
    if (ret != 0) {
        goto cleanup;
    }

    ret = mbedtls_mpi_write_binary(&ECP_POINT_Y(Q), pubkey_out + 33, 32);
    if (ret != 0) {
        goto cleanup;
    }

    success = true;

cleanup:
    mbedtls_ecp_group_free(&grp);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_point_free(&Q);

    return success;
}
