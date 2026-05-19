/**
 * \file
 * \brief Big-endian byte-packing helpers.
 */

#ifndef CDC_CORE_BYTES_H
#define CDC_CORE_BYTES_H

#include <cstdint>

namespace cdc {
namespace core {

/**
 * \brief Writes a 32-bit value to a buffer in big-endian order.
 * \param out Destination buffer of at least four bytes.
 * \param v Value to encode.
 */
inline void writeBe32(uint8_t* out, uint32_t v) {
    out[0] = static_cast<uint8_t>((v >> 24) & 0xFF);
    out[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
    out[2] = static_cast<uint8_t>((v >> 8) & 0xFF);
    out[3] = static_cast<uint8_t>(v & 0xFF);
}

/**
 * \brief Reads a 32-bit value from a buffer in big-endian order.
 * \param in Source buffer of at least four bytes.
 * \return Decoded value.
 */
inline uint32_t readBe32(const uint8_t* in) {
    return (static_cast<uint32_t>(in[0]) << 24) |
           (static_cast<uint32_t>(in[1]) << 16) |
           (static_cast<uint32_t>(in[2]) << 8) |
            static_cast<uint32_t>(in[3]);
}

/**
 * \brief Writes a 16-bit value to a buffer in big-endian order.
 * \param out Destination buffer of at least two bytes.
 * \param v Value to encode.
 */
inline void writeBe16(uint8_t* out, uint16_t v) {
    out[0] = static_cast<uint8_t>((v >> 8) & 0xFF);
    out[1] = static_cast<uint8_t>(v & 0xFF);
}

/**
 * \brief Reads a 16-bit value from a buffer in big-endian order.
 * \param in Source buffer of at least two bytes.
 * \return Decoded value.
 */
inline uint16_t readBe16(const uint8_t* in) {
    return static_cast<uint16_t>((static_cast<uint16_t>(in[0]) << 8) |
                                  static_cast<uint16_t>(in[1]));
}

} // namespace core
} // namespace cdc

#endif // CDC_CORE_BYTES_H
