#include "base/toml.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "expect.h"
#include "utest.h"

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

UTEST(toml, float_writes_the_shortest_text_that_reads_back_the_same)
{
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_float(&b, "a", 0.25));
    ASSERT_TRUE(toml_float(&b, "b", 1.3));
    ASSERT_TRUE(toml_float(&b, "c", 0.200001));
    ASSERT_TRUE(toml_float(&b, "d", -2.5));
    ASSERT_TRUE(holds(&b, "a = 0.25\nb = 1.3\nc = 0.200001\nd = -2.5\n"));
    buffer_free(&b);
}

UTEST(toml, float_of_a_whole_number_still_has_a_point)
{
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_float(&b, "a", 1.0));
    ASSERT_TRUE(toml_float(&b, "b", 0.0));
    ASSERT_TRUE(toml_float(&b, "c", 900.0));
    ASSERT_TRUE(holds(&b, "a = 1.0\nb = 0.0\nc = 900.0\n"));
    buffer_free(&b);
}

UTEST(toml, float_keeps_every_digit_a_number_needs)
{
    /* One third has no short form: it takes 16 digits to bring it back
     * exactly. */
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_float(&b, "a", 1.0 / 3.0));
    ASSERT_TRUE(holds(&b, "a = 0.3333333333333333\n"));
    buffer_free(&b);
}

UTEST(toml, float_writes_very_large_and_very_small_numbers)
{
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_float(&b, "a", 1e300));
    ASSERT_TRUE(toml_float(&b, "b", 2.5e-10));
    ASSERT_TRUE(holds(&b, "a = 1e+300\nb = 2.5e-10\n"));
    buffer_free(&b);
}

UTEST(toml, float_refuses_a_number_that_is_not_finite)
{
    ByteBuffer b = {0};
    double zero = 0.0;
    ASSERT_FALSE(toml_float(&b, "a", 1.0 / zero));
    ASSERT_FALSE(toml_float(&b, "a", -1.0 / zero));
    ASSERT_FALSE(toml_float(&b, "a", zero / zero));
    ASSERT_EQ(0u, b.size);
    buffer_free(&b);
}

UTEST(toml, bool_writes_true_or_false)
{
    ByteBuffer b = {0};
    ASSERT_TRUE(toml_bool(&b, "second", true));
    ASSERT_TRUE(toml_bool(&b, "flat", false));
    ASSERT_TRUE(holds(&b, "second = true\nflat = false\n"));
    buffer_free(&b);
}

/* ---- Reading ---- */

/* Reads a text as a TOML file. The reader gets a copy that ends where
 * the text ends, with no zero after it, so that the sanitizers see a
 * read past the end. */
static bool read_copy(const char *text, TomlTable **out, size_t *line)
{
    size_t size = strlen(text);
    uint8_t *copy = malloc(size > 0 ? size : 1);
    if (copy == NULL) {
        return false;
    }
    memcpy(copy, text, size);
    bool ok = toml_read(copy, size, out, line);
    free(copy);
    return ok;
}

static bool reads(const char *text, TomlTable **out)
{
    return read_copy(text, out, NULL);
}

/* True if the text is refused at the line `line`, and what would be
 * given is left alone. */
static bool refuses_at(const char *text, size_t line)
{
    TomlTable *marker = (TomlTable *)&line;
    TomlTable *table = marker;
    size_t error_line = 99;
    return !read_copy(text, &table, &error_line)
        && table == marker && error_line == line;
}

static bool refuses(const char *text)
{
    return refuses_at(text, 1);
}

UTEST(toml, reads_each_kind_of_value)
{
    TomlTable *t = NULL;
    ASSERT_TRUE(reads(
        "id = 3\n"
        "low = -40\n"
        "part = 0.25\n"
        "tiny = -1.5e-3\n"
        "big = 2E2\n"
        "yes = true\n"
        "no = false\n"
        "name = \"Human Swordsman\"\n", &t));
    int32_t whole = 0;
    double number = 0.0;
    bool flag = false;
    const char *text = NULL;
    EXPECT_TRUE(toml_get_int(t, "id", &whole));
    EXPECT_EQ(3, whole);
    EXPECT_TRUE(toml_get_int(t, "low", &whole));
    EXPECT_EQ(-40, whole);
    EXPECT_TRUE(toml_get_float(t, "part", &number));
    EXPECT_EQ(0.25, number);
    EXPECT_TRUE(toml_get_float(t, "tiny", &number));
    EXPECT_EQ(-0.0015, number);
    EXPECT_TRUE(toml_get_float(t, "big", &number));
    EXPECT_EQ(200.0, number);
    EXPECT_TRUE(toml_get_bool(t, "yes", &flag));
    EXPECT_TRUE(flag);
    EXPECT_TRUE(toml_get_bool(t, "no", &flag));
    EXPECT_FALSE(flag);
    EXPECT_TRUE(toml_get_text(t, "name", &text));
    EXPECT_STREQ("Human Swordsman", text);
    toml_free(t);
}

