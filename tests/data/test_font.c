#include "data/font.h"

#include <string.h>

#include "utest.h"

#define START "glyphs = \"g.png\"\ncoloured = false\ncell = [8, 10]\n" \
    "columns = 16\n"

/* Reads a definition written as text. */
static bool reads(const char *text, Font *out)
{
    TomlTable *top = NULL;
    if (!toml_read((const uint8_t *)text, strlen(text), &top, NULL)) {
        return false;
    }
    bool ok = font_read(top, out);
    toml_free(top);
    return ok;
}

/* True if the definition is refused and `out` is left as it was. */
static bool refuses(const char *text)
{
    static Font font;
    static Font before;
    memset(&font, 0x5a, sizeof font);
    before = font;
    return !reads(text, &font) && memcmp(&font, &before, sizeof font) == 0;
}

UTEST(font, reads_its_picture_its_cells_and_the_width_of_each_glyph)
{
    Font font;
    ASSERT_TRUE(reads(START "first_character = 32\ncount = 3\n"
        "widths = [0, 2, 4]\n", &font));
    EXPECT_STREQ("g.png", font.glyphs);
    EXPECT_FALSE(font.coloured);
    EXPECT_EQ(8u, font.cell_width);
    EXPECT_EQ(10u, font.cell_height);
    EXPECT_EQ(16u, font.columns);
    EXPECT_EQ(32u, font.first_character);
    EXPECT_EQ(3u, font.count);
    EXPECT_EQ(0u, font.widths[0]);
    EXPECT_EQ(2u, font.widths[1]);
    EXPECT_EQ(4u, font.widths[2]);
}

UTEST(font, refuses_a_definition_that_lacks_a_part)
{
    EXPECT_TRUE(refuses("coloured = false\ncell = [8, 10]\ncolumns = 16\n"
        "first_character = 32\ncount = 1\nwidths = [4]\n"));
    EXPECT_TRUE(refuses("glyphs = \"g.png\"\ncell = [8, 10]\ncolumns = 16\n"
        "first_character = 32\ncount = 1\nwidths = [4]\n"));
    EXPECT_TRUE(refuses(START "count = 1\nwidths = [4]\n"));
    EXPECT_TRUE(refuses(START "first_character = 32\nwidths = [4]\n"));
    EXPECT_TRUE(refuses(START "first_character = 32\ncount = 1\n"));
}

UTEST(font, refuses_widths_that_are_not_one_for_each_glyph)
{
    EXPECT_TRUE(refuses(START "first_character = 32\ncount = 3\n"
        "widths = [0, 2]\n"));
    EXPECT_TRUE(refuses(START "first_character = 32\ncount = 1\n"
        "widths = [0, 2]\n"));
    /* A glyph wider than a cell, and one of less than no width. */
    EXPECT_TRUE(refuses(START "first_character = 32\ncount = 1\n"
        "widths = [9]\n"));
    EXPECT_TRUE(refuses(START "first_character = 32\ncount = 1\n"
        "widths = [-1]\n"));
}

UTEST(font, refuses_characters_that_do_not_fit_in_a_byte)
{
    EXPECT_TRUE(refuses(START "first_character = 255\ncount = 2\n"
        "widths = [1, 1]\n"));
    EXPECT_TRUE(refuses(START "first_character = -1\ncount = 1\n"
        "widths = [1]\n"));
    EXPECT_TRUE(refuses(START "first_character = 32\ncount = 0\n"
        "widths = []\n"));
    Font font;
    EXPECT_TRUE(reads(START "first_character = 255\ncount = 1\n"
        "widths = [1]\n", &font));
}

UTEST(font, refuses_a_cell_or_a_name_that_cannot_be)
{
    EXPECT_TRUE(refuses("glyphs = \"g.png\"\ncoloured = false\n"
        "cell = [0, 10]\ncolumns = 16\nfirst_character = 32\ncount = 1\n"
        "widths = [0]\n"));
    EXPECT_TRUE(refuses("glyphs = \"../g.png\"\ncoloured = false\n"
        "cell = [8, 10]\ncolumns = 16\nfirst_character = 32\ncount = 1\n"
        "widths = [4]\n"));
    EXPECT_TRUE(refuses("glyphs = \"g.png\"\ncoloured = false\n"
        "cell = [8, 10]\ncolumns = 0\nfirst_character = 32\ncount = 1\n"
        "widths = [4]\n"));
}
