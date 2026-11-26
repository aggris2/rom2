#include "base/zlib.h"

#include <string.h>

#include "utest.h"

/* Streams made by the real zlib library, one of each kind of block. They
 * check that zlib_decompress() reads what other programs write, not only
 * what zlib_compress() writes. */

/* zlib level 9 on "abcabcabcabcabc": a block with the fixed codes. */
static const uint8_t FIXED_STREAM[13] = {
    0x78, 0xda, 0x4b, 0x4c, 0x4a, 0x4e, 0x44, 0x42, 0x00, 0x2d, 0xf5, 0x05,
    0xbf,
};

/* zlib level 0 on "stored bytes": a block with no packing. */
static const uint8_t STORED_STREAM[23] = {
    0x78, 0x01, 0x01, 0x0c, 0x00, 0xf3, 0xff, 0x73, 0x74, 0x6f, 0x72, 0x65,
    0x64, 0x20, 0x62, 0x79, 0x74, 0x65, 0x73, 0x1f, 0xcf, 0x04, 0xd9,
};

/* zlib level 9 on the text built by make_text() below: a block that
 * carries its own code tables. */
static const uint8_t DYNAMIC_STREAM[158] = {
    0x78, 0xda, 0xdd, 0x8d, 0x65, 0x12, 0x82, 0x50, 0x18, 0x45, 0xed, 0xee,
    0x6e, 0x3f, 0xbb, 0xb1, 0xbb, 0xd6, 0x02, 0x4a, 0x29, 0xf8, 0x68, 0x84,
    0xd5, 0xfb, 0xc6, 0x5d, 0xe8, 0xcf, 0x3b, 0xf7, 0xcc, 0x39, 0x1a, 0x47,
    0x83, 0xac, 0xf3, 0xb7, 0x27, 0x50, 0x0a, 0x32, 0x5f, 0xc0, 0xa0, 0x37,
    0x3c, 0x74, 0x51, 0x52, 0x01, 0x19, 0xb4, 0x02, 0x1a, 0xbe, 0x05, 0xd2,
    0xb6, 0xe0, 0x8e, 0x58, 0xe2, 0xbb, 0xfe, 0x1a, 0x76, 0x38, 0x5d, 0x6e,
    0x8f, 0xd7, 0xe7, 0x0f, 0x04, 0x43, 0xe1, 0x48, 0x34, 0x16, 0x4f, 0x24,
    0x53, 0xe9, 0x4c, 0x36, 0x97, 0x2f, 0x14, 0x4b, 0xe5, 0x4a, 0xb5, 0x56,
    0x87, 0x46, 0xb3, 0xd5, 0xee, 0x74, 0x7b, 0xfd, 0xc1, 0x70, 0x34, 0x9e,
    0x10, 0xd3, 0xd9, 0x7c, 0xb1, 0x5c, 0xad, 0x37, 0xdb, 0xdd, 0xfe, 0x70,
    0x3c, 0x9d, 0x2f, 0x57, 0x89, 0xc4, 0x1d, 0xd1, 0x02, 0x0a, 0x47, 0x4c,
    0x5e, 0xe3, 0x80, 0xe1, 0x0d, 0x1a, 0xab, 0x6d, 0xfa, 0x05, 0x02, 0x2f,
    0xeb, 0x48, 0xc1, 0x6d, 0x56, 0x25, 0xe0, 0x17, 0xc0, 0x0f, 0x83, 0x34,
    0xb3, 0xbe,
};

enum { TEXT_SIZE = 539 };

/* Six times one sentence, the bytes 0 to 63, then five times another. */
static void make_text(uint8_t text[TEXT_SIZE])
{
    const char *const first = "the quick brown fox jumps over the lazy dog. ";
    const char *const second = "pack my box with five dozen liquor jugs. ";
    size_t at = 0;
    for (int i = 0; i < 6; i++) {
        memcpy(text + at, first, strlen(first));
        at += strlen(first);
    }
    for (int i = 0; i < 64; i++) {
        text[at++] = (uint8_t)i;
    }
    for (int i = 0; i < 5; i++) {
        memcpy(text + at, second, strlen(second));
        at += strlen(second);
    }
}