UTEST(toml, reads_lists_of_whole_numbers_and_of_texts)
{
    TomlTable *t = NULL;
    ASSERT_TRUE(reads(
        "ticks = [2, -2,1 ]\n"
        "names = [\"a\", \"b c\"]\n"
        "none = []\n"
        "comma = [ 7, ]\n", &t));
    const int32_t *values = NULL;
    const char *const *texts = NULL;
    size_t count = 0;
    ASSERT_TRUE(toml_get_int_list(t, "ticks", &values, &count));
    ASSERT_EQ(3u, count);
    EXPECT_EQ(2, values[0]);
    EXPECT_EQ(-2, values[1]);
    EXPECT_EQ(1, values[2]);
    ASSERT_TRUE(toml_get_text_list(t, "names", &texts, &count));
    ASSERT_EQ(2u, count);
    EXPECT_STREQ("a", texts[0]);
    EXPECT_STREQ("b c", texts[1]);
    /* An empty list is one of either kind. */
    count = 5;
    EXPECT_TRUE(toml_get_int_list(t, "none", &values, &count));
    EXPECT_EQ(0u, count);
    count = 5;
    EXPECT_TRUE(toml_get_text_list(t, "none", &texts, &count));
    EXPECT_EQ(0u, count);
    ASSERT_TRUE(toml_get_int_list(t, "comma", &values, &count));
    ASSERT_EQ(1u, count);
    EXPECT_EQ(7, values[0]);
    toml_free(t);
}

UTEST(toml, reads_a_long_list_and_many_keys)
{
    ByteBuffer b = {0};
    char line[32];
    for (int i = 0; i < 40; i++) {
        snprintf(line, sizeof line, "k%d", i);
        toml_int(&b, line, i);
    }
    int32_t values[40];
    for (int i = 0; i < 40; i++) {
        values[i] = i * 3;
    }
    toml_int_list(&b, "list", values, 40);
    TomlTable *t = NULL;
    ASSERT_TRUE(toml_read(b.data, b.size, &t, NULL));
    const int32_t *read = NULL;
    size_t count = 0;
    int32_t whole = 0;
    EXPECT_TRUE(toml_get_int(t, "k39", &whole));
    EXPECT_EQ(39, whole);
    ASSERT_TRUE(toml_get_int_list(t, "list", &read, &count));
    ASSERT_EQ(40u, count);
    EXPECT_EQ(117, read[39]);
    toml_free(t);
    buffer_free(&b);
}

UTEST(toml, reads_the_escapes_of_a_text)
{
    TomlTable *t = NULL;
    ASSERT_TRUE(reads(
        "a = \"q\\\" b\\\\ n\\n t\\t r\\r\"\n"
        "b = \"\\u0041\\u00e9\\u20AC\"\n"
        "c = \"tab\there, and \xd0\x96 as it is\"\n"
        "d = \"\"\n", &t));
    const char *text = NULL;
    ASSERT_TRUE(toml_get_text(t, "a", &text));
    EXPECT_STREQ("q\" b\\ n\n t\t r\r", text);
    ASSERT_TRUE(toml_get_text(t, "b", &text));
    EXPECT_STREQ("A\xc3\xa9\xe2\x82\xac", text);
    ASSERT_TRUE(toml_get_text(t, "c", &text));
    EXPECT_STREQ("tab\there, and \xd0\x96 as it is", text);
    ASSERT_TRUE(toml_get_text(t, "d", &text));
    EXPECT_STREQ("", text);
    toml_free(t);
}

UTEST(toml, reads_back_any_text_it_writes)
{
    const char *hard = "a \"b\" \\ c\n\t\x01\x1f\x7f \xd0\x96";
    ByteBuffer b = {0};
    toml_text(&b, "t", hard);
    TomlTable *t = NULL;
    ASSERT_TRUE(toml_read(b.data, b.size, &t, NULL));
    const char *text = NULL;
    ASSERT_TRUE(toml_get_text(t, "t", &text));
    EXPECT_STREQ(hard, text);
    toml_free(t);
    buffer_free(&b);
}

