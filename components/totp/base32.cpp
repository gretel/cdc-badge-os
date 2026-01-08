// Base32 Decoder (RFC 4648)

#include "base32.h"
#include "cdc_log.h"
#include <string.h>
#include <ctype.h>

// Decode a single Base32 character to its 5-bit value
// Returns -1 for invalid characters
static int base32_char_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a';  // Accept lowercase
    if (c >= '2' && c <= '7') return c - '2' + 26;
    return -1;  // Invalid
}

size_t base32_decoded_size(const char *encoded) {
    if (!encoded) return 0;

    // Count valid Base32 characters (skip spaces and dashes)
    size_t count = 0;
    for (const char *p = encoded; *p; p++) {
        if (base32_char_value(*p) >= 0) {
            count++;
        } else if (*p != ' ' && *p != '-' && *p != '=') {
            // Invalid character (not space, dash, or padding)
            return 0;
        }
    }

    // 5 bits per character, 8 bits per output byte
    // Every 8 characters = 40 bits = 5 bytes
    return (count * 5) / 8;
}

int base32_decode(const char *encoded, uint8_t *out, size_t outMax) {
    if (!encoded || !out || outMax == 0) {
        return -1;
    }

    size_t outIdx = 0;
    uint32_t buffer = 0;
    int bitsLeft = 0;

    for (const char *p = encoded; *p; p++) {
        // Skip spaces, dashes, and padding
        if (*p == ' ' || *p == '-' || *p == '=') {
            continue;
        }

        int value = base32_char_value(*p);
        if (value < 0) {
            LOG_W("BASE32", "Invalid char '%c' (0x%02X)", *p, (uint8_t)*p);
            return -1;
        }

        // Add 5 bits to buffer
        buffer = (buffer << 5) | value;
        bitsLeft += 5;

        // Extract complete bytes
        if (bitsLeft >= 8) {
            if (outIdx >= outMax) {
                LOG_W("BASE32", "Output buffer too small");
                return -1;
            }
            bitsLeft -= 8;
            out[outIdx++] = (buffer >> bitsLeft) & 0xFF;
        }
    }

    LOG_D("BASE32", "Decoded %d bytes", outIdx);
    return outIdx;
}

// Base32 alphabet (RFC 4648)
static const char BASE32_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

int base32_encode(const uint8_t *data, size_t dataLen, char *out, size_t outMax) {
    if (!data || !out || outMax == 0) {
        return -1;
    }

    // Calculate required output size: ceil(dataLen * 8 / 5) + null terminator
    size_t requiredLen = ((dataLen * 8 + 4) / 5) + 1;
    if (outMax < requiredLen) {
        return -1;
    }

    size_t outIdx = 0;
    uint32_t buffer = 0;
    int bitsLeft = 0;

    for (size_t i = 0; i < dataLen; i++) {
        buffer = (buffer << 8) | data[i];
        bitsLeft += 8;

        while (bitsLeft >= 5) {
            bitsLeft -= 5;
            out[outIdx++] = BASE32_ALPHABET[(buffer >> bitsLeft) & 0x1F];
        }
    }

    // Handle remaining bits (pad with zeros)
    if (bitsLeft > 0) {
        out[outIdx++] = BASE32_ALPHABET[(buffer << (5 - bitsLeft)) & 0x1F];
    }

    out[outIdx] = '\0';
    return outIdx;
}
