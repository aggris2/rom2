#include "import/sprite256.h"

#include <string.h>

#include "utest.h"

/* Writes a trailer into the last 4 bytes of a block, low byte first. */
static void put_trailer(uint8_t *bytes, size_t size, uint32_t trailer)
{
    bytes[size - 4] = (uint8_t)(trailer & 0xff);
    bytes[size - 3] = (uint8_t)((trailer >> 8) & 0xff);
    bytes[size - 2] = (uint8_t)((trailer >> 16) & 0xff);
    bytes[size - 1] = (uint8_t)((trailer >> 24) & 0xff);
}

/* A Sprite256 that has not been written to. A failed open must leave it
 * exactly like this. */
static Sprite256 untouched_sprite(void)
{
    Sprite256 s = {
        .data = NULL,
        .size = 12345,
        .frame_count = 0xaaaaaaaau,
        .has_palette = true
    };
    return s;
}

static bool sprite_is_untouched(const Sprite256 *s)
{
    return s->data == NULL
        && s->size == 12345
        && s->frame_count == 0xaaaaaaaau
        && s->has_palette;
}

/* ---- sprite256_open ---- */

UTEST(sprite256, open_reads_the_frame_count_and_the_palette_flag)
{
    /* A palette, room for exactly three frame headers, and the trailer. */
    uint8_t bytes[1024 + 3 * 12 + 4];
    memset(bytes, 0, sizeof bytes);
    put_trailer(bytes, sizeof bytes, 0x80000003u);
    Sprite256 s = untouched_sprite();
    s.has_palette = false;

    ASSERT_TRUE(sprite256_open(bytes, sizeof bytes, &s));

    ASSERT_TRUE(s.data == bytes);
    ASSERT_EQ(sizeof bytes, s.size);
    ASSERT_EQ(3u, s.frame_count);
    ASSERT_TRUE(s.has_palette);
}

UTEST(sprite256, open_of_a_file_without_a_palette)
{
    /* Top bit clear: the frames start at byte 0. */
    uint8_t bytes[2 * 12 + 4];
    memset(bytes, 0, sizeof bytes);
    put_trailer(bytes, sizeof bytes, 2);
    Sprite256 s = untouched_sprite();

    ASSERT_TRUE(sprite256_open(bytes, sizeof bytes, &s));

    ASSERT_EQ(2u, s.frame_count);
    ASSERT_FALSE(s.has_palette);
}

UTEST(sprite256, open_of_a_file_with_no_frames)
{
    uint8_t bytes[4];
    put_trailer(bytes, sizeof bytes, 0);
    Sprite256 s = untouched_sprite();

    ASSERT_TRUE(sprite256_open(bytes, sizeof bytes, &s));

    ASSERT_EQ(0u, s.frame_count);
    ASSERT_FALSE(s.has_palette);
}

UTEST(sprite256, open_of_a_palette_and_no_frames)
{
    uint8_t bytes[1024 + 4];
    memset(bytes, 0, sizeof bytes);
    put_trailer(bytes, sizeof bytes, 0x80000000u);
    Sprite256 s = untouched_sprite();
    s.has_palette = false;

    ASSERT_TRUE(sprite256_open(bytes, sizeof bytes, &s));

    ASSERT_EQ(0u, s.frame_count);
    ASSERT_TRUE(s.has_palette);
}

UTEST(sprite256, open_ignores_unused_bytes_before_the_trailer)
{
    /* Five original files have a few bytes left over after the last
     * frame. One frame header and 7 spare bytes here. */
    uint8_t bytes[12 + 7 + 4];
    memset(bytes, 0, sizeof bytes);
    put_trailer(bytes, sizeof bytes, 1);
    Sprite256 s = untouched_sprite();

    ASSERT_TRUE(sprite256_open(bytes, sizeof bytes, &s));

    ASSERT_EQ(1u, s.frame_count);
}

UTEST(sprite256, open_of_an_empty_file_fails)
{
    /* Eight original cursor files are 0 bytes long. */
    uint8_t byte = 0;
    Sprite256 s = untouched_sprite();

    ASSERT_FALSE(sprite256_open(&byte, 0, &s));
    ASSERT_FALSE(sprite256_open(NULL, 0, &s));
    ASSERT_TRUE(sprite_is_untouched(&s));
}

UTEST(sprite256, open_of_three_bytes_fails)
{
    const uint8_t bytes[3] = {1, 0, 0};
    Sprite256 s = untouched_sprite();

    ASSERT_FALSE(sprite256_open(bytes, sizeof bytes, &s));
    ASSERT_TRUE(sprite_is_untouched(&s));
}

