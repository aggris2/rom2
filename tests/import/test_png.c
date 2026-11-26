#include "import/png.h"

#include <string.h>

#include "base/pack.h"
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
    out->crc_ok = crc == pack_crc32(
        pack_crc32(PACK_CRC32_START, type, 4), data, size);
    return true;
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
        Chunk c = {0};
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
    Chunk c = {0};

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
    Chunk c = {0};
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
    Chunk c = {0};
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
    Chunk c = {0};
    bool found = false;
    while (!found && read_chunk(&r, &c)) {
        found = strcmp(c.type, "IDAT") == 0;
    }
    size_t expected_size = (size_t)height * (width + 1);
    ByteBuffer raw = {0};
    bool same = found
        && unpack_zlib(&raw, c.data, c.size, expected_size)
        && raw.size == expected_size;

    /* Each row is one filter byte, which must be 0, and then its pixels. */
    for (uint32_t y = 0; same && y < height; y++) {
        const uint8_t *row = raw.data + (size_t)y * (width + 1);
        same = row[0] == 0
            && memcmp(row + 1, pixels + (size_t)y * width, width) == 0;
    }
    buffer_free(&raw);
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

UTEST(png, every_pixel_of_a_large_picture_comes_back)
{
    /* 300 by 300 is 90,300 bytes with the filter bytes, more than the
     * 32,768 bytes the packer can look back over. */
    static uint8_t pixels[300 * 300];
    for (size_t i = 0; i < sizeof pixels; i++) {
        pixels[i] = (uint8_t)((i * 7 + i / 300) % 256);
    }

    ASSERT_TRUE(pixels_survive(300, 300, pixels));
}