UTEST(toml, reads_back_the_numbers_with_a_fraction_it_writes)
{
    const double numbers[] = {0.1, -2.5, 1e300, 1.0 / 3.0, 5.0, 1e-300};
    for (size_t i = 0; i < sizeof numbers / sizeof numbers[0]; i++) {
        ByteBuffer b = {0};
        toml_float(&b, "n", numbers[i]);
        TomlTable *t = NULL;
        ASSERT_TRUE(toml_read(b.data, b.size, &t, NULL));
        double number = 0.0;
        int32_t whole = 0;
        EXPECT_TRUE(toml_get_float(t, "n", &number));
        EXPECT_EQ(numbers[i], number);
        /* Even 5.0 is not a whole number once written with a point. */
        EXPECT_FALSE(toml_get_int(t, "n", &whole));
        toml_free(t);
        buffer_free(&b);
    }
}

UTEST(toml, reads_the_largest_and_smallest_whole_numbers)
{
    TomlTable *t = NULL;
    ASSERT_TRUE(reads("a = 2147483647\nb = -2147483648\nc = +5\n", &t));
    int32_t whole = 0;
    EXPECT_TRUE(toml_get_int(t, "a", &whole));
    EXPECT_EQ(INT32_MAX, whole);
    EXPECT_TRUE(toml_get_int(t, "b", &whole));
    EXPECT_EQ(INT32_MIN, whole);
    EXPECT_TRUE(toml_get_int(t, "c", &whole));
    EXPECT_EQ(5, whole);
    toml_free(t);
    EXPECT_TRUE(refuses("a = 2147483648\n"));
    EXPECT_TRUE(refuses("a = -2147483649\n"));
    EXPECT_TRUE(refuses("a = 99999999999999999999999\n"));
}

UTEST(toml, a_whole_number_is_also_given_as_one_with_a_fraction)
{
    TomlTable *t = NULL;
    ASSERT_TRUE(reads("a = 7\n", &t));
    double number = 0.0;
    EXPECT_TRUE(toml_get_float(t, "a", &number));
    EXPECT_EQ(7.0, number);
    toml_free(t);
}

UTEST(toml, reads_tables_and_tables_inside_them)
{
    TomlTable *t = NULL;
    ASSERT_TRUE(reads(
        "id = 1\n"
        "\n"
        "[sprite]\n"
        "sheet = \"sprites.png\"\n"
        "\n"
        "[anim.move]\n"
        "column = 0\n"
        "\n"
        "[anim.attack]\n"
        "column = 10\n"
        "\n"
        "[\"Path Finding\"]\n"
        "id = 2\n", &t));
    int32_t whole = 0;
    const char *text = NULL;
    const TomlTable *sprite = toml_get_table(t, "sprite");
    const TomlTable *anim = toml_get_table(t, "anim");
    ASSERT_TRUE(sprite != NULL);
    ASSERT_TRUE(anim != NULL);
    EXPECT_TRUE(toml_get_text(sprite, "sheet", &text));
    EXPECT_STREQ("sprites.png", text);
    /* A key belongs to its own table alone. */
    EXPECT_FALSE(toml_get_text(t, "sheet", &text));
    EXPECT_FALSE(toml_get_int(sprite, "id", &whole));
    const TomlTable *move = toml_get_table(anim, "move");
    const TomlTable *attack = toml_get_table(anim, "attack");
    ASSERT_TRUE(move != NULL);
    ASSERT_TRUE(attack != NULL);
    EXPECT_TRUE(toml_get_int(move, "column", &whole));
    EXPECT_EQ(0, whole);
    EXPECT_TRUE(toml_get_int(attack, "column", &whole));
    EXPECT_EQ(10, whole);
    const TomlTable *quoted = toml_get_table(t, "Path Finding");
    ASSERT_TRUE(quoted != NULL);
    EXPECT_TRUE(toml_get_int(quoted, "id", &whole));
    EXPECT_EQ(2, whole);
    EXPECT_TRUE(toml_get_int(t, "id", &whole));
    EXPECT_EQ(1, whole);
    EXPECT_TRUE(toml_get_table(t, "none") == NULL);
    toml_free(t);
}

