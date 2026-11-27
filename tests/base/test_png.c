#include "base/png.h"

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

/* ---- png_read ---- */

/* Adds one chunk to a file being built by hand, with its CRC. */
static void put_chunk(ByteBuffer *out, const char type[4],
    const uint8_t *data, size_t size)
{
    uint32_t crc = pack_crc32(PACK_CRC32_START, (const uint8_t *)type, 4);
    crc = pack_crc32(crc, data, size);
    buffer_u32_be(out, (uint32_t)size);
    buffer_append(out, (const uint8_t *)type, 4);
    buffer_append(out, data, size);
    buffer_u32_be(out, crc);
}

/* Builds a PNG by hand: a header, the chunks in `middle`, the rows
 * packed into `pieces` chunks, and the end. `rows` is each row with its
 * storing byte before it. */
static void build_png(ByteBuffer *out, uint32_t width, uint32_t height,
    uint8_t depth, uint8_t colour, uint8_t interlace,
    const ByteBuffer *middle, const uint8_t *rows, size_t rows_size,
    int pieces)
{
    static const uint8_t signature[8] = {
        0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a
    };
    ByteBuffer header = {0};
    ByteBuffer packed = {0};
    out->size = 0;
    buffer_append(out, signature, 8);
    buffer_u32_be(&header, width);
    buffer_u32_be(&header, height);
    buffer_push(&header, depth);
    buffer_push(&header, colour);
    buffer_push(&header, 0);
    buffer_push(&header, 0);
    buffer_push(&header, interlace);
    put_chunk(out, "IHDR", header.data, header.size);
    if (middle != NULL) {
        buffer_append(out, middle->data, middle->size);
    }
    pack_zlib(&packed, rows, rows_size);
    size_t at = 0;
    for (int i = 0; i < pieces; i++) {
        size_t part = i == pieces - 1 ? packed.size - at
            : packed.size / (size_t)pieces;
        put_chunk(out, "IDAT", packed.data + at, part);
        at += part;
    }
    put_chunk(out, "IEND", NULL, 0);
    buffer_free(&header);
    buffer_free(&packed);
}

UTEST(png, read_gives_back_an_indexed_picture_and_its_palette)
{
    Rgb palette[256];
    const uint8_t pixels[6] = {0, 1, 2, 253, 254, 255};
    ByteBuffer file = {0};
    Png png;
    make_palette(palette);
    ASSERT_TRUE(png_write_indexed(&file, 3, 2, pixels, palette));

    ASSERT_TRUE(png_read(file.data, file.size, &png));

    ASSERT_EQ(3u, png.width);
    ASSERT_EQ(2u, png.height);
    ASSERT_EQ((int)PNG_INDEXED, (int)png.kind);
    ASSERT_EQ(0, memcmp(png.pixels, pixels, 6));
    ASSERT_EQ(0, memcmp(png.palette, palette, sizeof palette));
    /* Index 0 is seen through, and no other. */
    ASSERT_EQ(0, png.alpha[0]);
    ASSERT_EQ(255, png.alpha[1]);
    ASSERT_EQ(255, png.alpha[255]);
    png_free(&png);
    ASSERT_TRUE(png.pixels == NULL);
    buffer_free(&file);
}

UTEST(png, read_of_a_solid_indexed_picture_has_every_index_solid)
{
    Rgb palette[256];
    const uint8_t pixels[2] = {0, 9};
    ByteBuffer file = {0};
    Png png;
    make_palette(palette);
    ASSERT_TRUE(png_write_indexed_solid(&file, 2, 1, pixels, palette));

    ASSERT_TRUE(png_read(file.data, file.size, &png));

    ASSERT_EQ(255, png.alpha[0]);
    ASSERT_EQ(0, png.pixels[0]);
    ASSERT_EQ(9, png.pixels[1]);
    png_free(&png);
    buffer_free(&file);
}