/* Packs the bytes, unpacks the result and checks nothing changed. Returns
 * the packed size through `packed_size` if it is not NULL. */
static bool survives(const uint8_t *data, size_t size, size_t *packed_size)
{
    ByteBuffer packed = {0};
    ByteBuffer unpacked = {0};
    bool ok = zlib_compress(&packed, data, size)
        && zlib_decompress(&unpacked, packed.data, packed.size, size)
        && unpacked.size == size
        && (size == 0 || memcmp(unpacked.data, data, size) == 0);
    if (packed_size != NULL) {
        *packed_size = packed.size;
    }
    buffer_free(&packed);
    buffer_free(&unpacked);
    return ok;
}

/* ---- zlib_decompress, on streams made by zlib ---- */

UTEST(zlib, decompress_reads_a_block_with_the_fixed_codes)
{
    ByteBuffer out = {0};

    ASSERT_TRUE(zlib_decompress(&out, FIXED_STREAM, sizeof FIXED_STREAM, 100));

    ASSERT_EQ(15u, out.size);
    ASSERT_EQ(0, memcmp(out.data, "abcabcabcabcabc", 15));
    buffer_free(&out);
}

UTEST(zlib, decompress_reads_a_block_with_no_packing)
{
    ByteBuffer out = {0};

    ASSERT_TRUE(zlib_decompress(&out, STORED_STREAM, sizeof STORED_STREAM,
        100));

    ASSERT_EQ(12u, out.size);
    ASSERT_EQ(0, memcmp(out.data, "stored bytes", 12));
    buffer_free(&out);
}

UTEST(zlib, decompress_reads_a_block_with_its_own_code_tables)
{
    uint8_t text[TEXT_SIZE];
    make_text(text);
    ByteBuffer out = {0};

    ASSERT_TRUE(zlib_decompress(&out, DYNAMIC_STREAM, sizeof DYNAMIC_STREAM,
        TEXT_SIZE));

    ASSERT_EQ((size_t)TEXT_SIZE, out.size);
    ASSERT_EQ(0, memcmp(out.data, text, TEXT_SIZE));
    buffer_free(&out);
}

UTEST(zlib, decompress_adds_to_what_is_already_in_the_buffer)
{
    ByteBuffer out = {0};
    ASSERT_TRUE(buffer_push(&out, 0xab));

    ASSERT_TRUE(zlib_decompress(&out, FIXED_STREAM, sizeof FIXED_STREAM, 100));

    ASSERT_EQ(16u, out.size);
    ASSERT_EQ(0xab, out.data[0]);
    ASSERT_EQ('a', out.data[1]);
    ASSERT_EQ('c', out.data[15]);
    buffer_free(&out);
}

/* ---- zlib_decompress, on damaged streams ---- */

UTEST(zlib, decompress_refuses_more_output_than_allowed)
{
    ByteBuffer out = {0};
    ASSERT_TRUE(buffer_push(&out, 0xab));

    /* Each stream unpacks to one byte more than is allowed here. */
    ASSERT_FALSE(zlib_decompress(&out, FIXED_STREAM, sizeof FIXED_STREAM, 14));
    ASSERT_FALSE(zlib_decompress(&out, STORED_STREAM, sizeof STORED_STREAM,
        11));
    ASSERT_FALSE(zlib_decompress(&out, DYNAMIC_STREAM, sizeof DYNAMIC_STREAM,
        TEXT_SIZE - 1));

    ASSERT_EQ(1u, out.size);
    ASSERT_EQ(0xab, out.data[0]);
    buffer_free(&out);
}

UTEST(zlib, decompress_of_a_stream_cut_off_anywhere_fails)
{
    /* Every shorter version of the stream, from nothing at all up to one
     * byte short of whole. None may succeed, and none may crash. */
    ByteBuffer out = {0};

    for (size_t size = 0; size < sizeof DYNAMIC_STREAM; size++) {
        ASSERT_FALSE(zlib_decompress(&out, DYNAMIC_STREAM, size, TEXT_SIZE));
        ASSERT_EQ(0u, out.size);
    }
    for (size_t size = 0; size < sizeof STORED_STREAM; size++) {
        ASSERT_FALSE(zlib_decompress(&out, STORED_STREAM, size, 100));
        ASSERT_EQ(0u, out.size);
    }
    buffer_free(&out);
}