UTEST(toml, a_table_may_be_named_after_one_inside_it_was)
{
    TomlTable *t = NULL;
    ASSERT_TRUE(reads("[a.b]\nx = 1\n[a]\ny = 2\n", &t));
    int32_t whole = 0;
    const TomlTable *a = toml_get_table(t, "a");
    ASSERT_TRUE(a != NULL);
    EXPECT_TRUE(toml_get_int(a, "y", &whole));
    EXPECT_EQ(2, whole);
    EXPECT_TRUE(toml_get_table(a, "b") != NULL);
    toml_free(t);
}

UTEST(toml, reads_a_list_of_tables)
{
    TomlTable *t = NULL;
    ASSERT_TRUE(reads(
        "[[kind]]\n"
        "id = 1\n"
        "\n"
        "[[kind]]\n"
        "id = 2\n"
        "\n"
        "[[ kind ]]\n"
        "id = 3\n", &t));
    int32_t whole = 0;
    ASSERT_EQ(3u, toml_get_table_count(t, "kind"));
    for (size_t i = 0; i < 3; i++) {
        const TomlTable *kind = toml_get_table_at(t, "kind", i);
        ASSERT_TRUE(kind != NULL);
        EXPECT_TRUE(toml_get_int(kind, "id", &whole));
        EXPECT_EQ((int32_t)i + 1, whole);
    }
    EXPECT_TRUE(toml_get_table_at(t, "kind", 3) == NULL);
    EXPECT_EQ(0u, toml_get_table_count(t, "none"));
    EXPECT_TRUE(toml_get_table_at(t, "none", 0) == NULL);
    toml_free(t);
}

UTEST(toml, passes_comments_spaces_and_either_end_of_line)
{
    TomlTable *t = NULL;
    ASSERT_TRUE(reads(
        "# a comment\r\n"
        "\r\n"
        "  \t  \n"
        "\t a\t=  1  # after a value\r\n"
        "[ t ]   # after a header\n"
        "b=2", &t));
    int32_t whole = 0;
    EXPECT_TRUE(toml_get_int(t, "a", &whole));
    EXPECT_EQ(1, whole);
    ASSERT_TRUE(toml_get_table(t, "t") != NULL);
    EXPECT_TRUE(toml_get_int(toml_get_table(t, "t"), "b", &whole));
    EXPECT_EQ(2, whole);
    toml_free(t);
}

UTEST(toml, an_empty_file_is_an_empty_table)
{
    TomlTable *t = NULL;
    ASSERT_TRUE(toml_read(NULL, 0, &t, NULL));
    int32_t whole = 0;
    EXPECT_FALSE(toml_get_int(t, "a", &whole));
    toml_free(t);
}

UTEST(toml, a_key_holds_one_kind_of_value_and_no_other)
{
    TomlTable *t = NULL;
    ASSERT_TRUE(reads(
        "i = 1\nf = 1.5\nb = true\ns = \"x\"\nil = [1]\nsl = [\"x\"]\n"
        "[t]\n[[l]]\n", &t));
    int32_t whole = 77;
    double number = 77.0;
    bool flag = true;
    const char *text = "kept";
    const int32_t *values = NULL;
    const char *const *texts = NULL;
    size_t count = 77;
    EXPECT_FALSE(toml_get_int(t, "f", &whole));
    EXPECT_FALSE(toml_get_int(t, "s", &whole));
    EXPECT_FALSE(toml_get_int(t, "none", &whole));
    EXPECT_EQ(77, whole);
    EXPECT_FALSE(toml_get_float(t, "s", &number));
    EXPECT_FALSE(toml_get_float(t, "b", &number));
    EXPECT_EQ(77.0, number);
    EXPECT_FALSE(toml_get_bool(t, "i", &flag));
    EXPECT_TRUE(flag);
    EXPECT_FALSE(toml_get_text(t, "i", &text));
    EXPECT_FALSE(toml_get_text(t, "sl", &text));
    EXPECT_STREQ("kept", text);
    EXPECT_FALSE(toml_get_int_list(t, "sl", &values, &count));
    EXPECT_FALSE(toml_get_int_list(t, "i", &values, &count));
    EXPECT_FALSE(toml_get_text_list(t, "il", &texts, &count));
    EXPECT_FALSE(toml_get_text_list(t, "s", &texts, &count));
    EXPECT_EQ(77u, count);
    EXPECT_TRUE(toml_get_table(t, "l") == NULL);
    EXPECT_TRUE(toml_get_table(t, "i") == NULL);
    EXPECT_EQ(0u, toml_get_table_count(t, "t"));
    EXPECT_TRUE(toml_get_table_at(t, "t", 0) == NULL);
    toml_free(t);
}