UTEST(sprite256, open_with_a_palette_flag_but_no_room_for_one_fails)
{
    /* One byte short of a palette and a trailer. */
    uint8_t bytes[1024 + 4 - 1];
    memset(bytes, 0, sizeof bytes);
    put_trailer(bytes, sizeof bytes, 0x80000000u);
    Sprite256 s = untouched_sprite();

    ASSERT_FALSE(sprite256_open(bytes, sizeof bytes, &s));
    ASSERT_TRUE(sprite_is_untouched(&s));
}

UTEST(sprite256, open_with_one_frame_too_many_fails)
{
    /* Room for three frame headers, and the trailer claims four. */
    uint8_t bytes[1024 + 3 * 12 + 4];
    memset(bytes, 0, sizeof bytes);
    put_trailer(bytes, sizeof bytes, 0x80000004u);
    Sprite256 s = untouched_sprite();

    ASSERT_FALSE(sprite256_open(bytes, sizeof bytes, &s));
    ASSERT_TRUE(sprite_is_untouched(&s));
}

UTEST(sprite256, open_with_room_one_byte_short_of_a_frame_fails)
{
    uint8_t bytes[12 - 1 + 4];
    memset(bytes, 0, sizeof bytes);
    put_trailer(bytes, sizeof bytes, 1);
    Sprite256 s = untouched_sprite();

    ASSERT_FALSE(sprite256_open(bytes, sizeof bytes, &s));
    ASSERT_TRUE(sprite_is_untouched(&s));
}

UTEST(sprite256, open_with_a_huge_frame_count_fails)
{
    /* 0x15555557 * 12 wraps round to 20 in 32 bits, and 20 bytes is less
     * than the room this file has for frames. */
    uint8_t bytes[2 * 12 + 4];
    memset(bytes, 0, sizeof bytes);
    put_trailer(bytes, sizeof bytes, 0x15555557u);
    Sprite256 s = untouched_sprite();

    ASSERT_FALSE(sprite256_open(bytes, sizeof bytes, &s));
    ASSERT_TRUE(sprite_is_untouched(&s));
}

/* ---- sprite256_palette ---- */

UTEST(sprite256, palette_reads_blue_green_red_and_skips_the_fourth_byte)
{
    /* A palette whose every byte is different from its neighbours, so a
     * wrong order or a wrong step shows. Colour n is stored as the four
     * bytes n, n + 1, n + 2 and 0xee. */
    uint8_t bytes[1024 + 4];
    for (size_t i = 0; i < 256; i++) {
        bytes[i * 4] = (uint8_t)i;
        bytes[i * 4 + 1] = (uint8_t)(i + 1);
        bytes[i * 4 + 2] = (uint8_t)(i + 2);
        bytes[i * 4 + 3] = 0xee;
    }
    put_trailer(bytes, sizeof bytes, 0x80000000u);
    Sprite256 s;
    ASSERT_TRUE(sprite256_open(bytes, sizeof bytes, &s));
    Rgb palette[256];
    memset(palette, 0x55, sizeof palette);

    ASSERT_TRUE(sprite256_palette(&s, palette));

    ASSERT_EQ(0, palette[0].b);
    ASSERT_EQ(1, palette[0].g);
    ASSERT_EQ(2, palette[0].r);
    ASSERT_EQ(100, palette[100].b);
    ASSERT_EQ(101, palette[100].g);
    ASSERT_EQ(102, palette[100].r);
    ASSERT_EQ(255, palette[255].b);
    ASSERT_EQ(0, palette[255].g);
    ASSERT_EQ(1, palette[255].r);
}

UTEST(sprite256, palette_of_a_sprite_without_one_fails)
{
    uint8_t bytes[12 + 4];
    memset(bytes, 0, sizeof bytes);
    put_trailer(bytes, sizeof bytes, 1);
    Sprite256 s;
    ASSERT_TRUE(sprite256_open(bytes, sizeof bytes, &s));
    Rgb palette[256];
    memset(palette, 0x55, sizeof palette);

    ASSERT_FALSE(sprite256_palette(&s, palette));

    ASSERT_EQ(0x55, palette[0].r);
    ASSERT_EQ(0x55, palette[255].b);
}

/* ---- sprite256_frames, sprite256_frame_read and frame256_free ---- */

/* The code bytes used below. The top two bits are the kind, the low six
 * bits the count. */
