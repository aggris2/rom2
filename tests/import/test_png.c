#include "import/png.h"

#include <stdlib.h>
#include <string.h>

#include "base/checksum.h"
#include "base/reader.h"
#include "utest.h"

/* The tests read back what the writer wrote, with a small PNG reader of
 * their own that understands just the kind of file the writer makes. */

/* PNG stores its numbers high byte first. */
static uint32_t read_u32_be(Reader *r)
{
    uint32_t b0 = reader_u8(r);
    uint32_t b1 = reader_u8(r);
    uint32_t b2 = reader_u8(r);
    uint32_t b3 = reader_u8(r);
    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}

/* One chunk of a PNG file. */
typedef struct Chunk {
    char type[5];        /* four letters and a closing zero */
    const uint8_t *data;
    uint32_t size;
    bool crc_ok;
} Chunk;

/* Reads the chunk the reader stands at. Returns false at the end of the
 * file or if the chunk is cut off. */
static bool read_chunk(Reader *r, Chunk *out)
{
    uint32_t size = read_u32_be(r);
    const uint8_t *type = reader_bytes(r, 4);
    const uint8_t *data = reader_bytes(r, size);
    uint32_t crc = read_u32_be(r);
    if (!reader_ok(r)) {
        return false;
    }
    memcpy(out->type, type, 4);
    out->type[4] = '\0';
    out->data = data;
    out->size = size;
    out->crc_ok = crc == crc32_update(
        crc32_update(CRC32_START, type, 4), data, size);
    return true;
}

/* Undoes the zlib wrapping of an IDAT chunk made of stored blocks. Returns
 * a malloc()ed block of `*out_size` bytes, or NULL if the stream is not
 * what the writer should have made. */
static uint8_t *unwrap_stored(const uint8_t *data, size_t size,
    size_t *out_size)
{
    Reader r = reader_init(data, size);
    if (reader_u8(&r) != 0x78 || reader_u8(&r) != 0x01) {
        return NULL;
    }
    uint8_t *raw = malloc(size);
    size_t raw_size = 0;
    bool last = false;
    while (raw != NULL && !last) {
        uint8_t flag = reader_u8(&r);
        uint16_t len = reader_u16(&r);
        uint16_t flipped = reader_u16(&r);
        const uint8_t *bytes = reader_bytes(&r, len);
        if (!reader_ok(&r) || flag > 1 || (uint16_t)~len != flipped) {
            free(raw);
            return NULL;
        }
        if (len > 0) {
            memcpy(raw + raw_size, bytes, len);
        }
        raw_size += len;
        last = flag == 1;
    }
    if (raw == NULL) {
        return NULL;
    }
    uint32_t adler = read_u32_be(&r);
    if (!reader_ok(&r) || reader_remaining(&r) != 0
        || adler != adler32_update(ADLER32_START, raw, raw_size)) {
        free(raw);
        return NULL;
    }
    *out_size = raw_size;
    return raw;
}

/* A palette in which every colour differs: index n is (n, 255 - n, n / 2). */
static void make_palette(Rgb palette[256])
{
    for (size_t i = 0; i < 256; i++) {
        palette[i].r = (uint8_t)i;
        palette[i].g = (uint8_t)(255 - i);
        palette[i].b = (uint8_t)(i / 2);
    }
}

/* ---- png_write_indexed ---- */