UTEST(toml, refuses_a_key_or_a_table_given_twice)
{
    EXPECT_TRUE(refuses_at("a = 1\na = 2\n", 2));
    EXPECT_TRUE(refuses_at("[t]\na = 1\n[t]\n", 3));
    EXPECT_TRUE(refuses_at("[a.b]\n[a]\n[a]\n", 3));
    /* A key and a table of one name, either way round. */
    EXPECT_TRUE(refuses_at("t = 1\n[t]\n", 2));
    EXPECT_TRUE(refuses_at("t = 1\n[t.u]\n", 2));
    EXPECT_TRUE(refuses_at("t = 1\n[[t]]\n", 2));
    EXPECT_TRUE(refuses_at("[t]\n[[t]]\n", 2));
    EXPECT_TRUE(refuses_at("[[t]]\n[t]\n", 2));
    EXPECT_TRUE(refuses_at("[a.t]\n[a]\nt = 1\n", 3));
}

UTEST(toml, refuses_a_line_that_is_not_a_key_and_a_value)
{
    EXPECT_TRUE(refuses("a\n"));
    EXPECT_TRUE(refuses("a =\n"));
    EXPECT_TRUE(refuses("a = \n"));
    EXPECT_TRUE(refuses("= 1\n"));
    EXPECT_TRUE(refuses("a b = 1\n"));
    EXPECT_TRUE(refuses("a.b = 1\n"));
    EXPECT_TRUE(refuses("a = 1 2\n"));
    EXPECT_TRUE(refuses("a = 1 b = 2\n"));
    EXPECT_TRUE(refuses("a = truer\n"));
    EXPECT_TRUE(refuses("a = True\n"));
    EXPECT_TRUE(refuses("a = nothing\n"));
    EXPECT_TRUE(refuses("a = 'single'\n"));
    EXPECT_TRUE(refuses("a = { b = 1 }\n"));
    EXPECT_TRUE(refuses("a = 1979-05-27\n"));
    EXPECT_TRUE(refuses_at("ok = 1\n\n# fine\nbad\n", 4));
}

UTEST(toml, refuses_a_number_that_is_not_one)
{
    EXPECT_TRUE(refuses("a = -\n"));
    EXPECT_TRUE(refuses("a = 1.\n"));
    EXPECT_TRUE(refuses("a = .5\n"));
    EXPECT_TRUE(refuses("a = 1e\n"));
    EXPECT_TRUE(refuses("a = 1e+\n"));
    EXPECT_TRUE(refuses("a = 1.5.5\n"));
    EXPECT_TRUE(refuses("a = 0x10\n"));
    EXPECT_TRUE(refuses("a = 1_000\n"));
    EXPECT_TRUE(refuses("a = 1e999\n"));
    EXPECT_TRUE(refuses("a = inf\n"));
    EXPECT_TRUE(refuses("a = nan\n"));
    EXPECT_TRUE(refuses(
        "a = 0.00000000000000000000000000000000000000000000000000000000"
        "0000000001\n"));
}

UTEST(toml, refuses_a_text_that_is_not_closed_or_has_what_it_may_not)
{
    EXPECT_TRUE(refuses("a = \"open\n"));
    EXPECT_TRUE(refuses("a = \"open"));
    EXPECT_TRUE(refuses("a = \"bad \\q escape\"\n"));
    EXPECT_TRUE(refuses("a = \"ends in \\"));
    EXPECT_TRUE(refuses("a = \"\\u12\"\n"));
    EXPECT_TRUE(refuses("a = \"\\u12g4\"\n"));
    EXPECT_TRUE(refuses("a = \"\\U0001F600\"\n"));
    EXPECT_TRUE(refuses("a = \"\\b\"\n"));
    /* No character 0, and no half of a pair. */
    EXPECT_TRUE(refuses("a = \"\\u0000\"\n"));
    EXPECT_TRUE(refuses("a = \"\\ud800\"\n"));
    EXPECT_TRUE(refuses("a = \"\\udfff\"\n"));
    /* A control character as it is. */
    EXPECT_TRUE(refuses("a = \"bell \x07\"\n"));
    EXPECT_TRUE(refuses("a = \"del \x7f\"\n"));
    EXPECT_TRUE(refuses("a = 1 # bell \x07\n"));
    EXPECT_TRUE(refuses("a = 1 # del \x7f\n"));
}