#define PIXELS(k) ((uint8_t)(k))
#define SKIP_ROWS(k) ((uint8_t)(0x40 | (k)))
#define SKIP_PIXELS(k) ((uint8_t)(0x80 | (k)))
#define UNKNOWN_KIND(k) ((uint8_t)(0xc0 | (k)))

/* Writes a frame header at the start of `bytes`: width, height and the
 * number of code bytes, each a 32-bit number, low byte first. */
static void put_frame_header(uint8_t *bytes, uint32_t width,
    uint32_t height, uint32_t data_size)
{
    const uint32_t values[3] = {width, height, data_size};
    for (size_t i = 0; i < 3; i++) {
        bytes[i * 4] = (uint8_t)(values[i] & 0xff);
        bytes[i * 4 + 1] = (uint8_t)((values[i] >> 8) & 0xff);
        bytes[i * 4 + 2] = (uint8_t)((values[i] >> 16) & 0xff);
        bytes[i * 4 + 3] = (uint8_t)((values[i] >> 24) & 0xff);
    }
}

/* Builds one frame in `bytes`: the header, then `codes`. Returns the
 * number of bytes written. `bytes` must have room for 12 more than
 * `code_count`. */
static size_t make_frame(uint8_t *bytes, uint32_t width, uint32_t height,
    const uint8_t *codes, size_t code_count)
{
    put_frame_header(bytes, width, height, (uint32_t)code_count);
    if (code_count > 0) {
        memcpy(bytes + 12, codes, code_count);
    }
    return 12 + code_count;
}

/* A frame that has not been written to. */
static Frame256 untouched_frame(void)
{
    Frame256 f = {.width = 0xaaaaaaaau, .height = 0xaaaaaaaau, .pixels = NULL};
    return f;
}

static bool frame_is_untouched(const Frame256 *f)
{
    return f->width == 0xaaaaaaaau
        && f->height == 0xaaaaaaaau
        && f->pixels == NULL;
}

UTEST(sprite256, frames_start_after_the_palette)
{
    uint8_t bytes[1024 + 12 + 4];
    memset(bytes, 0, sizeof bytes);
    bytes[1024] = 0x77;
    put_trailer(bytes, sizeof bytes, 0x80000001u);
    Sprite256 s;
    ASSERT_TRUE(sprite256_open(bytes, sizeof bytes, &s));

    Reader r = sprite256_frames(&s);

    ASSERT_EQ(12u, reader_remaining(&r));
    ASSERT_EQ(0x77, reader_u8(&r));
}

UTEST(sprite256, frames_start_at_byte_0_without_a_palette)
{
    uint8_t bytes[12 + 4];
    memset(bytes, 0, sizeof bytes);
    bytes[0] = 0x77;
    put_trailer(bytes, sizeof bytes, 1);
    Sprite256 s;
    ASSERT_TRUE(sprite256_open(bytes, sizeof bytes, &s));

    Reader r = sprite256_frames(&s);

    ASSERT_EQ(12u, reader_remaining(&r));
    ASSERT_EQ(0x77, reader_u8(&r));
}

UTEST(sprite256, frame_read_decodes_all_three_kinds_of_code)
{
    /* A 4 by 4 frame:
     *
     *     row 0   .  5  6  .      skip 1, two pixels, skip 1
     *     row 1   .  .  .  .      skip a whole row
     *     row 2   7  8  9 10      four pixels
     *     row 3   .  . 11  .      skip 2, one pixel, skip 1
     */
    const uint8_t codes[] = {
        SKIP_PIXELS(1), PIXELS(2), 5, 6, SKIP_PIXELS(1),
        SKIP_ROWS(1),
        PIXELS(4), 7, 8, 9, 10,
        SKIP_PIXELS(2), PIXELS(1), 11, SKIP_PIXELS(1),
    };
    const uint8_t expected[16] = {
        0, 5, 6, 0,
        0, 0, 0, 0,
        7, 8, 9, 10,
        0, 0, 11, 0,
    };
    uint8_t bytes[12 + sizeof codes];
    size_t size = make_frame(bytes, 4, 4, codes, sizeof codes);
    Reader r = reader_init(bytes, size);
    Frame256 f = untouched_frame();

    ASSERT_TRUE(sprite256_frame_read(&r, &f));

    ASSERT_EQ(4u, f.width);
    ASSERT_EQ(4u, f.height);
    for (size_t i = 0; i < sizeof expected; i++) {
        ASSERT_EQ(expected[i], f.pixels[i]);
    }
    frame256_free(&f);
}