UTEST(zlib, decompress_of_a_stream_with_one_byte_changed_never_crashes)
{
    /* Every byte of the stream in turn is changed in three ways. A changed
     * stream may fail, or by luck still unpack, but whatever it gives is
     * within the limit, and a failure leaves the buffer empty. */
    uint8_t stream[sizeof DYNAMIC_STREAM];
    ByteBuffer out = {0};

    for (size_t i = 0; i < sizeof stream; i++) {
        const uint8_t changes[3] = {0x01, 0x80, 0xff};
        for (size_t c = 0; c < 3; c++) {
            memcpy(stream, DYNAMIC_STREAM, sizeof stream);
            stream[i] ^= changes[c];

            bool ok = zlib_decompress(&out, stream, sizeof stream, TEXT_SIZE);

            ASSERT_TRUE(out.size <= TEXT_SIZE);
            if (!ok) {
                ASSERT_EQ(0u, out.size);
            }
            out.size = 0;
        }
    }
    buffer_free(&out);
}

UTEST(zlib, decompress_with_a_wrong_checksum_fails)
{
    uint8_t stream[sizeof FIXED_STREAM];
    memcpy(stream, FIXED_STREAM, sizeof stream);
    stream[sizeof stream - 1] ^= 0x01;
    ByteBuffer out = {0};

    ASSERT_FALSE(zlib_decompress(&out, stream, sizeof stream, 100));

    ASSERT_EQ(0u, out.size);
    buffer_free(&out);
}

UTEST(zlib, decompress_with_a_bad_header_fails)
{
    /* Not deflate, then a header that does not divide by 31, then one
     * that asks for a preset dictionary. */
    const uint8_t headers[3][2] = {{0x79, 0x01}, {0x78, 0x02}, {0x78, 0x20}};
    uint8_t stream[sizeof FIXED_STREAM];
    ByteBuffer out = {0};

    for (size_t i = 0; i < 3; i++) {
        memcpy(stream, FIXED_STREAM, sizeof stream);
        stream[0] = headers[i][0];
        stream[1] = headers[i][1];
        ASSERT_FALSE(zlib_decompress(&out, stream, sizeof stream, 100));
        ASSERT_EQ(0u, out.size);
    }
    buffer_free(&out);
}

UTEST(zlib, decompress_of_a_repeat_from_before_the_start_fails)
{
    /* A fixed-codes block whose first symbol says "copy 3 bytes from 1
     * byte back", when nothing has been produced yet. */
    const uint8_t stream[] = {
        0x78, 0x01, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01
    };
    ByteBuffer out = {0};
    ASSERT_TRUE(buffer_push(&out, 0xab));

    ASSERT_FALSE(zlib_decompress(&out, stream, sizeof stream, 100));

    ASSERT_EQ(1u, out.size);
    buffer_free(&out);
}

UTEST(zlib, decompress_refuses_one_plain_byte_more_than_allowed)
{
    /* Three different bytes pack to three plain-byte symbols, so the
     * limit is reached by a plain byte and not by a repeat. */
    const uint8_t data[] = {1, 2, 3};
    ByteBuffer packed = {0};
    ByteBuffer out = {0};
    ASSERT_TRUE(zlib_compress(&packed, data, sizeof data));

    ASSERT_FALSE(zlib_decompress(&out, packed.data, packed.size, 2));
    ASSERT_EQ(0u, out.size);

    ASSERT_TRUE(zlib_decompress(&out, packed.data, packed.size, 3));
    ASSERT_EQ(3u, out.size);
    buffer_free(&packed);
    buffer_free(&out);
}

UTEST(zlib, decompress_of_a_length_symbol_that_does_not_exist_fails)
{
    /* The fixed codes have room for symbols 286 and 287, but the format
     * gives them no meaning. This block's only symbol is 286. */
    const uint8_t stream[] = {0x78, 0x01, 0x1b, 0x03, 0x00, 0x00, 0x00, 0x01};
    ByteBuffer out = {0};

    ASSERT_FALSE(zlib_decompress(&out, stream, sizeof stream, 100));

    ASSERT_EQ(0u, out.size);
    buffer_free(&out);
}

