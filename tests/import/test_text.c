#include "import/text.h"

#include <string.h>

#include "utest.h"

/* Converts `size` bytes and says whether the result is exactly the
 * `expected_size` bytes of `expected`. */
static bool gives(const uint8_t *data, size_t size, const char *expected,
    size_t expected_size)
{
    ByteBuffer out = {0};
    bool ok = text_cp1251_to_utf8(data, size, &out)
        && out.size == expected_size
        && (expected_size == 0
            || memcmp(out.data, expected, expected_size) == 0);
    buffer_free(&out);
    return ok;
}

UTEST(text, ascii_stays_as_it_is)
{
    const uint8_t in[] = "Orc Archer\t~ 0\x7f";
    ASSERT_TRUE(gives(in, sizeof in - 1, "Orc Archer\t~ 0\x7f", 15));
}

UTEST(text, empty_text_gives_nothing)
{
    ASSERT_TRUE(gives(NULL, 0, "", 0));
}

UTEST(text, a_line_ends_with_a_line_feed_alone)
{
    const uint8_t in[] = "a\r\n\r\nb\r\n";
    ASSERT_TRUE(gives(in, sizeof in - 1, "a\n\nb\n", 5));
}

UTEST(text, a_carriage_return_not_before_a_line_feed_is_kept)
{
    /* In the middle, twice in a row before a line feed, and as the
     * last byte, where there is no byte after it to look at. */
    const uint8_t in[] = "a\rb\r\r\nc\r";
    ASSERT_TRUE(gives(in, sizeof in - 1, "a\rb\r\nc\r", 7));
}

UTEST(text, a_carriage_return_as_the_last_byte_is_kept)
{
    /* No byte follows it, not even a closing zero, so looking for a
     * line feed after it would read past the end. */
    const uint8_t in[2] = {'a', '\r'};
    ASSERT_TRUE(gives(in, sizeof in, "a\r", 2));
}

UTEST(text, the_cyrillic_letters_become_two_bytes_each)
{
    /* 0xc0 is the first, U+0410, and 0xff the last, U+044F. 0xce 0xca
     * is "OK" in Cyrillic letters, U+041E U+041A. */
    const uint8_t in[] = {0xc0, 0xff, 0xce, 0xca};
    ASSERT_TRUE(gives(in, sizeof in,
        "\xd0\x90\xd1\x8f\xd0\x9e\xd0\x9a", 8));
}

UTEST(text, the_bytes_from_0x80_to_0xbf_follow_the_table)
{
    /* 0x80 is U+0402, 0xa8 U+0401, 0xb8 U+0451 and 0xbf U+0457: two
     * bytes each. 0xa0 is U+00A0. */
    const uint8_t in[] = {0x80, 0xa8, 0xb8, 0xbf, 0xa0};
    ASSERT_TRUE(gives(in, sizeof in,
        "\xd0\x82\xd0\x81\xd1\x91\xd1\x97\xc2\xa0", 10));
}

UTEST(text, a_character_from_u0800_on_becomes_three_bytes)
{
    /* 0x85 is U+2026, 0x97 U+2014, 0xb9 U+2116 and 0x88 U+20AC. */
    const uint8_t in[] = {0x85, 0x97, 0xb9, 0x88};
    ASSERT_TRUE(gives(in, sizeof in,
        "\xe2\x80\xa6\xe2\x80\x94\xe2\x84\x96\xe2\x82\xac", 12));
}

UTEST(text, the_byte_without_a_meaning_becomes_the_unknown_mark)
{
    const uint8_t in[] = {'a', 0x98, 'b'};
    ASSERT_TRUE(gives(in, sizeof in, "a\xef\xbf\xbd" "b", 5));
}

UTEST(text, the_text_is_added_after_what_the_buffer_holds)
{
    ByteBuffer out = {0};
    const uint8_t in[] = {'b', 0xc0};
    ASSERT_TRUE(buffer_push(&out, 'a'));

    ASSERT_TRUE(text_cp1251_to_utf8(in, sizeof in, &out));

    ASSERT_EQ(4u, (unsigned)out.size);
    ASSERT_EQ(0, memcmp(out.data, "ab\xd0\x90", 4));
    buffer_free(&out);
}
