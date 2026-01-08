// CBOR Encoding/Decoding Helpers for CTAP2
// Minimal CBOR implementation for FIDO2

#include "cbor_helpers.h"
#include "cdc_log.h"
#include <string.h>

// ============================================================================
// Writer Implementation
// ============================================================================

void cbor_writer_init(cbor_writer_t *w, uint8_t *buffer, size_t size) {
    w->buffer = buffer;
    w->size = size;
    w->offset = 0;
    w->error = false;
}

size_t cbor_writer_length(const cbor_writer_t *w) {
    return w->offset;
}

bool cbor_writer_error(const cbor_writer_t *w) {
    return w->error;
}

static void write_byte(cbor_writer_t *w, uint8_t b) {
    if (w->error) return;
    if (w->offset >= w->size) {
        w->error = true;
        LOG_E("CBOR", "Write overflow");
        return;
    }
    w->buffer[w->offset++] = b;
}

static void write_bytes(cbor_writer_t *w, const uint8_t *data, size_t len) {
    if (w->error) return;
    if (w->offset + len > w->size) {
        w->error = true;
        LOG_E("CBOR", "Write overflow (need %d, have %d)", len, w->size - w->offset);
        return;
    }
    memcpy(w->buffer + w->offset, data, len);
    w->offset += len;
}

static void write_type_value(cbor_writer_t *w, uint8_t type, uint64_t value) {
    uint8_t major = type << 5;

    if (value < 24) {
        write_byte(w, major | (uint8_t)value);
    } else if (value <= 0xFF) {
        write_byte(w, major | 24);
        write_byte(w, (uint8_t)value);
    } else if (value <= 0xFFFF) {
        write_byte(w, major | 25);
        write_byte(w, (value >> 8) & 0xFF);
        write_byte(w, value & 0xFF);
    } else if (value <= 0xFFFFFFFF) {
        write_byte(w, major | 26);
        write_byte(w, (value >> 24) & 0xFF);
        write_byte(w, (value >> 16) & 0xFF);
        write_byte(w, (value >> 8) & 0xFF);
        write_byte(w, value & 0xFF);
    } else {
        write_byte(w, major | 27);
        write_byte(w, (value >> 56) & 0xFF);
        write_byte(w, (value >> 48) & 0xFF);
        write_byte(w, (value >> 40) & 0xFF);
        write_byte(w, (value >> 32) & 0xFF);
        write_byte(w, (value >> 24) & 0xFF);
        write_byte(w, (value >> 16) & 0xFF);
        write_byte(w, (value >> 8) & 0xFF);
        write_byte(w, value & 0xFF);
    }
}

void cbor_encode_uint(cbor_writer_t *w, uint64_t value) {
    write_type_value(w, CBOR_UNSIGNED, value);
}

void cbor_encode_int(cbor_writer_t *w, int64_t value) {
    if (value >= 0) {
        write_type_value(w, CBOR_UNSIGNED, (uint64_t)value);
    } else {
        write_type_value(w, CBOR_NEGATIVE, (uint64_t)(-1 - value));
    }
}

void cbor_encode_bytes(cbor_writer_t *w, const uint8_t *data, size_t len) {
    write_type_value(w, CBOR_BYTES, len);
    if (len > 0 && data) {
        write_bytes(w, data, len);
    }
}

void cbor_encode_text(cbor_writer_t *w, const char *str) {
    size_t len = str ? strlen(str) : 0;
    write_type_value(w, CBOR_TEXT, len);
    if (len > 0) {
        write_bytes(w, (const uint8_t *)str, len);
    }
}

void cbor_encode_text_len(cbor_writer_t *w, const char *str, size_t len) {
    write_type_value(w, CBOR_TEXT, len);
    if (len > 0 && str) {
        write_bytes(w, (const uint8_t *)str, len);
    }
}

void cbor_encode_bool(cbor_writer_t *w, bool value) {
    write_byte(w, value ? CBOR_TRUE : CBOR_FALSE);
}

void cbor_encode_null(cbor_writer_t *w) {
    write_byte(w, CBOR_NULL);
}

void cbor_encode_array(cbor_writer_t *w, size_t count) {
    write_type_value(w, CBOR_ARRAY, count);
}

void cbor_encode_map(cbor_writer_t *w, size_t count) {
    write_type_value(w, CBOR_MAP, count);
}