UTEST(zlib, decompress_of_an_unknown_kind_of_block_fails)
{
    /* The three bits after the header say "last block, kind 3". */
    const uint8_t stream[] = {0x78, 0x01, 0x07, 0x00, 0x00, 0x00, 0x00, 0x01};
    ByteBuffer out = {0};

    ASSERT_FALSE(zlib_decompress(&out, stream, sizeof stream, 100));

    ASSERT_EQ(0u, out.size);
    buffer_free(&out);
}

/* ---- zlib_compress ---- */

UTEST(zlib, compress_of_nothing_gives_a_stream_that_unpacks_to_nothing)
{
    size_t packed_size = 0;

    ASSERT_TRUE(survives(NULL, 0, &packed_size));

    /* Two header bytes, one end-of-block symbol, four checksum bytes. */
    ASSERT_TRUE(packed_size <= 8);
}

UTEST(zlib, compress_keeps_one_byte_of_every_value)
{
    for (int value = 0; value < 256; value++) {
        const uint8_t byte = (uint8_t)value;
        ASSERT_TRUE(survives(&byte, 1, NULL));
    }
}

UTEST(zlib, compress_keeps_text_with_repeats)
{
    uint8_t text[TEXT_SIZE];
    make_text(text);
    size_t packed_size = 0;

    ASSERT_TRUE(survives(text, sizeof text, &packed_size));

    ASSERT_TRUE(packed_size < sizeof text / 2);
}

UTEST(zlib, compress_keeps_every_length_of_repeat)
{
    /* A run of one byte value of every length from 0 to 600 covers each
     * repeat length from 3 to 258 and what happens just past it. */
    static uint8_t run[600];
    memset(run, 0x5a, sizeof run);

    for (size_t size = 0; size <= sizeof run; size++) {
        ASSERT_TRUE(survives(run, size, NULL));
    }
}

UTEST(zlib, compress_keeps_repeats_at_every_distance_up_to_the_limit)
{
    /* The same 5 bytes twice with a growing stretch of other bytes in
     * between, from next to each other to further apart than the 32,768
     * bytes a repeat can reach back. */
    static uint8_t data[40000];
    const size_t gaps[] = {0, 1, 2, 3, 255, 256, 257, 4000, 32760, 32763,
        32764, 32770, 39000};

    for (size_t g = 0; g < sizeof gaps / sizeof gaps[0]; g++) {
        size_t size = 5 + gaps[g] + 5;
        for (size_t i = 0; i < size; i++) {
            data[i] = (uint8_t)((i * 2654435761u) >> 13);
        }
        memcpy(data, "HELLO", 5);
        memcpy(data + 5 + gaps[g], "HELLO", 5);

        ASSERT_TRUE(survives(data, size, NULL));
    }
}

UTEST(zlib, compress_keeps_data_with_no_repeats_at_all)
{
    /* Scrambled bytes: nothing to find, so the stream comes out a little
     * larger than the data. It must still unpack to the same bytes. */
    static uint8_t data[70000];
    uint32_t state = 12345;
    for (size_t i = 0; i < sizeof data; i++) {
        state = state * 1664525u + 1013904223u;
        data[i] = (uint8_t)(state >> 24);
    }

    ASSERT_TRUE(survives(data, sizeof data, NULL));
}

UTEST(zlib, compress_makes_a_mostly_empty_picture_very_small)
{
    /* 64 by 64 zeros with a few other bytes in the middle, like a sprite
     * frame. It should shrink to a small part of its size. */
    static uint8_t data[64 * 64];
    memset(data, 0, sizeof data);
    for (size_t i = 0; i < 30; i++) {
        data[64 * 30 + 20 + i] = (uint8_t)(i + 1);
    }
    size_t packed_size = 0;

    ASSERT_TRUE(survives(data, sizeof data, &packed_size));

    ASSERT_TRUE(packed_size < sizeof data / 20);
}

