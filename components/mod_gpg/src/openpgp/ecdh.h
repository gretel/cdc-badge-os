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

#ifndef OPENPGP_ECDH_H
#define OPENPGP_ECDH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Compute ECDH shared secret using P-256 curve
 *
 * @param privkey     32-byte private key scalar (will be cleared on return)
 * @param peer_pubkey 65-byte peer public key (04 || X || Y uncompressed format)
 * @param shared_out  32-byte output buffer for shared secret
 * @return true on success, false on error
 *
 * SECURITY: The private key buffer is zeroed after use regardless of success/failure.
 *           Uses MbedTLS constant-time ECC implementation.
 */
bool ecdh_p256_compute_shared_secret(
    uint8_t* privkey,           // 32 bytes, WILL BE CLEARED
    const uint8_t* peer_pubkey, // 65 bytes (04||X||Y)
    uint8_t* shared_out         // 32 bytes
);

/**
 * Generate ephemeral P-256 key pair for ECDH
 *
 * @param privkey_out 32-byte output for private key
 * @param pubkey_out  65-byte output for public key (04 || X || Y)
 * @return true on success
 */
bool ecdh_p256_generate_keypair(
    uint8_t* privkey_out,  // 32 bytes
    uint8_t* pubkey_out    // 65 bytes
);

/**
 * Derive public key from private key
 *
 * @param privkey    32-byte private key
 * @param pubkey_out 65-byte output for public key (04 || X || Y)
 * @return true on success
 */
bool ecdh_p256_derive_pubkey(
    const uint8_t* privkey,  // 32 bytes
    uint8_t* pubkey_out      // 65 bytes
);

/**
 * Securely clear sensitive memory
 * Uses volatile writes to prevent compiler optimization
 *
 * @param ptr  Pointer to memory
 * @param size Number of bytes to clear
 */
void ecdh_secure_clear(void* ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif // OPENPGP_ECDH_H
