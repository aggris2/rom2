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