UTEST(sprite256, frame_read_moves_to_the_next_frame)
{
    /* Two frames one after the other: 2 by 1, then 1 by 1. */
    const uint8_t first[] = {PIXELS(2), 1, 2};
    const uint8_t second[] = {PIXELS(1), 3};
    uint8_t bytes[2 * 12 + sizeof first + sizeof second];
    size_t size = make_frame(bytes, 2, 1, first, sizeof first);
    size += make_frame(bytes + size, 1, 1, second, sizeof second);
    Reader r = reader_init(bytes, size);
    Frame256 f = untouched_frame();

    ASSERT_TRUE(sprite256_frame_read(&r, &f));
    ASSERT_EQ(2u, f.width);
    ASSERT_EQ(2, f.pixels[1]);
    frame256_free(&f);

    ASSERT_TRUE(sprite256_frame_read(&r, &f));
    ASSERT_EQ(1u, f.width);
    ASSERT_EQ(3, f.pixels[0]);
    frame256_free(&f);

    ASSERT_EQ(0u, reader_remaining(&r));
    ASSERT_TRUE(reader_ok(&r));
}

UTEST(sprite256, frame_read_allows_a_code_with_a_count_of_zero)
{
    /* The original files have 1,493 of these. They do nothing. */
    const uint8_t codes[] = {PIXELS(0), PIXELS(2), 4, 5, PIXELS(0)};
    uint8_t bytes[12 + sizeof codes];
    size_t size = make_frame(bytes, 2, 1, codes, sizeof codes);
    Reader r = reader_init(bytes, size);
    Frame256 f = untouched_frame();

    ASSERT_TRUE(sprite256_frame_read(&r, &f));

    ASSERT_EQ(4, f.pixels[0]);
    ASSERT_EQ(5, f.pixels[1]);
    frame256_free(&f);
}

UTEST(sprite256, frame_read_of_a_frame_with_no_pixels)
{
    uint8_t bytes[12];
    size_t size = make_frame(bytes, 0, 0, NULL, 0);
    Reader r = reader_init(bytes, size);
    Frame256 f = untouched_frame();

    ASSERT_TRUE(sprite256_frame_read(&r, &f));

    ASSERT_EQ(0u, f.width);
    ASSERT_EQ(0u, f.height);
    ASSERT_TRUE(f.pixels != NULL);
    frame256_free(&f);
}

UTEST(sprite256, frame_free_clears_the_frame)
{
    const uint8_t codes[] = {PIXELS(1), 9};
    uint8_t bytes[12 + sizeof codes];
    size_t size = make_frame(bytes, 1, 1, codes, sizeof codes);
    Reader r = reader_init(bytes, size);
    Frame256 f = untouched_frame();
    ASSERT_TRUE(sprite256_frame_read(&r, &f));

    frame256_free(&f);

    ASSERT_TRUE(f.pixels == NULL);
    ASSERT_EQ(0u, f.width);
    ASSERT_EQ(0u, f.height);
    frame256_free(&f); /* a second time must be harmless */
}

UTEST(sprite256, frame_read_with_a_cut_off_header_fails)
{
    const uint8_t codes[] = {PIXELS(1), 9};
    uint8_t bytes[12 + sizeof codes];
    make_frame(bytes, 1, 1, codes, sizeof codes);
    Reader r = reader_init(bytes, 11);
    Frame256 f = untouched_frame();

    ASSERT_FALSE(sprite256_frame_read(&r, &f));
    ASSERT_TRUE(frame_is_untouched(&f));
}

UTEST(sprite256, frame_read_with_cut_off_codes_fails)
{
    /* The header promises 2 code bytes and only 1 is there. */
    const uint8_t codes[] = {PIXELS(1), 9};
    uint8_t bytes[12 + sizeof codes];
    size_t size = make_frame(bytes, 1, 1, codes, sizeof codes);
    Reader r = reader_init(bytes, size - 1);
    Frame256 f = untouched_frame();

    ASSERT_FALSE(sprite256_frame_read(&r, &f));
    ASSERT_TRUE(frame_is_untouched(&f));
}

UTEST(sprite256, frame_read_with_too_few_pixels_fails)
{
    /* 3 pixels for a 2 by 2 frame. */
    const uint8_t codes[] = {PIXELS(2), 1, 2, SKIP_PIXELS(1)};
    uint8_t bytes[12 + sizeof codes];
    size_t size = make_frame(bytes, 2, 2, codes, sizeof codes);
    Reader r = reader_init(bytes, size);
    Frame256 f = untouched_frame();

    ASSERT_FALSE(sprite256_frame_read(&r, &f));
    ASSERT_TRUE(frame_is_untouched(&f));
}