void cbor_encode_cose_key_p256(cbor_writer_t *w, const uint8_t *x, const uint8_t *y) {
    // COSE_Key for P-256:
    // {
    //   1: 2,        // kty: EC2
    //   3: -7,       // alg: ES256
    //   -1: 1,       // crv: P-256
    //   -2: x,       // x coordinate (32 bytes)
    //   -3: y        // y coordinate (32 bytes)
    // }
    cbor_encode_map(w, 5);

    cbor_encode_uint(w, 1);    // kty
    cbor_encode_uint(w, 2);    // EC2

    cbor_encode_uint(w, 3);    // alg
    cbor_encode_int(w, -7);    // ES256

    cbor_encode_int(w, -1);    // crv
    cbor_encode_uint(w, 1);    // P-256

    cbor_encode_int(w, -2);    // x
    cbor_encode_bytes(w, x, 32);

    cbor_encode_int(w, -3);    // y
    cbor_encode_bytes(w, y, 32);
}

// ============================================================================
// Reader Implementation
// ============================================================================

void cbor_reader_init(cbor_reader_t *r, const uint8_t *data, size_t size) {
    r->data = data;
    r->size = size;
    r->offset = 0;
    r->error = false;
}

bool cbor_reader_error(const cbor_reader_t *r) {
    return r->error;
}

bool cbor_reader_available(const cbor_reader_t *r) {
    return !r->error && r->offset < r->size;
}

int cbor_reader_peek_type(const cbor_reader_t *r) {
    if (r->error || r->offset >= r->size) return -1;
    return r->data[r->offset] >> 5;
}

static bool read_byte(cbor_reader_t *r, uint8_t *b) {
    if (r->error) return false;
    if (r->offset >= r->size) {
        r->error = true;
        LOG_E("CBOR", "Read underflow");
        return false;
    }
    *b = r->data[r->offset++];
    return true;
}

static bool read_type_value(cbor_reader_t *r, uint8_t *type, uint64_t *value) {
    uint8_t initial;
    if (!read_byte(r, &initial)) return false;

    *type = initial >> 5;
    uint8_t info = initial & 0x1F;

    if (info < 24) {
        *value = info;
    } else if (info == 24) {
        uint8_t b;
        if (!read_byte(r, &b)) return false;
        *value = b;
    } else if (info == 25) {
        uint8_t b[2];
        if (!read_byte(r, &b[0]) || !read_byte(r, &b[1])) return false;
        *value = ((uint16_t)b[0] << 8) | b[1];
    } else if (info == 26) {
        uint8_t b[4];
        for (int i = 0; i < 4; i++) {
            if (!read_byte(r, &b[i])) return false;
        }
        *value = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
                 ((uint32_t)b[2] << 8) | b[3];
    } else if (info == 27) {
        uint8_t b[8];
        for (int i = 0; i < 8; i++) {
            if (!read_byte(r, &b[i])) return false;
        }
        *value = ((uint64_t)b[0] << 56) | ((uint64_t)b[1] << 48) |
                 ((uint64_t)b[2] << 40) | ((uint64_t)b[3] << 32) |
                 ((uint64_t)b[4] << 24) | ((uint64_t)b[5] << 16) |
                 ((uint64_t)b[6] << 8) | b[7];
    } else if (info == 31) {
        // Indefinite length - not supported in CTAP2
        r->error = true;
        LOG_E("CBOR", "Indefinite length not supported");
        return false;
    } else {
        r->error = true;
        LOG_E("CBOR", "Invalid additional info: %d", info);
        return false;
    }

    return true;
}

bool cbor_read_item(cbor_reader_t *r, cbor_item_t *item) {
    uint8_t type;
    uint64_t value;

    if (!read_type_value(r, &type, &value)) return false;

    item->type = type;
    item->value = value;
    item->bytes = NULL;
    item->length = 0;

    // For bytes/text, also read the data pointer
    if (type == CBOR_BYTES || type == CBOR_TEXT) {
        if (r->offset + value > r->size) {
            r->error = true;
            return false;
        }
        item->bytes = r->data + r->offset;
        item->length = value;
        r->offset += value;
    }

    return true;
}

bool cbor_read_uint(cbor_reader_t *r, uint64_t *value) {
    cbor_item_t item;
    if (!cbor_read_item(r, &item)) return false;
    if (item.type != CBOR_UNSIGNED) {
        r->error = true;
        return false;
    }
    *value = item.value;
    return true;
}

bool cbor_read_int(cbor_reader_t *r, int64_t *value) {
    cbor_item_t item;
    if (!cbor_read_item(r, &item)) return false;

    if (item.type == CBOR_UNSIGNED) {
        *value = (int64_t)item.value;
    } else if (item.type == CBOR_NEGATIVE) {
        *value = -1 - (int64_t)item.value;
    } else {
        r->error = true;
        return false;
    }
    return true;
}

