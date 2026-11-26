#include "import/pal.h"

#include <string.h>

#include "utest.h"

/* A .pal file of the BMP kind, built by hand: the two headers, the 256
 * colours, and the pixels of a picture of 2 by 2. Colour n is blue n,
 * green 255 - n, red n / 2. */
typedef struct MakeBmp {
    uint8_t bytes[2048];
    size_t size;
} MakeBmp;

static void put_u16(uint8_t *at, uint32_t value)
{
    at[0] = (uint8_t)(value & 0xff);
    at[1] = (uint8_t)((value >> 8) & 0xff);
}

static void put_u32(uint8_t *at, uint32_t value)
{
    put_u16(at, value & 0xffff);
    put_u16(at + 2, value >> 16);
}

/* `header_size` is the size of the picture header, 40 in the game's
 * files. */
static void make_bmp(MakeBmp *m, uint32_t header_size)
{
    memset(m, 0, sizeof *m);
    uint8_t *p = m->bytes;
    size_t table = 14 + header_size;
    p[0] = 'B';
    p[1] = 'M';
    put_u32(p + 10, (uint32_t)(table + 1024));  /* where the pixels are */
    put_u32(p + 14, header_size);
    put_u32(p + 18, 2);                         /* width */
    put_u32(p + 22, 2);                         /* height */
    put_u16(p + 26, 1);                         /* planes */
    put_u16(p + 28, 8);                         /* bits for each pixel */
    put_u32(p + 46, 256);                       /* colours in the table */
    for (size_t i = 0; i < 256; i++) {
        p[table + i * 4] = (uint8_t)i;
        p[table + i * 4 + 1] = (uint8_t)(255 - i);
        p[table + i * 4 + 2] = (uint8_t)(i / 2);
        p[table + i * 4 + 3] = 0;
    }
    m->size = table + 1024 + 8;
    put_u32(p + 2, (uint32_t)m->size);
}

/* ---- pal_bmp_read ---- */

UTEST(pal, bmp_read_gives_the_colour_table_blue_first)
{
    static MakeBmp m;
    Rgb palette[256];
    make_bmp(&m, 40);

    ASSERT_TRUE(pal_bmp_read(m.bytes, m.size, palette));

    for (size_t i = 0; i < 256; i++) {
        ASSERT_EQ(i, palette[i].b);
        ASSERT_EQ(255 - i, palette[i].g);
        ASSERT_EQ(i / 2, palette[i].r);
    }
}

UTEST(pal, bmp_read_accepts_a_colour_count_of_0_for_all_256)
{
    static MakeBmp m;
    Rgb palette[256];
    make_bmp(&m, 40);
    put_u32(m.bytes + 46, 0);

    ASSERT_TRUE(pal_bmp_read(m.bytes, m.size, palette));
    ASSERT_EQ(7, palette[7].b);
}

UTEST(pal, bmp_read_finds_the_table_after_a_longer_header)
{
    static MakeBmp m;
    Rgb palette[256];
    make_bmp(&m, 108);

    ASSERT_TRUE(pal_bmp_read(m.bytes, m.size, palette));
    ASSERT_EQ(200, palette[200].b);
    ASSERT_EQ(55, palette[200].g);
    ASSERT_EQ(100, palette[200].r);
}

UTEST(pal, bmp_read_does_not_need_the_pixels)
{
    static MakeBmp m;
    Rgb palette[256];
    make_bmp(&m, 40);

    ASSERT_TRUE(pal_bmp_read(m.bytes, m.size - 8, palette));
}

UTEST(pal, bmp_read_refuses_a_file_cut_short)
{
    static MakeBmp m;
    Rgb palette[256];
    memset(palette, 9, sizeof palette);
    make_bmp(&m, 40);

    /* One byte of the colour table is missing, or more. */
    for (size_t size = 0; size < 14 + 40 + 1024; size++) {
        ASSERT_FALSE(pal_bmp_read(m.bytes, size, palette));
    }
    ASSERT_EQ(9, palette[0].r);
    ASSERT_EQ(9, palette[255].b);
}

