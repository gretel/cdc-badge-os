#pragma once

#include <cctype>
#include <cstddef>

namespace cdc::core {

/**
 * \brief Advances over leading ASCII whitespace in a C string.
 * \param s Input string pointer.
 * \return Pointer to first non-whitespace character, or `nullptr` when input is null.
 */
inline const char* skipSpaces(const char* s) {
    while (s && *s && std::isspace(static_cast<unsigned char>(*s))) {
        s++;
    }
    return s;
}

/**
 * \brief Extracts one whitespace-delimited token from a string.
 * \param s Input cursor position.
 * \param out Output token buffer (will be null-terminated on success).
 * \param outSize Output buffer capacity.
 * \return Pointer to the next unread input position, or `nullptr` when no token exists.
 */
inline const char* nextToken(const char* s, char* out, size_t outSize) {
    if (!out || outSize == 0) return nullptr;
    s = skipSpaces(s);
    if (!s || !*s) return nullptr;
    size_t i = 0;
    while (*s && !std::isspace(static_cast<unsigned char>(*s)) && i + 1 < outSize) {
        out[i++] = *s++;
    }
    out[i] = '\0';
    return s;
}

} // namespace cdc::core