UTEST(sprite256, frame_read_with_one_pixel_too_many_fails)
{
    /* 5 pixels for a 2 by 2 frame. The fifth must not be written. */
    const uint8_t codes[] = {PIXELS(5), 1, 2, 3, 4, 5};
    uint8_t bytes[12 + sizeof codes];
    size_t size = make_frame(bytes, 2, 2, codes, sizeof codes);
    Reader r = reader_init(bytes, size);
    Frame256 f = untouched_frame();

    ASSERT_FALSE(sprite256_frame_read(&r, &f));
    ASSERT_TRUE(frame_is_untouched(&f));
}

UTEST(sprite256, frame_read_with_a_skip_past_the_end_fails)
{
    const uint8_t pixel_skip[] = {SKIP_PIXELS(5)};
    const uint8_t row_skip[] = {SKIP_ROWS(3)};
    uint8_t bytes[12 + 1];
    Frame256 f = untouched_frame();

    size_t size = make_frame(bytes, 2, 2, pixel_skip, sizeof pixel_skip);
    Reader r = reader_init(bytes, size);
    ASSERT_FALSE(sprite256_frame_read(&r, &f));

    size = make_frame(bytes, 2, 2, row_skip, sizeof row_skip);
    r = reader_init(bytes, size);
    ASSERT_FALSE(sprite256_frame_read(&r, &f));

    ASSERT_TRUE(frame_is_untouched(&f));
}

UTEST(sprite256, frame_read_with_a_run_longer_than_its_codes_fails)
{
    /* The code announces 3 pixels and only 2 bytes follow. */
    const uint8_t codes[] = {PIXELS(3), 1, 2};
    uint8_t bytes[12 + sizeof codes];
    size_t size = make_frame(bytes, 3, 1, codes, sizeof codes);
    Reader r = reader_init(bytes, size);
    Frame256 f = untouched_frame();

    ASSERT_FALSE(sprite256_frame_read(&r, &f));
    ASSERT_TRUE(frame_is_untouched(&f));
}

UTEST(sprite256, frame_read_with_an_unknown_kind_of_code_fails)
{
    const uint8_t codes[] = {UNKNOWN_KIND(1), PIXELS(1), 9};
    uint8_t bytes[12 + sizeof codes];
    size_t size = make_frame(bytes, 2, 1, codes, sizeof codes);
    Reader r = reader_init(bytes, size);
    Frame256 f = untouched_frame();

    ASSERT_FALSE(sprite256_frame_read(&r, &f));
    ASSERT_TRUE(frame_is_untouched(&f));
}

UTEST(sprite256, frame_read_of_an_impossibly_large_frame_fails)
{
    /* Four billion pixels on each side and no codes. It must be refused
     * before any memory is asked for. */
    uint8_t bytes[12];
    Frame256 f = untouched_frame();

    size_t size = make_frame(bytes, 0xffffffffu, 1, NULL, 0);
    Reader r = reader_init(bytes, size);
    ASSERT_FALSE(sprite256_frame_read(&r, &f));

    size = make_frame(bytes, 1, 0xffffffffu, NULL, 0);
    r = reader_init(bytes, size);
    ASSERT_FALSE(sprite256_frame_read(&r, &f));

    ASSERT_TRUE(frame_is_untouched(&f));
}

UTEST(sprite256, frame_read_uses_all_six_bits_of_the_count)
{
    /* 63 is the largest count a code can hold. A 63 by 3 frame: a row of
     * 63 pixels, a row skipped pixel by pixel, and a row skipped as a
     * row. */
    uint8_t codes[1 + 63 + 1 + 1];
    codes[0] = PIXELS(63);
    for (size_t i = 0; i < 63; i++) {
        codes[1 + i] = (uint8_t)(i + 1);
    }
    codes[64] = SKIP_PIXELS(63);
    codes[65] = SKIP_ROWS(1);
    uint8_t bytes[12 + sizeof codes];
    size_t size = make_frame(bytes, 63, 3, codes, sizeof codes);
    Reader r = reader_init(bytes, size);
    Frame256 f = untouched_frame();

    ASSERT_TRUE(sprite256_frame_read(&r, &f));

    ASSERT_EQ(1, f.pixels[0]);
    ASSERT_EQ(63, f.pixels[62]);
    ASSERT_EQ(0, f.pixels[63]);
    ASSERT_EQ(0, f.pixels[63 * 3 - 1]);
    frame256_free(&f);
}