UTEST(png, read_gives_back_a_grey_and_a_full_colour_picture)
{
    const uint8_t grey[4] = {0, 17, 128, 255};
    const Rgb colours[2] = {{1, 2, 3}, {250, 251, 252}};
    ByteBuffer file = {0};
    Png png;

    ASSERT_TRUE(png_write_grey(&file, 2, 2, grey));
    ASSERT_TRUE(png_read(file.data, file.size, &png));
    ASSERT_EQ((int)PNG_GREY, (int)png.kind);
    ASSERT_EQ(0, memcmp(png.pixels, grey, 4));
    png_free(&png);

    file.size = 0;
    ASSERT_TRUE(png_write_rgb(&file, 1, 2, colours));
    ASSERT_TRUE(png_read(file.data, file.size, &png));
    ASSERT_EQ((int)PNG_RGB, (int)png.kind);
    ASSERT_EQ(1u, png.width);
    ASSERT_EQ(2u, png.height);
    /* Three bytes for a pixel, red first. */
    ASSERT_EQ(1, png.pixels[0]);
    ASSERT_EQ(3, png.pixels[2]);
    ASSERT_EQ(250, png.pixels[3]);
    ASSERT_EQ(252, png.pixels[5]);
    png_free(&png);
    buffer_free(&file);
}

UTEST(png, read_of_a_large_picture_gives_every_pixel_back)
{
    static uint8_t pixels[300 * 200];
    Rgb palette[256];
    ByteBuffer file = {0};
    Png png;
    make_palette(palette);
    for (size_t i = 0; i < sizeof pixels; i++) {
        pixels[i] = (uint8_t)((i * 7 + i / 300) & 0xff);
    }
    ASSERT_TRUE(png_write_indexed(&file, 300, 200, pixels, palette));

    ASSERT_TRUE(png_read(file.data, file.size, &png));

    ASSERT_EQ(0, memcmp(png.pixels, pixels, sizeof pixels));
    png_free(&png);
    buffer_free(&file);
}

UTEST(png, read_undoes_each_of_the_five_ways_a_row_is_stored)
{
    /* A grey picture of 3 by 5, a row stored each way. The picture is
     *     10  20  40
     *     10  25  45
     *     12  30  50
     *     20  40  60
     *     21  39  70
     * Row 0 as it is. Row 1 as the difference from the byte to the
     * left. Row 2 from the byte above. Row 3 from the mean of left and
     * above, rounded down. Row 4 by PNG's guess from left, above and
     * above-left. */
    const uint8_t rows[] = {
        0, 10, 20, 40,
        1, 10, 15, 20,
        2, 2, 5, 5,
        /* means: (0+12)/2 = 6, (20+30)/2 = 25, (40+50)/2 = 45 */
        3, 14, 15, 15,
        /* guesses: 20 (above), then of 21, 40, 20 it is 40 (41 is
         * nearest 40), then of 39, 60, 40 it is 60 (59 is nearest
         * 60) */
        4, 1, 255, 10
    };
    const uint8_t expected[15] = {
        10, 20, 40, 10, 25, 45, 12, 30, 50, 20, 40, 60, 21, 39, 70
    };
    ByteBuffer file = {0};
    Png png;
    build_png(&file, 3, 5, 8, 0, 0, NULL, rows, sizeof rows, 1);

    ASSERT_TRUE(png_read(file.data, file.size, &png));

    ASSERT_EQ(0, memcmp(png.pixels, expected, 15));
    png_free(&png);
    buffer_free(&file);
}

UTEST(png, read_rounds_the_mean_down_and_breaks_a_tie_towards_above)
{
    /* Grey, 2 by 2, the first row 10 and 30 as they are, the second
     * stored as differences from the mean of left and above. */
    const uint8_t by_mean[] = {
        0, 10, 30,
        3, 2, 0
    };
    ByteBuffer file = {0};
    Png png;
    build_png(&file, 2, 2, 8, 0, 0, NULL, by_mean, sizeof by_mean, 1);
    ASSERT_TRUE(png_read(file.data, file.size, &png));
    /* 10 / 2 is 5, and 2 more is 7. Then 7 and 30 are 37, halved and
     * rounded down 18. */
    ASSERT_EQ(7, png.pixels[2]);
    ASSERT_EQ(18, png.pixels[3]);
    png_free(&png);

    /* By PNG's guess: the first pixel has only the 10 above. The
     * second has 0 to its left, 30 above and 10 above-left: left plus
     * above less above-left is 20, as far from 30 as from 10, and the
     * one above is taken. */
    const uint8_t by_guess[] = {
        0, 10, 30,
        4, 246, 5
    };
    build_png(&file, 2, 2, 8, 0, 0, NULL, by_guess, sizeof by_guess, 1);
    ASSERT_TRUE(png_read(file.data, file.size, &png));
    ASSERT_EQ(0, png.pixels[2]);
    ASSERT_EQ(35, png.pixels[3]);
    png_free(&png);
    buffer_free(&file);
}