bool cbor_read_bytes(cbor_reader_t *r, uint8_t *out, size_t max_len, size_t *out_len) {
    cbor_item_t item;
    if (!cbor_read_item(r, &item)) return false;
    if (item.type != CBOR_BYTES) {
        r->error = true;
        return false;
    }

    size_t copy_len = (item.length < max_len) ? item.length : max_len;
    if (out && copy_len > 0) {
        memcpy(out, item.bytes, copy_len);
    }
    if (out_len) *out_len = item.length;
    return true;
}

bool cbor_read_text(cbor_reader_t *r, char *out, size_t max_len, size_t *out_len) {
    if (max_len == 0) return false;  // Prevent underflow in (max_len - 1)

    cbor_item_t item;
    if (!cbor_read_item(r, &item)) return false;
    if (item.type != CBOR_TEXT) {
        r->error = true;
        return false;
    }

    size_t copy_len = (item.length < max_len - 1) ? item.length : (max_len - 1);
    if (out && copy_len > 0) {
        memcpy(out, item.bytes, copy_len);
    }
    if (out && max_len > 0) {
        out[copy_len] = '\0';
    }
    if (out_len) *out_len = item.length;
    return true;
}

bool cbor_read_bool(cbor_reader_t *r, bool *value) {
    uint8_t b;
    if (!read_byte(r, &b)) return false;

    if (b == CBOR_FALSE) {
        *value = false;
        return true;
    } else if (b == CBOR_TRUE) {
        *value = true;
        return true;
    }

    r->error = true;
    return false;
}

int cbor_read_map(cbor_reader_t *r) {
    cbor_item_t item;
    if (!cbor_read_item(r, &item)) return -1;
    if (item.type != CBOR_MAP) {
        r->error = true;
        return -1;
    }
    return (int)item.value;
}

int cbor_read_array(cbor_reader_t *r) {
    cbor_item_t item;
    if (!cbor_read_item(r, &item)) return -1;
    if (item.type != CBOR_ARRAY) {
        r->error = true;
        return -1;
    }
    return (int)item.value;
}

// Security limits for CBOR parsing
#define CBOR_MAX_RECURSION_DEPTH 8
#define CBOR_MAX_CONTAINER_SIZE  256

static bool cbor_skip_item_impl(cbor_reader_t *r, uint8_t depth) {
    if (depth > CBOR_MAX_RECURSION_DEPTH) {
        LOG_W("CBOR", "Max recursion depth exceeded");
        return false;
    }

    cbor_item_t item;
    if (!cbor_read_item(r, &item)) return false;

    // For containers, skip all nested items with limits
    if (item.type == CBOR_ARRAY) {
        if (item.value > CBOR_MAX_CONTAINER_SIZE) {
            LOG_W("CBOR", "Array too large: %llu", (unsigned long long)item.value);
            return false;
        }
        for (uint64_t i = 0; i < item.value; i++) {
            if (!cbor_skip_item_impl(r, depth + 1)) return false;
        }
    } else if (item.type == CBOR_MAP) {
        if (item.value > CBOR_MAX_CONTAINER_SIZE) {
            LOG_W("CBOR", "Map too large: %llu", (unsigned long long)item.value);
            return false;
        }
        for (uint64_t i = 0; i < item.value * 2; i++) {
            if (!cbor_skip_item_impl(r, depth + 1)) return false;
        }
    }
    // Bytes/text data already consumed by read_item

    return true;
}

bool cbor_skip_item(cbor_reader_t *r) {
    return cbor_skip_item_impl(r, 0);
}

bool cbor_parse_cose_key(cbor_reader_t *r, int *kty, int *alg,
                          uint8_t *x, uint8_t *y) {
    int count = cbor_read_map(r);
    if (count < 0) return false;

    *kty = 0;
    *alg = 0;

    for (int i = 0; i < count; i++) {
        int64_t key;
        if (!cbor_read_int(r, &key)) return false;

        switch (key) {
            case 1:  // kty
                {
                    uint64_t v;
                    if (!cbor_read_uint(r, &v)) return false;
                    *kty = (int)v;
                }
                break;
            case 3:  // alg
                {
                    int64_t v;
                    if (!cbor_read_int(r, &v)) return false;
                    *alg = (int)v;
                }
                break;
            case -2:  // x coordinate
                {
                    size_t len;
                    if (!cbor_read_bytes(r, x, 32, &len)) return false;
                }
                break;
            case -3:  // y coordinate
                if (y) {
                    size_t len;
                    if (!cbor_read_bytes(r, y, 32, &len)) return false;
                }
                break;
            default:
                // Skip unknown keys
                if (!cbor_skip_item(r)) return false;
                break;
        }
    }

    return true;
}
