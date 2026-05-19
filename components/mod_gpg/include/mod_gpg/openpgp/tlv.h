/**
 * \brief BER-TLV codec for OpenPGP Data Objects.
 *
 * OpenPGP smart cards use the BER-TLV encoding from ISO 7816-4 Annex D for
 * every Data Object exchanged via GET DATA / PUT DATA:
 *
 *   - **Tag**:    1 or 2 bytes. A 1-byte tag is restricted to values whose
 *                 lower five bits are not all set; a 2-byte tag uses bits
 *                 1..5 of the first byte = b11111 to indicate continuation.
 *                 (See ISO 7816-4, Annex D, 5.4.)
 *   - **Length**: BER definite length. Values < 128 use a single byte
 *                 directly; longer values use the 0x8x prefix where x is the
 *                 number of subsequent big-endian length bytes. OpenPGP
 *                 spec 3.4.1 uses at most 3-byte forms (0x82 LH LL → up to
 *                 64 KiB).
 *   - **Value**:  Raw bytes, of length given by the length field.
 *
 * The helpers in this header cover encoding and parsing for the subset used
 * by OpenPGP (1-/2-byte tags, 1-/2-/3-byte length).
 */

#ifndef MOD_GPG_OPENPGP_TLV_H
#define MOD_GPG_OPENPGP_TLV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Status of a TLV operation. */
typedef enum {
    TLV_OK = 0,
    TLV_ERR_BUF_TOO_SMALL,  /**< Output / input buffer exhausted before the field could be read or written. */
    TLV_ERR_BAD_LENGTH,     /**< Length field uses a form we do not support (e.g. > 3 bytes total). */
    TLV_ERR_BAD_TAG,        /**< Tag bytes violate ISO 7816-4 continuation rules. */
    TLV_ERR_NULL,           /**< NULL argument where forbidden. */
} tlv_status_t;

/**
 * \brief Compute the encoded size of a tag.
 * \param tag Tag value.
 * \return 1 if the tag fits in a single byte, otherwise 2.
 */
size_t tlv_tag_size(uint16_t tag);

/**
 * \brief Compute the BER definite-length encoded size for a length value.
 * \param value_len Value length.
 * \return Encoded length-field width (1, 2 or 3 bytes).
 */
size_t tlv_len_size(size_t value_len);

/**
 * \brief Encode a tag. Writes either one or two bytes depending on the
 *        magnitude of \p tag.
 *
 * \param buf Output buffer.
 * \param buf_max Buffer capacity.
 * \param tag Tag value.
 * \param written Receives the number of bytes written.
 */
tlv_status_t tlv_write_tag(uint8_t *buf, size_t buf_max, uint16_t tag,
                           size_t *written);

/**
 * \brief Encode a BER definite length field.
 *
 * Supports value lengths 0..65535 (≤ 64 KiB minus 1). Lengths beyond that
 * are rejected with TLV_ERR_BAD_LENGTH because OpenPGP 3.4.1 never asks for
 * more, and ESP32 buffers are far below that.
 *
 * \param buf Output buffer.
 * \param buf_max Buffer capacity.
 * \param value_len Value length to encode.
 * \param written Receives the number of bytes written.
 */
tlv_status_t tlv_write_len(uint8_t *buf, size_t buf_max, size_t value_len,
                           size_t *written);

/**
 * \brief Build a complete TLV: tag, length, and value (which may be NULL when
 *        \p value_len == 0).
 *
 * \param buf Output buffer.
 * \param buf_max Buffer capacity.
 * \param tag Tag value.
 * \param value Optional value bytes.
 * \param value_len Value length.
 * \param written Receives the total number of bytes written.
 */
tlv_status_t tlv_build(uint8_t *buf, size_t buf_max, uint16_t tag,
                       const uint8_t *value, size_t value_len, size_t *written);

/**
 * \brief Read a tag from the input buffer at \p pos.
 *
 * Advances \p pos past the tag. Two-byte tags are required to follow the
 * ISO 7816-4 continuation rule (low five bits of the first byte = 0x1F).
 *
 * \param buf Input buffer.
 * \param buf_len Available bytes in the buffer.
 * \param pos In/out byte index.
 * \param tag_out Receives the parsed tag value.
 */
tlv_status_t tlv_read_tag(const uint8_t *buf, size_t buf_len, size_t *pos,
                          uint16_t *tag_out);

/**
 * \brief Read a BER definite length field starting at \p pos.
 *
 * Accepts the short form (\p first_byte < 128) and the 0x81 / 0x82 long forms.
 *
 * \param buf Input buffer.
 * \param buf_len Available bytes in the buffer.
 * \param pos In/out byte index.
 * \param length_out Receives the parsed length value.
 */
tlv_status_t tlv_read_len(const uint8_t *buf, size_t buf_len, size_t *pos,
                          size_t *length_out);

/** \brief Parsed TLV: pointers alias into the caller-supplied buffer. */
typedef struct {
    uint16_t tag;
    size_t   length;
    const uint8_t *value;   /**< Points into the input buffer; NULL if length == 0. */
    size_t   total_size;    /**< Number of bytes the entire TLV consumed. */
} tlv_t;

/**
 * \brief Parse a single TLV starting at \p pos. On success \p pos advances
 *        past the entire field and \p out is filled.
 *
 * The function rejects truncated values (length field claims more bytes than
 * the buffer holds). It is safe to chain calls in a loop to walk a sequence
 * of TLVs.
 */
tlv_status_t tlv_parse(const uint8_t *buf, size_t buf_len, size_t *pos, tlv_t *out);

#ifdef __cplusplus
}
#endif

#endif  // MOD_GPG_OPENPGP_TLV_H