UTEST(toml, reads_the_characters_at_the_edges_of_what_a_text_may_hold)
{
    TomlTable *t = NULL;
    ASSERT_TRUE(reads(
        "a = \"\\u0001\\u007f\\u0080\\u07ff\\u0800\\ud7ff\\ue000\\uffff"
        "\"\n", &t));
    const char *text = NULL;
    ASSERT_TRUE(toml_get_text(t, "a", &text));
    EXPECT_STREQ("\x01\x7f\xc2\x80\xdf\xbf\xe0\xa0\x80\xed\x9f\xbf"
        "\xee\x80\x80\xef\xbf\xbf", text);
    toml_free(t);
}

UTEST(toml, refuses_a_list_that_is_not_one)
{
    EXPECT_TRUE(refuses("a = [1, 2\n"));
    EXPECT_TRUE(refuses("a = [1 2]\n"));
    EXPECT_TRUE(refuses("a = [,]\n"));
    EXPECT_TRUE(refuses("a = [1,,2]\n"));
    EXPECT_TRUE(refuses("a = [1, \"x\"]\n"));
    EXPECT_TRUE(refuses("a = [\"x\", 1]\n"));
    EXPECT_TRUE(refuses("a = [1.5]\n"));
    EXPECT_TRUE(refuses("a = [true]\n"));
    EXPECT_TRUE(refuses("a = [[1]]\n"));
    EXPECT_TRUE(refuses("a = [1] 2\n"));
    EXPECT_TRUE(refuses("a = [\"x\", \"open]\n"));
    /* A list is on one line. */
    EXPECT_TRUE(refuses("a = [1,\n2]\n"));
}

UTEST(toml, refuses_a_header_that_is_not_one)
{
    EXPECT_TRUE(refuses("[]\n"));
    EXPECT_TRUE(refuses("[t\n"));
    EXPECT_TRUE(refuses("[t] x\n"));
    EXPECT_TRUE(refuses("[t.]\n"));
    EXPECT_TRUE(refuses("[.t]\n"));
    EXPECT_TRUE(refuses("[t u]\n"));
    EXPECT_TRUE(refuses("[[t]\n"));
    EXPECT_TRUE(refuses("[[t]]]\n"));
    EXPECT_TRUE(refuses("[t]]\n"));
    EXPECT_TRUE(refuses("[[]]\n"));
    /* A list of tables has one name. */
    EXPECT_TRUE(refuses("[[a.b]]\n"));
}

UTEST(toml, a_name_is_letters_digits_underscore_and_dash_and_no_more)
{
    TomlTable *t = NULL;
    ASSERT_TRUE(reads("azAZ09_- = 1\n[bB-_9]\n", &t));
    int32_t whole = 0;
    EXPECT_TRUE(toml_get_int(t, "azAZ09_-", &whole));
    EXPECT_TRUE(toml_get_table(t, "bB-_9") != NULL);
    toml_free(t);
    /* The characters right beside those in the table of characters. */
    EXPECT_TRUE(refuses("a{ = 1\n"));
    EXPECT_TRUE(refuses("a[ = 1\n"));
    EXPECT_TRUE(refuses("a@ = 1\n"));
    EXPECT_TRUE(refuses("a` = 1\n"));
    EXPECT_TRUE(refuses("a: = 1\n"));
    EXPECT_TRUE(refuses("a/ = 1\n"));
    EXPECT_TRUE(refuses("a = true{\n"));
}

UTEST(toml, the_digits_of_a_code_are_0_to_9_and_a_to_f_in_either_case)
{
    TomlTable *t = NULL;
    ASSERT_TRUE(reads("a = \"\\u09af\\u09AF\"\n", &t));
    const char *text = NULL;
    ASSERT_TRUE(toml_get_text(t, "a", &text));
    EXPECT_STREQ("\xe0\xa6\xaf\xe0\xa6\xaf", text);
    toml_free(t);
    EXPECT_TRUE(refuses("a = \"\\u00g0\"\n"));
    EXPECT_TRUE(refuses("a = \"\\u00G0\"\n"));
    EXPECT_TRUE(refuses("a = \"\\u00:0\"\n"));
    EXPECT_TRUE(refuses("a = \"\\u00/0\"\n"));
    EXPECT_TRUE(refuses("a = \"\\u00`0\"\n"));
    EXPECT_TRUE(refuses("a = \"\\u00@0\"\n"));
}

