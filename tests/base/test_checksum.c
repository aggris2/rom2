#include "base/checksum.h"

#include <string.h>

#include "utest.h"

static const uint8_t *text(const char *s)
{
    return (const uint8_t *)s;
}

/* ---- crc32_update ---- */

UTEST(checksum, crc32_of_nothing_is_zero)
{
    ASSERT_EQ(0u, crc32_update(CRC32_START, NULL, 0));
}

UTEST(checksum, crc32_matches_the_published_check_values)
{
    /* "123456789" is the standard check input for every CRC. */
    ASSERT_EQ(0xcbf43926u, crc32_update(CRC32_START, text("123456789"), 9));
    ASSERT_EQ(0xe8b7be43u, crc32_update(CRC32_START, text("a"), 1));
    /* The chunk type "IEND" with no data: the last four bytes of every
     * PNG file are this number. */
    ASSERT_EQ(0xae426082u, crc32_update(CRC32_START, text("IEND"), 4));
}

UTEST(checksum, crc32_in_pieces_equals_crc32_in_one_go)
{
    uint32_t crc = crc32_update(CRC32_START, text("1234"), 4);
    crc = crc32_update(crc, text(""), 0);
    crc = crc32_update(crc, text("56789"), 5);

    ASSERT_EQ(0xcbf43926u, crc);
}

/* The CRC worked out one bit at a time, straight from its definition. */
static uint32_t crc32_bit_by_bit(const uint8_t *data, size_t size)
{
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 1u) != 0 ? (crc >> 1) ^ 0xedb88320u : crc >> 1;
        }
    }
    return ~crc;
}

UTEST(checksum, crc32_table_agrees_with_the_definition_for_every_byte)
{
    /* Every byte value on its own, and then all 256 in a row, which
     * takes the working value through many different states. */
    uint8_t all[256];
    for (int value = 0; value < 256; value++) {
        const uint8_t byte = (uint8_t)value;
        all[value] = byte;
        ASSERT_EQ(crc32_bit_by_bit(&byte, 1),
            crc32_update(CRC32_START, &byte, 1));
    }

    ASSERT_EQ(crc32_bit_by_bit(all, sizeof all),
        crc32_update(CRC32_START, all, sizeof all));
}

/* ---- adler32_update ---- */

UTEST(checksum, adler32_of_nothing_is_one)
{
    ASSERT_EQ(1u, adler32_update(ADLER32_START, NULL, 0));
}

UTEST(checksum, adler32_matches_the_published_check_values)
{
    ASSERT_EQ(0x11e60398u, adler32_update(ADLER32_START, text("Wikipedia"), 9));
    ASSERT_EQ(0x00620062u, adler32_update(ADLER32_START, text("a"), 1));
}

UTEST(checksum, adler32_in_pieces_equals_adler32_in_one_go)
{
    uint32_t adler = adler32_update(ADLER32_START, text("Wiki"), 4);
    adler = adler32_update(adler, text(""), 0);
    adler = adler32_update(adler, text("pedia"), 5);

    ASSERT_EQ(0x11e60398u, adler);
}

UTEST(checksum, adler32_does_not_overflow_on_a_long_run_of_large_bytes)
{
    /* 100,000 bytes of 0xff: far more than can be summed without
     * reducing along the way. The expected value comes from zlib. */
    static uint8_t bytes[100000];
    memset(bytes, 0xff, sizeof bytes);

    ASSERT_EQ(0x149a302cu, adler32_update(ADLER32_START, bytes, sizeof bytes));
}