UTEST(png, every_pixel_of_a_mostly_empty_picture_comes_back)
{
    /* Like a sprite: nearly all transparent, with a little in the
     * middle. This is what packs down to almost nothing. */
    static uint8_t pixels[256 * 255];
    memset(pixels, 0, sizeof pixels);
    for (size_t i = 0; i < 40; i++) {
        pixels[256 * 120 + 100 + i] = (uint8_t)(i + 1);
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

/* ---- png_write_rgb ---- */

UTEST(png, rgb_file_is_the_signature_and_three_chunks)
{
    const Rgb pixels[2] = {{1, 2, 3}, {4, 5, 6}};
    ByteBuffer png = {0};
    ASSERT_TRUE(png_write_rgb(&png, 2, 1, pixels));

    Reader r = reader_init(png.data, png.size);
    reader_skip(&r, 8);
    Chunk c = {0};
    ASSERT_TRUE(read_chunk(&r, &c));
    ASSERT_STREQ("IHDR", c.type);
    ASSERT_TRUE(c.crc_ok);
    ASSERT_EQ(13u, c.size);
    /* Width 2, height 1, 8 bits, colour type 2: red, green, blue. */
    ASSERT_EQ(2, c.data[3]);
    ASSERT_EQ(1, c.data[7]);
    ASSERT_EQ(8, c.data[8]);
    ASSERT_EQ(2, c.data[9]);
    ASSERT_TRUE(read_chunk(&r, &c));
    ASSERT_STREQ("IDAT", c.type);
    ASSERT_TRUE(c.crc_ok);
    ASSERT_TRUE(read_chunk(&r, &c));
    ASSERT_STREQ("IEND", c.type);
    ASSERT_TRUE(c.crc_ok);
    ASSERT_EQ(0u, reader_remaining(&r));
    buffer_free(&png);
}

UTEST(png, rgb_every_colour_comes_back_red_first)
{
    /* 3 by 2. Pixel n is (n, 100 + n, 200 + n). */
    Rgb pixels[6];
    for (uint8_t i = 0; i < 6; i++) {
        pixels[i].r = i;
        pixels[i].g = (uint8_t)(100 + i);
        pixels[i].b = (uint8_t)(200 + i);
    }
    ByteBuffer png = {0};
    ASSERT_TRUE(png_write_rgb(&png, 3, 2, pixels));

    Reader r = reader_init(png.data, png.size);
    reader_skip(&r, 8);
    Chunk c = {0};
    ASSERT_TRUE(read_chunk(&r, &c));
    ASSERT_TRUE(read_chunk(&r, &c));
    ByteBuffer raw = {0};
    ASSERT_TRUE(unpack_zlib(&raw, c.data, c.size, 20));
    /* Two rows of a filter byte and 3 pixels of 3 bytes. */
    ASSERT_EQ(20u, raw.size);
    const uint8_t expected[20] = {
        0, 0, 100, 200, 1, 101, 201, 2, 102, 202,
        0, 3, 103, 203, 4, 104, 204, 5, 105, 205
    };
    ASSERT_EQ(0, memcmp(expected, raw.data, 20));
    buffer_free(&raw);
    buffer_free(&png);
}

UTEST(png, rgb_picture_with_no_size_or_too_large_is_refused)
{
    const Rgb pixels[1] = {{1, 2, 3}};
    const uint8_t before[2] = {7, 8};
    ByteBuffer png = {0};
    ASSERT_TRUE(buffer_append(&png, before, 2));

    ASSERT_FALSE(png_write_rgb(&png, 0, 1, pixels));
    ASSERT_FALSE(png_write_rgb(&png, 1, 0, pixels));
    ASSERT_FALSE(png_write_rgb(&png, PNG_MAX_SIDE + 1, 1, pixels));
    ASSERT_FALSE(png_write_rgb(&png, 1, PNG_MAX_SIDE + 1, pixels));

    ASSERT_EQ(2u, png.size);
    buffer_free(&png);
}

/* ---- png_write_grey ---- */

UTEST(png, grey_every_shade_comes_back)
{
    /* 3 by 2. */
    const uint8_t pixels[6] = {0, 17, 128, 200, 254, 255};
    ByteBuffer png = {0};
    ASSERT_TRUE(png_write_grey(&png, 3, 2, pixels));

    Reader r = reader_init(png.data, png.size);
    reader_skip(&r, 8);
    Chunk c = {0};
    ASSERT_TRUE(read_chunk(&r, &c));
    ASSERT_STREQ("IHDR", c.type);
    ASSERT_TRUE(c.crc_ok);
    /* Width 3, height 2, 8 bits, colour type 0: shades of grey. */
    ASSERT_EQ(3, c.data[3]);
    ASSERT_EQ(2, c.data[7]);
    ASSERT_EQ(8, c.data[8]);
    ASSERT_EQ(0, c.data[9]);
    ASSERT_TRUE(read_chunk(&r, &c));
    ASSERT_STREQ("IDAT", c.type);
    ByteBuffer raw = {0};
    ASSERT_TRUE(unpack_zlib(&raw, c.data, c.size, 8));
    /* Two rows of a filter byte and 3 pixels. */
    ASSERT_EQ(8u, raw.size);
    const uint8_t expected[8] = {0, 0, 17, 128, 0, 200, 254, 255};
    ASSERT_EQ(0, memcmp(expected, raw.data, 8));
    ASSERT_TRUE(read_chunk(&r, &c));
    ASSERT_STREQ("IEND", c.type);
    ASSERT_EQ(0u, reader_remaining(&r));
    buffer_free(&raw);
    buffer_free(&png);
}

UTEST(png, grey_picture_with_no_size_or_too_large_is_refused)
{
    const uint8_t pixels[1] = {1};
    const uint8_t before[2] = {7, 8};
    ByteBuffer png = {0};
    ASSERT_TRUE(buffer_append(&png, before, 2));

    ASSERT_FALSE(png_write_grey(&png, 0, 1, pixels));
    ASSERT_FALSE(png_write_grey(&png, 1, 0, pixels));
    ASSERT_FALSE(png_write_grey(&png, PNG_MAX_SIDE + 1, 1, pixels));
    ASSERT_FALSE(png_write_grey(&png, 1, PNG_MAX_SIDE + 1, pixels));

    ASSERT_EQ(2u, png.size);
    buffer_free(&png);
}

/* ---- png_write_indexed_solid ---- */

UTEST(png, indexed_solid_has_no_transparency_chunk)
{
    const uint8_t pixels[4] = {0, 1, 2, 0};
    Rgb palette[256];
    make_palette(palette);
    ByteBuffer png = {0};
    ASSERT_TRUE(png_write_indexed_solid(&png, 2, 2, pixels, palette));

    /* The header, the palette, the pixels and the end: no tRNS. */
    Reader r = reader_init(png.data, png.size);
    reader_skip(&r, 8);
    Chunk c = {0};
    ASSERT_TRUE(read_chunk(&r, &c));
    ASSERT_STREQ("IHDR", c.type);
    ASSERT_EQ(3, c.data[9]);
    ASSERT_TRUE(read_chunk(&r, &c));
    ASSERT_STREQ("PLTE", c.type);
    ASSERT_TRUE(read_chunk(&r, &c));
    ASSERT_STREQ("IDAT", c.type);
    ASSERT_TRUE(c.crc_ok);
    ByteBuffer raw = {0};
    ASSERT_TRUE(unpack_zlib(&raw, c.data, c.size, 6));
    const uint8_t expected[6] = {0, 0, 1, 0, 2, 0};
    ASSERT_EQ(6u, raw.size);
    ASSERT_EQ(0, memcmp(expected, raw.data, 6));
    ASSERT_TRUE(read_chunk(&r, &c));
    ASSERT_STREQ("IEND", c.type);
    ASSERT_EQ(0u, reader_remaining(&r));
    buffer_free(&raw);
    buffer_free(&png);
}

UTEST(png, indexed_solid_refuses_a_picture_of_no_size)
{
    const uint8_t pixels[1] = {0};
    Rgb palette[256];
    make_palette(palette);
    ByteBuffer png = {0};
    ASSERT_FALSE(png_write_indexed_solid(&png, 0, 1, pixels, palette));
    ASSERT_FALSE(png_write_indexed_solid(&png, 1, PNG_MAX_SIDE + 1, pixels,
        palette));
    ASSERT_EQ(0u, png.size);
    buffer_free(&png);
}
