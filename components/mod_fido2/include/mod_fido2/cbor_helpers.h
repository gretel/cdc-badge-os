// CBOR Encoding/Decoding Helpers for CTAP2
// Minimal CBOR implementation for FIDO2

#ifndef CBOR_HELPERS_H
#define CBOR_HELPERS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// CBOR Major Types
// ============================================================================

#define CBOR_UNSIGNED   0   // 0x00-0x1F
#define CBOR_NEGATIVE   1   // 0x20-0x3F
#define CBOR_BYTES      2   // 0x40-0x5F
#define CBOR_TEXT       3   // 0x60-0x7F
#define CBOR_ARRAY      4   // 0x80-0x9F
#define CBOR_MAP        5   // 0xA0-0xBF
#define CBOR_TAG        6   // 0xC0-0xDF
#define CBOR_SIMPLE     7   // 0xE0-0xFF

// Simple values
#define CBOR_FALSE      0xF4
#define CBOR_TRUE       0xF5
#define CBOR_NULL       0xF6
#define CBOR_UNDEFINED  0xF7

// ============================================================================
// CBOR Writer
// ============================================================================

#ifdef __DOXYGEN__
namespace cdc::mod_fido2 {
#endif

typedef struct {
    uint8_t *buffer;
    size_t size;
    size_t offset;
    bool error;
} cbor_writer_t;

/**
 * Initialize CBOR writer.
 *
 * @param w Writer context
 * @param buffer Output buffer
 * @param size Buffer size
 */
void cbor_writer_init(cbor_writer_t *w, uint8_t *buffer, size_t size);

/**
 * Get current written length.
 */
size_t cbor_writer_length(const cbor_writer_t *w);

/**
 * Check for write errors.
 */
bool cbor_writer_error(const cbor_writer_t *w);

// Encode primitives
void cbor_encode_uint(cbor_writer_t *w, uint64_t value);
void cbor_encode_int(cbor_writer_t *w, int64_t value);
void cbor_encode_bytes(cbor_writer_t *w, const uint8_t *data, size_t len);
void cbor_encode_text(cbor_writer_t *w, const char *str);
void cbor_encode_text_len(cbor_writer_t *w, const char *str, size_t len);
void cbor_encode_bool(cbor_writer_t *w, bool value);
void cbor_encode_null(cbor_writer_t *w);

// Encode containers (write header, then encode items)
void cbor_encode_array(cbor_writer_t *w, size_t count);
void cbor_encode_map(cbor_writer_t *w, size_t count);

// Encode COSE key (P-256 public key)
void cbor_encode_cose_key_p256(cbor_writer_t *w, const uint8_t *x, const uint8_t *y);

// Encode COSE key (Ed25519 public key)
// COSE_Key format for OKP (Octet Key Pair):
// 1 (kty): 1 (OKP)
// 3 (alg): -8 (EdDSA)
// -1 (crv): 6 (Ed25519)
// -2 (x): 32-byte public key
void cbor_encode_cose_key_ed25519(cbor_writer_t *w, const uint8_t *pubkey);

// ============================================================================
// CBOR Reader
// ============================================================================

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t offset;
    bool error;
} cbor_reader_t;

typedef struct {
    uint8_t type;       // Major type
    uint64_t value;     // Argument value (or length for containers)
    const uint8_t *bytes;   // Pointer to byte/text data
    size_t length;      // Length of byte/text data
} cbor_item_t;

#ifdef __DOXYGEN__
} // namespace cdc::mod_fido2
#endif

/**
 * Initialize CBOR reader.
 *
 * @param r Reader context
 * @param data Input data
 * @param size Data size
 */
void cbor_reader_init(cbor_reader_t *r, const uint8_t *data, size_t size);

/**
 * Check for read errors.
 */
bool cbor_reader_error(const cbor_reader_t *r);

/**
 * Check if more data available.
 */
bool cbor_reader_available(const cbor_reader_t *r);

/**
 * Peek at next item type without consuming.
 *
 * @return Major type (0-7) or -1 on error
 */
int cbor_reader_peek_type(const cbor_reader_t *r);

/**
 * Read next CBOR item.
 *
 * @param r Reader context
 * @param item Output item
 * @return true on success
 */
bool cbor_read_item(cbor_reader_t *r, cbor_item_t *item);

/**
 * Read unsigned integer.
 */
bool cbor_read_uint(cbor_reader_t *r, uint64_t *value);

/**
 * Read signed integer.
 */
bool cbor_read_int(cbor_reader_t *r, int64_t *value);

/**
 * Read byte string.
 *
 * @param r Reader context
 * @param out Output buffer
 * @param max_len Maximum length
 * @param out_len Actual length
 * @return true on success
 */
bool cbor_read_bytes(cbor_reader_t *r, uint8_t *out, size_t max_len, size_t *out_len);

/**
 * Read text string.
 *
 * @param r Reader context
 * @param out Output buffer (null-terminated)
 * @param max_len Maximum length including null
 * @param out_len Actual length excluding null
 * @return true on success
 */
bool cbor_read_text(cbor_reader_t *r, char *out, size_t max_len, size_t *out_len);

/**
 * Read boolean value.
 */
bool cbor_read_bool(cbor_reader_t *r, bool *value);

/**
 * Read map header and return item count.
 *
 * @return Item count, or -1 on error
 */
int cbor_read_map(cbor_reader_t *r);

/**
 * Read array header and return item count.
 *
 * @return Item count, or -1 on error
 */
int cbor_read_array(cbor_reader_t *r);

/**
 * Skip the next CBOR item (including nested containers).
 */
bool cbor_skip_item(cbor_reader_t *r);

// ============================================================================
// COSE Key Parsing
// ============================================================================

/**
 * Parse COSE_Key from CBOR.
 *
 * @param r Reader context (positioned at key map)
 * @param kty Output: key type (1=OKP, 2=EC2)
 * @param alg Output: algorithm
 * @param x Output: X coordinate (32 bytes)
 * @param y Output: Y coordinate (32 bytes, NULL for OKP)
 * @return true on success
 */
bool cbor_parse_cose_key(cbor_reader_t *r, int *kty, int *alg,
                          uint8_t *x, uint8_t *y);

#ifdef __cplusplus
}
#endif

#endif // CBOR_HELPERS_H
