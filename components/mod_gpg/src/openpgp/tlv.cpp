/**
 * \brief BER-TLV codec implementation (see tlv.h).
 */

#include "mod_gpg/openpgp/tlv.h"

#include <string.h>

size_t tlv_tag_size(uint16_t tag) {
    return (tag > 0xFF) ? 2 : 1;
}

size_t tlv_len_size(size_t value_len) {
    if (value_len < 0x80) return 1;
    if (value_len <= 0xFF) return 2;
    return 3;
}

tlv_status_t tlv_write_tag(uint8_t *buf, size_t buf_max, uint16_t tag,
                           size_t *written) {
    if (!buf || !written) return TLV_ERR_NULL;
    const size_t need = tlv_tag_size(tag);
    if (buf_max < need) return TLV_ERR_BUF_TOO_SMALL;
    if (need == 2) {
        buf[0] = static_cast<uint8_t>((tag >> 8) & 0xFF);
        buf[1] = static_cast<uint8_t>(tag & 0xFF);
    } else {
        buf[0] = static_cast<uint8_t>(tag & 0xFF);
    }
    *written = need;
    return TLV_OK;
}

tlv_status_t tlv_write_len(uint8_t *buf, size_t buf_max, size_t value_len,
                           size_t *written) {
    if (!buf || !written) return TLV_ERR_NULL;
    if (value_len > 0xFFFF) return TLV_ERR_BAD_LENGTH;
    const size_t need = tlv_len_size(value_len);
    if (buf_max < need) return TLV_ERR_BUF_TOO_SMALL;
    if (need == 1) {
        buf[0] = static_cast<uint8_t>(value_len);
    } else if (need == 2) {
        buf[0] = 0x81;
        buf[1] = static_cast<uint8_t>(value_len);
    } else {
        buf[0] = 0x82;
        buf[1] = static_cast<uint8_t>((value_len >> 8) & 0xFF);
        buf[2] = static_cast<uint8_t>(value_len & 0xFF);
    }
    *written = need;
    return TLV_OK;
}

tlv_status_t tlv_build(uint8_t *buf, size_t buf_max, uint16_t tag,
                       const uint8_t *value, size_t value_len, size_t *written) {
    if (!buf || !written) return TLV_ERR_NULL;
    if (value_len > 0 && !value) return TLV_ERR_NULL;

    size_t pos = 0;
    size_t n = 0;
    tlv_status_t st = tlv_write_tag(buf, buf_max, tag, &n);
    if (st != TLV_OK) return st;
    pos += n;
    st = tlv_write_len(buf + pos, buf_max - pos, value_len, &n);
    if (st != TLV_OK) return st;
    pos += n;
    if (value_len > 0) {
        if (buf_max - pos < value_len) return TLV_ERR_BUF_TOO_SMALL;
        memcpy(buf + pos, value, value_len);
        pos += value_len;
    }
    *written = pos;
    return TLV_OK;
}

tlv_status_t tlv_read_tag(const uint8_t *buf, size_t buf_len, size_t *pos,
                          uint16_t *tag_out) {
    if (!buf || !pos || !tag_out) return TLV_ERR_NULL;
    if (*pos >= buf_len) return TLV_ERR_BUF_TOO_SMALL;

    const uint8_t b0 = buf[*pos];
    // ISO 7816-4 Annex D: if bits 1..5 of the first tag byte are all set,
    // a second tag byte follows.
    if ((b0 & 0x1F) == 0x1F) {
        if (*pos + 1 >= buf_len) return TLV_ERR_BUF_TOO_SMALL;
        const uint8_t b1 = buf[*pos + 1];
        // OpenPGP tags fit in two bytes; reject multi-byte continuation.
        if (b1 & 0x80) return TLV_ERR_BAD_TAG;
        *tag_out = static_cast<uint16_t>((b0 << 8) | b1);
        *pos += 2;
    } else {
        *tag_out = b0;
        *pos += 1;
    }
    return TLV_OK;
}

tlv_status_t tlv_read_len(const uint8_t *buf, size_t buf_len, size_t *pos,
                          size_t *length_out) {
    if (!buf || !pos || !length_out) return TLV_ERR_NULL;
    if (*pos >= buf_len) return TLV_ERR_BUF_TOO_SMALL;

    const uint8_t first = buf[*pos];
    if (first < 0x80) {
        *length_out = first;
        *pos += 1;
        return TLV_OK;
    }
    const uint8_t n = first & 0x7F;
    if (n == 0 || n > 2) return TLV_ERR_BAD_LENGTH;  // OpenPGP uses at most 0x82
    if (*pos + 1 + n > buf_len) return TLV_ERR_BUF_TOO_SMALL;

    size_t length = 0;
    for (uint8_t i = 0; i < n; ++i) {
        length = (length << 8) | buf[*pos + 1 + i];
    }
    *length_out = length;
    *pos += 1 + n;
    return TLV_OK;
}

tlv_status_t tlv_parse(const uint8_t *buf, size_t buf_len, size_t *pos, tlv_t *out) {
    if (!buf || !pos || !out) return TLV_ERR_NULL;
    const size_t start = *pos;

    uint16_t tag = 0;
    tlv_status_t st = tlv_read_tag(buf, buf_len, pos, &tag);
    if (st != TLV_OK) return st;

    size_t length = 0;
    st = tlv_read_len(buf, buf_len, pos, &length);
    if (st != TLV_OK) return st;

    if (*pos + length > buf_len) return TLV_ERR_BUF_TOO_SMALL;

    out->tag = tag;
    out->length = length;
    out->value = (length > 0) ? buf + *pos : nullptr;
    *pos += length;
    out->total_size = *pos - start;
    return TLV_OK;
}
