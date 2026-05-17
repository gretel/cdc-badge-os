/**
 * \file
 * \brief Centralized non-cryptographic hash utilities.
 */

#ifndef CDC_CORE_HASH_H
#define CDC_CORE_HASH_H

#include <cstddef>
#include <cstdint>

namespace cdc {
namespace core {
namespace hash {

/**
 * \brief FNV-1a 32-bit constants (Fowler/Noll/Vo).
 *
 * See: https://datatracker.ietf.org/doc/html/draft-eastlake-fnv-1a
 *      http://www.isthe.com/chongo/tech/comp/fnv/
 */
static constexpr uint32_t FNV1A_32_OFFSET_BASIS = 0x811C9DC5u;  // 2166136261
static constexpr uint32_t FNV1A_32_PRIME        = 0x01000193u;  // 16777619

/**
 * \brief Mixes a single value into a running FNV-1a 32-bit hash.
 * \param hash In/out running hash; pass `FNV1A_32_OFFSET_BASIS` initially.
 * \param value Byte to mix in.
 */
inline void fnv1a_mix_byte(uint32_t& hash, uint8_t value) {
    hash ^= value;
    hash *= FNV1A_32_PRIME;
}

/**
 * \brief Mixes a 32-bit word into a running FNV-1a 32-bit hash.
 * \param hash In/out running hash; pass `FNV1A_32_OFFSET_BASIS` initially.
 * \param value Word to mix in.
 *
 * Convenience for callers that hash structured numeric fields rather than
 * byte buffers; matches the pre-existing pattern in TropicSlotMap.
 */
inline void fnv1a_mix_u32(uint32_t& hash, uint32_t value) {
    hash ^= value;
    hash *= FNV1A_32_PRIME;
}

/**
 * \brief Computes FNV-1a 32-bit hash over a byte buffer.
 * \param data Input buffer.
 * \param len Buffer length in bytes.
 * \return 32-bit hash value.
 */
inline uint32_t fnv1a_32(const uint8_t* data, size_t len) {
    uint32_t hash = FNV1A_32_OFFSET_BASIS;
    for (size_t i = 0; i < len; i++) {
        fnv1a_mix_byte(hash, data[i]);
    }
    return hash;
}

}  // namespace hash
}  // namespace core
}  // namespace cdc

#endif  // CDC_CORE_HASH_H
