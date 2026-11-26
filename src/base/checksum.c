#include "base/checksum.h"

/* The CRC-32 polynomial, with its bits in reversed order. */
#define CRC32_POLYNOMIAL 0xedb88320u

/* The largest prime below 65536. */
#define ADLER32_MODULUS 65521u

/* The most bytes that can be added up before the sums have to be reduced,
 * or the second sum would overflow 32 bits. This is the figure zlib uses. */
#define ADLER32_BLOCK 5552u

uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t size)
{
    /* One bit at a time: simple, and fast enough for a tool that runs
     * once. The working value is kept inverted between the first and the
     * last step. */
    crc = ~crc;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if ((crc & 1u) != 0) {
                crc = (crc >> 1) ^ CRC32_POLYNOMIAL;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

uint32_t adler32_update(uint32_t adler, const uint8_t *data, size_t size)
{
    /* Two running sums: `a` of the bytes, `b` of every value `a` has had.
     * The checksum is `b` in the high 16 bits and `a` in the low 16. */
    uint32_t a = adler & 0xffffu;
    uint32_t b = (adler >> 16) & 0xffffu;

    size_t i = 0;
    while (i < size) {
        size_t block = size - i;
        if (block > ADLER32_BLOCK) {
            block = ADLER32_BLOCK;
        }
        for (size_t end = i + block; i < end; i++) {
            a += data[i];
            b += a;
        }
        a %= ADLER32_MODULUS;
        b %= ADLER32_MODULUS;
    }
    return (b << 16) | a;
}
