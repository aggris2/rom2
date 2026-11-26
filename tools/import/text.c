#include "import/text.h"

/* What the bytes 0x80 to 0xbf of code page 1251 mean in Unicode. The
 * bytes from 0xc0 on are the letters U+0410 to U+044F in order. */
static const uint16_t UPPER_HALF[64] = {
    0x0402, 0x0403, 0x201a, 0x0453, 0x201e, 0x2026, 0x2020, 0x2021,
    0x20ac, 0x2030, 0x0409, 0x2039, 0x040a, 0x040c, 0x040b, 0x040f,
    0x0452, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
    0xfffd, 0x2122, 0x0459, 0x203a, 0x045a, 0x045c, 0x045b, 0x045f,
    0x00a0, 0x040e, 0x045e, 0x0408, 0x00a4, 0x0490, 0x00a6, 0x00a7,
    0x0401, 0x00a9, 0x0404, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x0407,
    0x00b0, 0x00b1, 0x0406, 0x0456, 0x0491, 0x00b5, 0x00b6, 0x00b7,
    0x0451, 0x2116, 0x0454, 0x00bb, 0x0458, 0x0405, 0x0455, 0x0457
};

/* Appends one character below U+10000 as UTF-8. */
static bool put_character(ByteBuffer *out, uint32_t c)
{
    if (c < 0x80) {
        return buffer_push(out, (uint8_t)c);
    }
    if (c < 0x800) {
        return buffer_push(out, (uint8_t)(0xc0 | (c >> 6)))
            && buffer_push(out, (uint8_t)(0x80 | (c & 0x3f)));
    }
    return buffer_push(out, (uint8_t)(0xe0 | (c >> 12)))
        && buffer_push(out, (uint8_t)(0x80 | ((c >> 6) & 0x3f)))
        && buffer_push(out, (uint8_t)(0x80 | (c & 0x3f)));
}

bool text_cp1251_to_utf8(const uint8_t *data, size_t size, ByteBuffer *out)
{
    for (size_t i = 0; i < size; i++) {
        uint8_t byte = data[i];
        if (byte == '\r' && i + 1 < size && data[i + 1] == '\n') {
            continue;
        }
        uint32_t c = byte;
        if (byte >= 0xc0) {
            c = 0x0410u + (uint32_t)(byte - 0xc0);
        } else if (byte >= 0x80) {
            c = UPPER_HALF[byte - 0x80];
        }
        if (!put_character(out, c)) {
            return false;
        }
    }
    return true;
}