UTEST(toml, refuses_what_is_right_after_the_digits_of_a_number)
{
    EXPECT_TRUE(refuses("a = 1:\n"));
    EXPECT_TRUE(refuses("a = 1/\n"));
    EXPECT_TRUE(refuses("a = [1:]\n"));
}

UTEST(toml, a_number_may_be_63_characters_long_and_no_longer)
{
    /* "0." and 61 digits, then one more. */
    char line[80] = "a = 0.";
    memset(line + 6, '1', 61);
    line[67] = '\n';
    line[68] = '\0';
    TomlTable *t = NULL;
    ASSERT_TRUE(reads(line, &t));
    double number = 0.0;
    EXPECT_TRUE(toml_get_float(t, "a", &number));
    EXPECT_TRUE(number > 0.111 && number < 0.112);
    toml_free(t);
    line[67] = '1';
    line[68] = '\n';
    line[69] = '\0';
    EXPECT_TRUE(refuses(line));
}

UTEST(toml, reads_a_value_that_ends_with_the_file)
{
    TomlTable *t = NULL;
    bool flag = false;
    int32_t whole = 0;
    const char *text = NULL;
    const int32_t *values = NULL;
    size_t count = 0;
    ASSERT_TRUE(reads("a = true", &t));
    EXPECT_TRUE(toml_get_bool(t, "a", &flag));
    EXPECT_TRUE(flag);
    toml_free(t);
    ASSERT_TRUE(reads("a = false", &t));
    EXPECT_TRUE(toml_get_bool(t, "a", &flag));
    EXPECT_FALSE(flag);
    toml_free(t);
    ASSERT_TRUE(reads("a = 12", &t));
    EXPECT_TRUE(toml_get_int(t, "a", &whole));
    EXPECT_EQ(12, whole);
    toml_free(t);
    ASSERT_TRUE(reads("a = \"x\"", &t));
    EXPECT_TRUE(toml_get_text(t, "a", &text));
    EXPECT_STREQ("x", text);
    toml_free(t);
    ASSERT_TRUE(reads("a = [1, 2]", &t));
    EXPECT_TRUE(toml_get_int_list(t, "a", &values, &count));
    EXPECT_EQ(2u, count);
    toml_free(t);
    ASSERT_TRUE(reads("[t]", &t));
    EXPECT_TRUE(toml_get_table(t, "t") != NULL);
    toml_free(t);
    ASSERT_TRUE(reads("a = 1 # no end of line", &t));
    toml_free(t);
    ASSERT_TRUE(reads("a = 1\n  \t", &t));
    toml_free(t);
    /* And what is cut off in the middle. */
    EXPECT_TRUE(refuses("a = tru"));
    EXPECT_TRUE(refuses("a = [1,"));
    EXPECT_TRUE(refuses("a = [1"));
    EXPECT_TRUE(refuses("a = \"x\\"));
    EXPECT_TRUE(refuses("a = \"\\u00"));
    EXPECT_TRUE(refuses("a ="));
    EXPECT_TRUE(refuses("a"));
    EXPECT_TRUE(refuses("["));
    EXPECT_TRUE(refuses("[t"));
    EXPECT_TRUE(refuses("[[t]"));
    EXPECT_TRUE(refuses("a = 1."));
    EXPECT_TRUE(refuses("a = -"));
}

UTEST(toml, refuses_a_file_cut_short_inside_a_value_at_any_byte)
{
    const char *text = "name = \"a \\u00e9 b\"\nlist = [1, 2, 3]\n"
        "texts = [\"x\", \"y\"]\n[[kind]]\nyes = true\n[a.b]\nn = -1.5e3\n";
    size_t size = strlen(text);
    for (size_t cut = 0; cut <= size; cut++) {
        uint8_t *copy = malloc(cut > 0 ? cut : 1);
        ASSERT_TRUE(copy != NULL);
        memcpy(copy, text, cut);
        TomlTable *t = NULL;
        /* Read or refused, it must not look past the cut. */
        if (toml_read(copy, cut, &t, NULL)) {
            toml_free(t);
        }
        free(copy);
    }
}