UTEST(png, read_takes_the_byte_of_the_pixel_before_in_full_colour)
{
    /* Full colour, 2 by 1, stored as differences from the left: the
     * byte to the left is the same colour of the pixel before, three
     * bytes back. (10, 20, 30) and then (11, 22, 33). */
    const uint8_t rows[] = {1, 10, 20, 30, 1, 2, 3};
    ByteBuffer file = {0};
    Png png;
    build_png(&file, 2, 1, 8, 2, 0, NULL, rows, sizeof rows, 1);

    ASSERT_TRUE(png_read(file.data, file.size, &png));

    ASSERT_EQ(11, png.pixels[3]);
    ASSERT_EQ(22, png.pixels[4]);
    ASSERT_EQ(33, png.pixels[5]);
    png_free(&png);
    buffer_free(&file);
}

UTEST(png, read_joins_pixels_cut_into_several_chunks)
{
    const uint8_t rows[] = {0, 1, 2, 3, 0, 4, 5, 6, 0, 7, 8, 9};
    const uint8_t expected[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    ByteBuffer file = {0};
    Png png;
    build_png(&file, 3, 3, 8, 0, 0, NULL, rows, sizeof rows, 3);

    ASSERT_TRUE(png_read(file.data, file.size, &png));

    ASSERT_EQ(0, memcmp(png.pixels, expected, 9));
    png_free(&png);
    buffer_free(&file);
}

UTEST(png, read_takes_a_short_palette_and_passes_over_unknown_chunks)
{
    /* A palette of two colours, as an editor may save it, a chunk of
     * text before it, and solidness for the first two indices. */
    const uint8_t colours[6] = {1, 2, 3, 4, 5, 6};
    const uint8_t solidness[2] = {7, 8};
    const uint8_t rows[] = {0, 1, 0};
    ByteBuffer middle = {0};
    ByteBuffer file = {0};
    Png png;
    put_chunk(&middle, "tEXt", (const uint8_t *)"a\0b", 3);
    put_chunk(&middle, "PLTE", colours, 6);
    put_chunk(&middle, "tRNS", solidness, 2);
    build_png(&file, 2, 1, 8, 3, 0, &middle, rows, sizeof rows, 1);

    ASSERT_TRUE(png_read(file.data, file.size, &png));

    ASSERT_EQ(4, png.palette[1].r);
    ASSERT_EQ(6, png.palette[1].b);
    /* The rest of the palette is black and solid. */
    ASSERT_EQ(0, png.palette[2].r);
    ASSERT_EQ(7, png.alpha[0]);
    ASSERT_EQ(8, png.alpha[1]);
    ASSERT_EQ(255, png.alpha[2]);
    ASSERT_EQ(1, png.pixels[0]);
    png_free(&png);
    buffer_free(&middle);
    buffer_free(&file);
}

/* A picture that has not been written to. */
static Png untouched_png(void)
{
    Png png;
    memset(&png, 0x5a, sizeof png);
    return png;
}

static bool png_is_untouched(const Png *png)
{
    Png same = untouched_png();
    return memcmp(png, &same, sizeof same) == 0;
}

UTEST(png, read_of_a_file_cut_short_fails_at_every_length)
{
    Rgb palette[256];
    const uint8_t pixels[6] = {0, 1, 2, 3, 4, 5};
    ByteBuffer file = {0};
    Png png = untouched_png();
    make_palette(palette);
    ASSERT_TRUE(png_write_indexed(&file, 3, 2, pixels, palette));

    for (size_t size = 0; size < file.size; size++) {
        ASSERT_FALSE(png_read(file.data, size, &png));
    }
    ASSERT_TRUE(png_is_untouched(&png));
    buffer_free(&file);
}

UTEST(png, read_of_a_damaged_file_fails_whichever_byte_is_changed)
{
    /* Every chunk has a check over its type and its data, the packed
     * pixels have one of their own, and the sizes must add up: no one
     * changed byte gets through. */
    const uint8_t grey[4] = {1, 2, 3, 4};
    ByteBuffer file = {0};
    Png png = untouched_png();
    ASSERT_TRUE(png_write_grey(&file, 2, 2, grey));

    for (size_t i = 0; i < file.size; i++) {
        file.data[i] ^= 0x40;
        ASSERT_FALSE(png_read(file.data, file.size, &png));
        file.data[i] ^= 0x40;
    }
    ASSERT_TRUE(png_is_untouched(&png));
    ASSERT_TRUE(png_read(file.data, file.size, &png));
    png_free(&png);
    buffer_free(&file);
}

UTEST(png, read_refuses_the_kinds_of_picture_it_does_not_know)
{
    const uint8_t rows[32] = {0};
    ByteBuffer file = {0};
    Png png = untouched_png();

    /* 16 bits, and 4 bits. */
    build_png(&file, 1, 1, 16, 0, 0, NULL, rows, 3, 1);
    ASSERT_FALSE(png_read(file.data, file.size, &png));
    build_png(&file, 1, 1, 4, 0, 0, NULL, rows, 2, 1);
    ASSERT_FALSE(png_read(file.data, file.size, &png));
    /* Grey with solidness, and full colour with solidness. */
    build_png(&file, 1, 1, 8, 4, 0, NULL, rows, 3, 1);
    ASSERT_FALSE(png_read(file.data, file.size, &png));
    build_png(&file, 1, 1, 8, 6, 0, NULL, rows, 5, 1);
    ASSERT_FALSE(png_read(file.data, file.size, &png));
    /* Interlaced. */
    build_png(&file, 1, 1, 8, 0, 1, NULL, rows, 2, 1);
    ASSERT_FALSE(png_read(file.data, file.size, &png));
    /* Indexed without a palette. */
    build_png(&file, 1, 1, 8, 3, 0, NULL, rows, 2, 1);
    ASSERT_FALSE(png_read(file.data, file.size, &png));
    /* A row stored in a sixth way. */
    const uint8_t sixth[] = {5, 0};
    build_png(&file, 1, 1, 8, 0, 0, NULL, sixth, 2, 1);
    ASSERT_FALSE(png_read(file.data, file.size, &png));
    ASSERT_TRUE(png_is_untouched(&png));
    buffer_free(&file);
}

UTEST(png, read_refuses_pixels_that_are_too_few_or_too_many)
{
    const uint8_t rows[] = {0, 1, 2, 0, 3, 4};
    ByteBuffer file = {0};
    Png png = untouched_png();

    /* 2 by 2 needs 6 bytes of rows: 5 are too few, and the rows of a
     * 2 by 2 are too many for a 2 by 1. */
    build_png(&file, 2, 2, 8, 0, 0, NULL, rows, 5, 1);
    ASSERT_FALSE(png_read(file.data, file.size, &png));
    build_png(&file, 2, 1, 8, 0, 0, NULL, rows, 6, 1);
    ASSERT_FALSE(png_read(file.data, file.size, &png));
    ASSERT_TRUE(png_is_untouched(&png));

    build_png(&file, 2, 2, 8, 0, 0, NULL, rows, 6, 1);
    ASSERT_TRUE(png_read(file.data, file.size, &png));
    png_free(&png);
    buffer_free(&file);
}

UTEST(png, read_refuses_a_picture_of_no_size_or_larger_than_the_most)
{
    const uint8_t rows[] = {0, 1};
    ByteBuffer file = {0};
    Png png = untouched_png();

    build_png(&file, 0, 1, 8, 0, 0, NULL, rows, 2, 1);
    ASSERT_FALSE(png_read(file.data, file.size, &png));
    build_png(&file, 1, 0, 8, 0, 0, NULL, rows, 2, 1);
    ASSERT_FALSE(png_read(file.data, file.size, &png));
    build_png(&file, PNG_MAX_SIDE + 1, 1, 8, 0, 0, NULL, rows, 2, 1);
    ASSERT_FALSE(png_read(file.data, file.size, &png));
    build_png(&file, 1, PNG_MAX_SIDE + 1, 8, 0, 0, NULL, rows, 2, 1);
    ASSERT_FALSE(png_read(file.data, file.size, &png));
    ASSERT_TRUE(png_is_untouched(&png));
    buffer_free(&file);
}

UTEST(png, read_refuses_what_is_not_a_png)
{
    const uint8_t text[] = "This is not a picture at all, only words.";
    Png png = untouched_png();

    ASSERT_FALSE(png_read(text, sizeof text, &png));
    ASSERT_FALSE(png_read(NULL, 0, &png));
    ASSERT_TRUE(png_is_untouched(&png));
}
