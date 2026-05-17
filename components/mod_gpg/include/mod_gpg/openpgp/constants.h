/**
 * \brief Centralized magic-value constants for the OpenPGP smart-card application.
 *
 * Collects RFC 4880 algorithm identifiers, MPI sizes, OpenPGP card password
 * reference codes, and curve specific cryptographic key/digest sizes that are
 * used across the OpenPGP implementation in `mod_gpg`.
 *
 * References:
 * - OpenPGP RFC 4880 Section 9.1: Public-Key Algorithm IDs
 * - OpenPGP Smart Card Application 3.4.1, Section 4.4 / Section 6
 * - SEC 1: Elliptic Curve Cryptography (uncompressed point format)
 * - FIPS 180-4: SHA-256 specification
 */

#ifndef MOD_GPG_OPENPGP_CONSTANTS_H
#define MOD_GPG_OPENPGP_CONSTANTS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* === RFC 4880 Public-Key Algorithm Identifiers === */

/** \brief OpenPGP algorithm ID for ECDSA (RFC 4880 Section 9.1). */
#define OPENPGP_ALGO_ECDSA 19

/** \brief OpenPGP algorithm ID for EdDSA (Ed25519). */
#define OPENPGP_ALGO_EDDSA 22

/* === Fingerprint and digest sizes === */

/** \brief OpenPGP v4 fingerprint size (SHA-1 digest length, in bytes). */
#define OPENPGP_FINGERPRINT_SIZE 20

/** \brief SHA-256 digest output size in bytes (FIPS 180-4). */
#define SHA256_DIGEST_SIZE 32

/* === P-256 (secp256r1) sizes === */

/** \brief P-256 uncompressed public key size: 0x04 || X(32) || Y(32). */
#define P256_PUBKEY_SIZE 65

/** \brief P-256 private key (scalar) size in bytes. */
#define P256_PRIVKEY_SIZE 32

/** \brief P-256 ECDH shared secret size in bytes. */
#define P256_ECDH_SECRET_SIZE 32

/** \brief P-256 public key bit-length (used as MPI bit count). */
#define P256_PUBKEY_BITS 520

/* === Ed25519 sizes === */

/** \brief Ed25519 raw public key size in bytes. */
#define ED25519_PUBKEY_SIZE 32

/* === OpenPGP MPI sizes (RFC 4880 Section 3.2: 2-byte length prefix + payload) === */

/** \brief OpenPGP MPI header size (2-byte length prefix). */
#define MPI_HEADER_SIZE 2

/** \brief Full Ed25519 MPI buffer size: 2 byte length prefix + 32 byte key. */
#define MPI_FULL_SIZE_ED25519 (MPI_HEADER_SIZE + ED25519_PUBKEY_SIZE)

/** \brief Full P-256 MPI buffer size: 2 byte length prefix + 65 byte uncompressed key. */
#define MPI_FULL_SIZE_P256 (MPI_HEADER_SIZE + P256_PUBKEY_SIZE)

/* === OpenPGP Smart Card password reference codes (Spec 3.4.1, Section 7.2.2) === */

/** \brief PW1 reference for signature operations (User PIN). */
#define PW1_CODE_1 0x81

/** \brief PW1 reference for non-signature operations (User PIN, alt). */
#define PW1_CODE_2 0x82

/** \brief PW3 reference (Admin PIN). */
#define PW3_CODE 0x83

#ifdef __cplusplus
}
#endif

#endif /* MOD_GPG_OPENPGP_CONSTANTS_H */
