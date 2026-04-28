---
title: "[MEDIUM] Hardcoded UTF-8 bit masks and Unicode codepoint values"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "magic-values"
---

## Summary
UTF-8 encoding bit masks, shift values, and Unicode codepoint values are used directly in the HID keyboard implementation without named constants. These are well-defined by the UTF-8 specification but are not self-documenting in the code.

**Files affected:**
- `components/mod_hid/src/BleHidKeyboard.cpp:478-502` - UTF-8 to codepoint conversion
- `components/mod_hid/src/BleHidKeyboard.cpp:457-464` - German umlaut Unicode fallback

**Magic values found:**
- `0x80`, `0xE0`, `0xF0`, `0xF8` - UTF-8 leading byte masks
- `0xC0`, `0xE0`, `0xF0` - UTF-8 leading byte signatures
- `0x1F`, `0x0F`, `0x07` - UTF-8 leading byte data masks
- `0x3F` - UTF-8 continuation byte mask (repeated 6 times)
- `6`, `12`, `18` - Bit shift values for UTF-8 decoding
- `0x00E4`, `0x00F6`, `0x00FC`, `0x00C4`, `0x00D6`, `0x00DC`, `0x00DF`, `0x20AC` - Unicode codepoints for German characters

## Impact
- **Readability**: `b0 & 0x1F` doesn't convey "UTF-8 3-byte leading byte data mask"
- **Maintainability**: Adding support for other Unicode ranges requires searching through bit logic
- **Standards documentation**: UTF-8 specification link is not in the code
- **Error-prone**: Easy to use wrong mask when modifying UTF-8 logic

## Evidence
```cpp
// components/mod_hid/src/BleHidKeyboard.cpp:473-507
int BleHidKeyboard::utf8ToCodepoint(const char* utf8, uint32_t* codepoint) {
    if (!utf8 || !codepoint) return 0;

    uint8_t b0 = static_cast<uint8_t>(utf8[0]);

    // Magic: 0x80 = UTF-8 ASCII mask
    if ((b0 & 0x80) == 0) {
        *codepoint = b0;
        return 1;
    }

    // Magic: 0xE0, 0xC0 = UTF-8 2-byte header
    // Magic: 0x1F = 2-byte leading byte data mask
    // Magic: 0x3F = continuation byte mask
    // Magic: 6 = bit shift for 2nd byte
    if ((b0 & 0xE0) == 0xC0) {
        if (!utf8[1]) return 0;
        *codepoint = ((b0 & 0x1F) << 6) | (static_cast<uint8_t>(utf8[1]) & 0x3F);
        return 2;
    }

    // Magic: 0xF0, 0xE0 = UTF-8 3-byte header
    // Magic: 0x0F = 3-byte leading byte data mask
    // Magic: 12, 6 = bit shifts
    if ((b0 & 0xF0) == 0xE0) {
        if (!utf8[1] || !utf8[2]) return 0;
        *codepoint = ((b0 & 0x0F) << 12) |
                     ((static_cast<uint8_t>(utf8[1]) & 0x3F) << 6) |
                     (static_cast<uint8_t>(utf8[2]) & 0x3F);
        return 3;
    }

    // Magic: 0xF8, 0xF0 = UTF-8 4-byte header
    // Magic: 0x07 = 4-byte leading byte data mask
    // Magic: 18, 12, 6 = bit shifts
    if ((b0 & 0xF8) == 0xF0) {
        if (!utf8[1] || !utf8[2] || !utf8[3]) return 0;
        *codepoint = ((b0 & 0x07) << 18) |
                     ((static_cast<uint8_t>(utf8[1]) & 0x3F) << 12) |
                     ((static_cast<uint8_t>(utf8[2]) & 0x3F) << 6) |
                     (static_cast<uint8_t>(utf8[3]) & 0x3F);
        return 4;
    }

    return 0;
}

// components/mod_hid/src/BleHidKeyboard.cpp:457-464
bool BleHidKeyboard::typeAsciiFallback(uint32_t codepoint) {
    switch (codepoint) {
        case 0x00E4: typeAsciiChar('a'); typeAsciiChar('e'); return true;  // ä
        case 0x00F6: typeAsciiChar('o'); typeAsciiChar('e'); return true;  // ö
        case 0x00FC: typeAsciiChar('u'); typeAsciiChar('e'); return true;  // ü
        case 0x00C4: typeAsciiChar('A'); typeAsciiChar('e'); return true;  // Ä
        case 0x00D6: typeAsciiChar('O'); typeAsciiChar('e'); return true;  // Ö
        case 0x00DC: typeAsciiChar('U'); typeAsciiChar('e'); return true;  // Ü
        case 0x00DF: typeAsciiChar('s'); typeAsciiChar('s'); return true;  // ß
        case 0x20AC: typeAsciiChar('E'); typeAsciiChar('U'); typeAsciiChar('R'); return true;  // €
        default: return true;
    }
}
```

