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
 *
 * Supports `\ ` (backslash-space) as an escaped literal space inside a token,
 * so tokens may contain space characters when escaped.
 *
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
    while (*s && i + 1 < outSize) {
        if (*s == '\\' && *(s + 1) == ' ') {
            out[i++] = ' ';
            s += 2;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(*s))) break;
        out[i++] = *s++;
    }
    out[i] = '\0';
    while (*s) {
        if (*s == '\\' && *(s + 1) == ' ') {
            s += 2;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(*s))) break;
        s++;
    }
    return s;
}

/**
 * \brief Replaces every `\ ` escape sequence with a single space character in-place.
 * \param s Mutable null-terminated string buffer.
 */
inline void unescapeSpaces(char* s) {
    if (!s) return;
    char* w = s;
    for (const char* r = s; *r; ) {
        if (*r == '\\' && *(r + 1) == ' ') {
            *w++ = ' ';
            r += 2;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
}

} // namespace cdc::core