/* The kind of the first block of a stream: 0 stored, 1 fixed codes,
 * 2 own code tables. It sits in bits 1 and 2 of the byte after the
 * 2-byte header. */
static int first_block_kind(const ByteBuffer *packed)
{
    return (packed->data[2] >> 1) & 3;
}

UTEST(zlib, compress_stores_data_that_cannot_be_packed)
{
    /* Scrambled bytes have nothing to shorten. The block is then written
     * as it is, and the stream is only a few bytes larger than the data:
     * 5 bytes for each block of up to 65,535, and 6 for the stream. */
    static uint8_t data[70000];
    uint32_t state = 777;
    for (size_t i = 0; i < sizeof data; i++) {
        state = state * 1664525u + 1013904223u;
        data[i] = (uint8_t)(state >> 24);
    }
    ByteBuffer packed = {0};

    ASSERT_TRUE(zlib_compress(&packed, data, sizeof data));

    ASSERT_EQ(0, first_block_kind(&packed));
    ASSERT_TRUE(packed.size <= sizeof data + 2 * 5 + 6);
    buffer_free(&packed);
}

UTEST(zlib, compress_uses_the_fixed_codes_for_very_little_data)
{
    /* Code tables of its own would cost more than they save here. */
    const uint8_t data[] = {1, 2, 3};
    ByteBuffer packed = {0};

    ASSERT_TRUE(zlib_compress(&packed, data, sizeof data));

    ASSERT_EQ(1, first_block_kind(&packed));
    buffer_free(&packed);
}

UTEST(zlib, compress_uses_its_own_code_tables_when_they_pay_off)
{
    uint8_t text[TEXT_SIZE];
    make_text(text);
    ByteBuffer packed = {0};

    ASSERT_TRUE(zlib_compress(&packed, text, sizeof text));

    ASSERT_EQ(2, first_block_kind(&packed));
    buffer_free(&packed);
}

UTEST(zlib, compress_keeps_data_whose_byte_values_are_very_uneven)
{
    /* 24 byte values, each used about 1.6 times as often as the one
     * before, as in the Fibonacci numbers. Left alone, Huffman's method
     * gives the rarest ones codes longer than the 15 bits the format
     * allows, so this checks that the lengths are brought back in. */
    static uint8_t data[60000];
    uint32_t counts[24];
    counts[0] = 1;
    counts[1] = 1;
    for (int i = 2; i < 24; i++) {
        counts[i] = counts[i - 1] + counts[i - 2];
    }
    size_t at = 0;
    for (int value = 0; value < 24; value++) {
        for (uint32_t n = 0; n < counts[value] && at < sizeof data; n++) {
            data[at++] = (uint8_t)value;
        }
    }
    /* Shuffle, so that repeats do not hide the uneven counts. */
    uint32_t state = 4242;
    for (size_t i = at - 1; i > 0; i--) {
        state = state * 1664525u + 1013904223u;
        size_t j = (state >> 8) % (i + 1);
        uint8_t swap = data[i];
        data[i] = data[j];
        data[j] = swap;
    }

    ASSERT_TRUE(survives(data, at, NULL));
}

UTEST(zlib, compress_keeps_data_longer_than_one_block)
{
    /* 200,000 bytes is four blocks. Repeats that reach back across a
     * block boundary must still be right. */
    static uint8_t data[200000];
    for (size_t i = 0; i < sizeof data; i++) {
        data[i] = (uint8_t)((i % 1000) * 7 + i / 50000);
    }
    size_t packed_size = 0;

    ASSERT_TRUE(survives(data, sizeof data, &packed_size));

    ASSERT_TRUE(packed_size < sizeof data / 10);
}

UTEST(zlib, compress_adds_to_what_is_already_in_the_buffer)
{
    const uint8_t data[] = {1, 2, 3};
    ByteBuffer packed = {0};
    ASSERT_TRUE(buffer_push(&packed, 0xab));

    ASSERT_TRUE(zlib_compress(&packed, data, sizeof data));

    ASSERT_EQ(0xab, packed.data[0]);
    ASSERT_EQ(0x78, packed.data[1]);
    buffer_free(&packed);
}
