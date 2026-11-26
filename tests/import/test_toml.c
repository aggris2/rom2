#include "import/toml.h"

#include <stdint.h>
#include <string.h>

#include "utest.h"

/* True if the buffer holds exactly the text `expected`. */
static bool holds(const ByteBuffer *b, const char *expected)
{
    size_t length = strlen(expected);
    return b->size == length
        && (length == 0 || memcmp(b->data, expected, length) == 0);
}

UTEST(toml, comment_writes_a_line_starting_with_a_hash)
{
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_comment(&b, "Human Swordsman"));
    ASSERT_TRUE(holds(&b, "# Human Swordsman\n"));
    buffer_free(&b);
}

UTEST(toml, table_writes_an_empty_line_and_the_name_in_brackets)
{
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_table(&b, "anim.attack"));
    ASSERT_TRUE(holds(&b, "\n[anim.attack]\n"));
    buffer_free(&b);
}

UTEST(toml, int_writes_key_and_number)
{
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_int(&b, "id", 3));
    ASSERT_TRUE(holds(&b, "id = 3\n"));
    buffer_free(&b);
}

UTEST(toml, int_writes_zero_and_negative_numbers)
{
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_int(&b, "a", 0));
    ASSERT_TRUE(toml_int(&b, "b", -12));
    ASSERT_TRUE(holds(&b, "a = 0\nb = -12\n"));
    buffer_free(&b);
}

UTEST(toml, int_writes_the_largest_and_the_smallest_number)
{
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_int(&b, "max", INT32_MAX));
    ASSERT_TRUE(toml_int(&b, "min", INT32_MIN));
    ASSERT_TRUE(holds(&b, "max = 2147483647\nmin = -2147483648\n"));
    buffer_free(&b);
}

UTEST(toml, text_writes_the_text_in_quotes)
{
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_text(&b, "name", "Human Swordsman"));
    ASSERT_TRUE(holds(&b, "name = \"Human Swordsman\"\n"));
    buffer_free(&b);
}

UTEST(toml, text_of_no_characters_writes_empty_quotes)
{
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_text(&b, "name", ""));
    ASSERT_TRUE(holds(&b, "name = \"\"\n"));
    buffer_free(&b);
}

UTEST(toml, text_puts_a_backslash_before_quotes_and_backslashes)
{
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_text(&b, "file", "a\\b \"c\""));
    ASSERT_TRUE(holds(&b, "file = \"a\\\\b \\\"c\\\"\"\n"));
    buffer_free(&b);
}

UTEST(toml, text_writes_control_characters_as_codes)
{
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_text(&b, "t", "a\nb\tc\x7f"));
    ASSERT_TRUE(holds(&b, "t = \"a\\u000Ab\\u0009c\\u007F\"\n"));
    buffer_free(&b);
}

UTEST(toml, text_leaves_utf8_bytes_alone)
{
    /* U+00E9 as two bytes. */
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_text(&b, "t", "caf\xc3\xa9"));
    ASSERT_TRUE(holds(&b, "t = \"caf\xc3\xa9\"\n"));
    buffer_free(&b);
}

UTEST(toml, int_list_writes_the_numbers_with_commas)
{
    const int32_t values[] = {2, 2, 1};
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_int_list(&b, "ticks", values, 3));
    ASSERT_TRUE(holds(&b, "ticks = [2, 2, 1]\n"));
    buffer_free(&b);
}

UTEST(toml, int_list_of_one_number_has_no_comma)
{
    const int32_t values[] = {-7};
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_int_list(&b, "k", values, 1));
    ASSERT_TRUE(holds(&b, "k = [-7]\n"));
    buffer_free(&b);
}

UTEST(toml, int_list_of_no_numbers_writes_empty_brackets)
{
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_int_list(&b, "k", NULL, 0));
    ASSERT_TRUE(holds(&b, "k = []\n"));
    buffer_free(&b);
}

UTEST(toml, lines_follow_each_other_in_the_buffer)
{
    const int32_t cell[] = {88, 72};
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_comment(&b, "a unit"));
    ASSERT_TRUE(toml_int(&b, "id", 3));
    ASSERT_TRUE(toml_table(&b, "sprite"));
    ASSERT_TRUE(toml_text(&b, "sheet", "sprites.png"));
    ASSERT_TRUE(toml_int_list(&b, "cell", cell, 2));
    ASSERT_TRUE(holds(&b, "# a unit\nid = 3\n\n[sprite]\n"
        "sheet = \"sprites.png\"\ncell = [88, 72]\n"));
    buffer_free(&b);
}

UTEST(toml, table_list_writes_the_name_in_double_brackets)
{
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_table_list(&b, "kind"));
    ASSERT_TRUE(toml_int(&b, "id", 600));
    ASSERT_TRUE(toml_table_list(&b, "kind"));
    ASSERT_TRUE(holds(&b, "\n[[kind]]\nid = 600\n\n[[kind]]\n"));
    buffer_free(&b);
}

UTEST(toml, text_list_writes_each_text_in_quotes)
{
    const char *const items[] = {"Bronze Pike", "Wooden Shield"};
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_text_list(&b, "items", items, 2));
    ASSERT_TRUE(holds(&b, "items = [\"Bronze Pike\", \"Wooden Shield\"]\n"));
    buffer_free(&b);
}

UTEST(toml, text_list_of_one_or_no_texts)
{
    const char *const items[] = {"a \"b\""};
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_text_list(&b, "one", items, 1));
    ASSERT_TRUE(toml_text_list(&b, "none", NULL, 0));
    ASSERT_TRUE(holds(&b, "one = [\"a \\\"b\\\"\"]\nnone = []\n"));
    buffer_free(&b);
}