## Recommended Fix
1. **Create UTF-8 constant definitions** in `components/mod_hid/include/mod_hid/utf8_constants.h`:
    ```cpp
    #pragma once
    #include <cstdint>
    
    /**
     * \brief UTF-8 encoding constants (RFC 3629)
     * 
     * Based on: https://datatracker.ietf.org/doc/html/rfc3629
     * UTF-8 is a variable-length character encoding for Unicode.
     */
    
    // UTF-8 leading byte masks
    static constexpr uint8_t UTF8_MASK_1BYTE = 0x80;   // 0xxxxxxx (ASCII)
    static constexpr uint8_t UTF8_MASK_2BYTE = 0xE0;   // 110xxxxx
    static constexpr uint8_t UTF8_MASK_3BYTE = 0xF0;   // 1110xxxx
    static constexpr uint8_t UTF8_MASK_4BYTE = 0xF8;   // 11110xxx
    
    // UTF-8 leading byte signatures
    static constexpr uint8_t UTF8_SIG_2BYTE = 0xC0;    // 11000000
    static constexpr uint8_t UTF8_SIG_3BYTE = 0xE0;    // 11100000
    static constexpr uint8_t UTF8_SIG_4BYTE = 0xF0;    // 11110000
    
    // UTF-8 data masks (extract payload bits)
    static constexpr uint8_t UTF8_DATA_2BYTE = 0x1F;   // 00011111
    static constexpr uint8_t UTF8_DATA_3BYTE = 0x0F;   // 00001111
    static constexpr uint8_t UTF8_DATA_4BYTE = 0x07;   // 00000111
    
    // UTF-8 continuation byte mask
    static constexpr uint8_t UTF8_CONT_MASK = 0x3F;    // 00111111
    
    // UTF-8 bit shifts for combining bytes
    static constexpr int UTF8_SHIFT_2BYTE = 6;
    static constexpr int UTF8_SHIFT_3BYTE_1 = 12;
    static constexpr int UTF8_SHIFT_3BYTE_2 = 6;
    static constexpr int UTF8_SHIFT_4BYTE_1 = 18;
    static constexpr int UTF8_SHIFT_4BYTE_2 = 12;
    static constexpr int UTF8_SHIFT_4BYTE_3 = 6;
    
    // Unicode codepoint ranges
    static constexpr uint32_t UTF8_MAX_CODEPOINT = 0x10FFFF;
    
    // Common Unicode characters (German keyboard layout)
    static constexpr uint32_t UNICODE_AE_UMLAUT = 0x00E4;  // ä
    static constexpr uint32_t UNICODE_OE_UMLAUT = 0x00F6;  // ö
    static constexpr uint32_t UNICODE_UE_UMLAUT = 0x00FC;  // ü
    static constexpr uint32_t UNICODE_AE_CAP_UMLAUT = 0x00C4;  // Ä
    static constexpr uint32_t UNICODE_OE_CAP_UMLAUT = 0x00D6;  // Ö
    static constexpr uint32_t UNICODE_UE_CAP_UMLAUT = 0x00DC;  // Ü
    static constexpr uint32_t UNICODE_ESZE = 0x00DF;    // ß
    static constexpr uint32_t UNICODE_EURO = 0x20AC;    // €
    ```

2. **Refactor UTF-8 decoding** to use constants:
    ```cpp
    // Before:
    if ((b0 & 0xE0) == 0xC0) {
        *codepoint = ((b0 & 0x1F) << 6) | (static_cast<uint8_t>(utf8[1]) & 0x3F);
        return 2;
    }
    
    // After:
    if ((b0 & UTF8_MASK_2BYTE) == UTF8_SIG_2BYTE) {
        *codepoint = ((b0 & UTF8_DATA_2BYTE) << UTF8_SHIFT_2BYTE) |
                     (static_cast<uint8_t>(utf8[1]) & UTF8_CONT_MASK);
        return 2;
    }
    ```

3. **Refactor Unicode fallback** to use constants:
    ```cpp
    // Before:
    case 0x00E4: typeAsciiChar('a'); typeAsciiChar('e'); return true;
    
    // After:
    case UNICODE_AE_UMLAUT: typeAsciiChar('a'); typeAsciiChar('e'); return true;
    ```

4. **Add documentation** explaining the UTF-8 algorithm:
    ```cpp
    /**
     * \brief UTF-8 bit pattern reference:
     * 
     * 1 byte:  0xxxxxxx
     * 2 bytes: 110xxxxx 10xxxxxx
     * 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx
     * 4 bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
     */
    ```

## References
- [RFC 3629 - UTF-8, a transformation format of ISO 10646](https://datatracker.ietf.org/doc/html/rfc3629)
- [Unicode Standard - UTF-8 Encoding](https://unicode.org/versions/Unicode15.0.0/ch03.pdf)
- [UTF-8 Decoder Algorithm](https://www.unicode.org/versions/Unicode15.0.0/ch03.pdf#G13044)