UTEST(png, a_file_is_the_signature_and_five_chunks_in_order)
{
    const uint8_t pixels[6] = {0, 1, 2, 3, 4, 5};
    const uint8_t signature[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    Rgb palette[256];
    make_palette(palette);
    ByteBuffer png = {0};

    ASSERT_TRUE(png_write_indexed(&png, 3, 2, pixels, palette));

    Reader r = reader_init(png.data, png.size);
    const uint8_t *start = reader_bytes(&r, 8);
    ASSERT_TRUE(reader_ok(&r));
    ASSERT_EQ(0, memcmp(start, signature, 8));

    const char *const expected[5] = {"IHDR", "PLTE", "tRNS", "IDAT", "IEND"};
    for (size_t i = 0; i < 5; i++) {
        Chunk c;
        ASSERT_TRUE(read_chunk(&r, &c));
        ASSERT_STREQ(expected[i], c.type);
        ASSERT_TRUE(c.crc_ok);
    }
    ASSERT_EQ(0u, reader_remaining(&r));
    buffer_free(&png);
}

UTEST(png, the_header_gives_the_size_and_says_8_bit_indexed)
{
    const uint8_t pixels[6] = {0, 1, 2, 3, 4, 5};
    Rgb palette[256];
    make_palette(palette);
    ByteBuffer png = {0};
    ASSERT_TRUE(png_write_indexed(&png, 3, 2, pixels, palette));
    Reader r = reader_init(png.data, png.size);
    reader_skip(&r, 8);
    Chunk c;

    ASSERT_TRUE(read_chunk(&r, &c));

    ASSERT_EQ(13u, c.size);
    Reader h = reader_init(c.data, c.size);
    ASSERT_EQ(3u, read_u32_be(&h)); /* width */
    ASSERT_EQ(2u, read_u32_be(&h)); /* height */
    ASSERT_EQ(8, reader_u8(&h));    /* bits per pixel */
    ASSERT_EQ(3, reader_u8(&h));    /* colour type: palette */
    ASSERT_EQ(0, reader_u8(&h));
    ASSERT_EQ(0, reader_u8(&h));
    ASSERT_EQ(0, reader_u8(&h));    /* not interlaced */
    buffer_free(&png);
}

UTEST(png, the_palette_is_256_colours_red_first)
{
    const uint8_t pixels[1] = {7};
    Rgb palette[256];
    make_palette(palette);
    ByteBuffer png = {0};
    ASSERT_TRUE(png_write_indexed(&png, 1, 1, pixels, palette));
    Reader r = reader_init(png.data, png.size);
    reader_skip(&r, 8);
    Chunk c;
    ASSERT_TRUE(read_chunk(&r, &c)); /* IHDR */

    ASSERT_TRUE(read_chunk(&r, &c));

    ASSERT_STREQ("PLTE", c.type);
    ASSERT_EQ(768u, c.size);
    ASSERT_EQ(0, c.data[0]);         /* colour 0: red */
    ASSERT_EQ(255, c.data[1]);       /* colour 0: green */
    ASSERT_EQ(0, c.data[2]);         /* colour 0: blue */
    ASSERT_EQ(200, c.data[200 * 3]);
    ASSERT_EQ(55, c.data[200 * 3 + 1]);
    ASSERT_EQ(100, c.data[200 * 3 + 2]);
    ASSERT_EQ(255, c.data[765]);
    ASSERT_EQ(0, c.data[766]);
    ASSERT_EQ(127, c.data[767]);
    buffer_free(&png);
}

UTEST(png, only_index_0_is_made_transparent)
{
    const uint8_t pixels[1] = {7};
    Rgb palette[256];
    make_palette(palette);
    ByteBuffer png = {0};
    ASSERT_TRUE(png_write_indexed(&png, 1, 1, pixels, palette));
    Reader r = reader_init(png.data, png.size);
    reader_skip(&r, 8);
    Chunk c;
    ASSERT_TRUE(read_chunk(&r, &c)); /* IHDR */
    ASSERT_TRUE(read_chunk(&r, &c)); /* PLTE */

    ASSERT_TRUE(read_chunk(&r, &c));

    ASSERT_STREQ("tRNS", c.type);
    ASSERT_EQ(1u, c.size);
    ASSERT_EQ(0, c.data[0]);
    buffer_free(&png);
}

/* Writes a picture, reads it back and checks that every pixel came through
 * unchanged. Returns false at the first thing that is wrong. */
static bool pixels_survive(uint32_t width, uint32_t height,
    const uint8_t *pixels)
{
    Rgb palette[256];
    make_palette(palette);
    ByteBuffer png = {0};
    if (!png_write_indexed(&png, width, height, pixels, palette)) {
        return false;
    }

    Reader r = reader_init(png.data, png.size);
    reader_skip(&r, 8);
    Chunk c;
    bool found = false;
    while (!found && read_chunk(&r, &c)) {
        found = strcmp(c.type, "IDAT") == 0;
    }
    size_t raw_size = 0;
    uint8_t *raw = found ? unwrap_stored(c.data, c.size, &raw_size) : NULL;

    /* Each row is one filter byte, which must be 0, and then its pixels. */
    bool same = raw != NULL && raw_size == (size_t)height * (width + 1);
    for (uint32_t y = 0; same && y < height; y++) {
        const uint8_t *row = raw + (size_t)y * (width + 1);
        same = row[0] == 0
            && memcmp(row + 1, pixels + (size_t)y * width, width) == 0;
    }
    free(raw);
    buffer_free(&png);
    return same;
}

UTEST(png, every_pixel_of_a_small_picture_comes_back)
{
    const uint8_t pixels[12] = {
        0, 5, 6, 0,
        7, 8, 9, 10,
        0, 0, 255, 0,
    };

    ASSERT_TRUE(pixels_survive(4, 3, pixels));
}

UTEST(png, every_pixel_of_a_one_pixel_picture_comes_back)
{
    const uint8_t pixel = 200;

    ASSERT_TRUE(pixels_survive(1, 1, &pixel));
}

UTEST(png, every_pixel_of_a_picture_needing_several_blocks_comes_back)
{
    /* 300 by 300 is 90,300 bytes with the filter bytes: more than one
     * stored block of 65,535 holds, so the data is split in two. */
    static uint8_t pixels[300 * 300];
    for (size_t i = 0; i < sizeof pixels; i++) {
        pixels[i] = (uint8_t)((i * 7 + i / 300) % 256);
    }

    ASSERT_TRUE(pixels_survive(300, 300, pixels));
}

UTEST(png, a_picture_that_fills_one_block_exactly_comes_back)
{
    /* 65,535 bytes with the filter bytes: 255 rows of 256 pixels and one
     * filter byte each. Exactly one full block, and it must be marked as
     * the last. */
    static uint8_t pixels[256 * 255];
    for (size_t i = 0; i < sizeof pixels; i++) {
        pixels[i] = (uint8_t)(i % 253);
    }

    ASSERT_TRUE(pixels_survive(256, 255, pixels));
}

UTEST(png, writing_adds_to_what_is_already_in_the_buffer)
{
    const uint8_t pixels[1] = {7};
    Rgb palette[256];
    make_palette(palette);
    ByteBuffer png = {0};
    ASSERT_TRUE(buffer_push(&png, 0xab));

    ASSERT_TRUE(png_write_indexed(&png, 1, 1, pixels, palette));

    ASSERT_EQ(0xab, png.data[0]);
    ASSERT_EQ(0x89, png.data[1]);
    ASSERT_EQ('P', png.data[2]);
    buffer_free(&png);
}

UTEST(png, a_picture_with_no_width_or_height_is_refused)
{
    const uint8_t pixels[1] = {7};
    Rgb palette[256];
    make_palette(palette);
    ByteBuffer png = {0};
    ASSERT_TRUE(buffer_push(&png, 0xab));

    ASSERT_FALSE(png_write_indexed(&png, 0, 1, pixels, palette));
    ASSERT_FALSE(png_write_indexed(&png, 1, 0, pixels, palette));

    ASSERT_EQ(1u, png.size);
    ASSERT_EQ(0xab, png.data[0]);
    buffer_free(&png);
}

UTEST(png, a_picture_larger_than_the_limit_is_refused)
{
    const uint8_t pixels[1] = {7};
    Rgb palette[256];
    make_palette(palette);
    ByteBuffer png = {0};

    ASSERT_FALSE(png_write_indexed(&png, PNG_MAX_SIDE + 1, 1, pixels, palette));
    ASSERT_FALSE(png_write_indexed(&png, 1, PNG_MAX_SIDE + 1, pixels, palette));
    ASSERT_FALSE(png_write_indexed(&png, 0xffffffffu, 1, pixels, palette));

    ASSERT_EQ(0u, png.size);
    buffer_free(&png);
}