UTEST(pal, bmp_read_refuses_a_file_that_is_not_a_bmp)
{
    static MakeBmp m;
    Rgb palette[256];
    make_bmp(&m, 40);
    m.bytes[0] = 'P';
    ASSERT_FALSE(pal_bmp_read(m.bytes, m.size, palette));

    make_bmp(&m, 40);
    m.bytes[1] = 'N';
    ASSERT_FALSE(pal_bmp_read(m.bytes, m.size, palette));
}

UTEST(pal, bmp_read_refuses_a_picture_that_is_not_8_bit)
{
    static MakeBmp m;
    Rgb palette[256];
    make_bmp(&m, 40);
    put_u16(m.bytes + 28, 24);
    ASSERT_FALSE(pal_bmp_read(m.bytes, m.size, palette));

    make_bmp(&m, 40);
    put_u16(m.bytes + 28, 4);
    ASSERT_FALSE(pal_bmp_read(m.bytes, m.size, palette));
}

UTEST(pal, bmp_read_refuses_a_table_of_fewer_colours)
{
    static MakeBmp m;
    Rgb palette[256];
    make_bmp(&m, 40);
    put_u32(m.bytes + 46, 16);

    ASSERT_FALSE(pal_bmp_read(m.bytes, m.size, palette));
}

UTEST(pal, bmp_read_refuses_a_header_that_is_too_short_or_far_too_long)
{
    static MakeBmp m;
    Rgb palette[256];
    make_bmp(&m, 40);
    put_u32(m.bytes + 14, 12);
    ASSERT_FALSE(pal_bmp_read(m.bytes, m.size, palette));

    /* A header size that points past the end of the file. */
    make_bmp(&m, 40);
    put_u32(m.bytes + 14, 0xffffffffu);
    ASSERT_FALSE(pal_bmp_read(m.bytes, m.size, palette));
}

/* ---- pal_table_count, pal_table_read ---- */

UTEST(pal, table_count_is_the_size_in_palettes)
{
    ASSERT_EQ(16u, pal_table_count(16384));
    ASSERT_EQ(1u, pal_table_count(1024));
    ASSERT_EQ(0u, pal_table_count(0));
}

UTEST(pal, table_count_is_0_for_a_size_that_does_not_fit)
{
    ASSERT_EQ(0u, pal_table_count(1023));
    ASSERT_EQ(0u, pal_table_count(1025));
    ASSERT_EQ(0u, pal_table_count(16385));
}

UTEST(pal, table_read_gives_the_palette_with_that_number)
{
    /* Three palettes. Colour n of palette p is blue p, green n, red 7. */
    static uint8_t bytes[3 * 1024];
    Rgb palette[256];
    for (size_t p = 0; p < 3; p++) {
        for (size_t n = 0; n < 256; n++) {
            uint8_t *at = bytes + p * 1024 + n * 4;
            at[0] = (uint8_t)p;
            at[1] = (uint8_t)n;
            at[2] = 7;
            at[3] = 99;
        }
    }

    ASSERT_TRUE(pal_table_read(bytes, sizeof bytes, 0, palette));
    ASSERT_EQ(0, palette[5].b);
    ASSERT_TRUE(pal_table_read(bytes, sizeof bytes, 2, palette));
    ASSERT_EQ(2, palette[0].b);
    ASSERT_EQ(0, palette[0].g);
    ASSERT_EQ(255, palette[255].g);
    ASSERT_EQ(7, palette[255].r);
}

UTEST(pal, table_read_refuses_a_number_past_the_end)
{
    static uint8_t bytes[2 * 1024];
    Rgb palette[256];
    memset(palette, 9, sizeof palette);

    ASSERT_FALSE(pal_table_read(bytes, sizeof bytes, 2, palette));
    ASSERT_FALSE(pal_table_read(bytes, sizeof bytes, 0xffffffffu, palette));
    /* A table of a size that does not fit has no palettes at all. */
    ASSERT_FALSE(pal_table_read(bytes, sizeof bytes - 1, 0, palette));
    ASSERT_EQ(9, palette[0].r);
}
