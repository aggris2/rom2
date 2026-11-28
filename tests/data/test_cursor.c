#include "data/cursor.h"

#include <string.h>

#include "utest.h"

#define WHOLE "sheet = \"s.png\"\nalpha = \"a.png\"\nframes = 1\n" \
    "columns = 1\ncell = [32, 32]\ntip = [4, 5]\n"

/* Reads a definition written as text. */
static bool reads(const char *text, Cursor *out)
{
    TomlTable *top = NULL;
    if (!toml_read((const uint8_t *)text, strlen(text), &top, NULL)) {
        return false;
    }
    bool ok = cursor_read(top, out);
    toml_free(top);
    return ok;
}

/* True if the definition is refused and `out` is left as it was. */
static bool refuses(const char *text)
{
    Cursor cursor;
    memset(&cursor, 0x5a, sizeof cursor);
    Cursor before = cursor;
    return !reads(text, &cursor)
        && memcmp(&cursor, &before, sizeof cursor) == 0;
}

UTEST(data_cursor, reads_the_files_the_cell_and_the_tip)
{
    Cursor cursor;
    ASSERT_TRUE(reads(WHOLE, &cursor));
    EXPECT_STREQ("s.png", cursor.sheet);
    EXPECT_STREQ("a.png", cursor.alpha);
    EXPECT_EQ(32u, cursor.cell_width);
    EXPECT_EQ(32u, cursor.cell_height);
    EXPECT_EQ(4u, cursor.tip_x);
    EXPECT_EQ(5u, cursor.tip_y);
}

UTEST(data_cursor, refuses_a_definition_that_lacks_a_part)
{
    EXPECT_TRUE(refuses("alpha = \"a.png\"\ncell = [32, 32]\ntip = [4, 4]\n"));
    EXPECT_TRUE(refuses("sheet = \"s.png\"\ncell = [32, 32]\ntip = [4, 4]\n"));
    EXPECT_TRUE(refuses("sheet = \"s.png\"\nalpha = \"a.png\"\n"
        "tip = [4, 4]\n"));
    /* A sprite that is no cursor has no tip. */
    EXPECT_TRUE(refuses("sheet = \"s.png\"\nalpha = \"a.png\"\n"
        "cell = [32, 32]\n"));
}

UTEST(data_cursor, refuses_a_name_that_leaves_its_folder)
{
    EXPECT_TRUE(refuses("sheet = \"../s.png\"\nalpha = \"a.png\"\n"
        "cell = [32, 32]\ntip = [4, 4]\n"));
    EXPECT_TRUE(refuses("sheet = \"s.png\"\nalpha = \"\"\n"
        "cell = [32, 32]\ntip = [4, 4]\n"));
}

UTEST(data_cursor, refuses_a_tip_that_is_not_a_pixel_of_the_cell)
{
    const char *start = "sheet = \"s.png\"\nalpha = \"a.png\"\n"
        "cell = [32, 16]\n";
    char text[200];
    const char *tips[5] = {"[32, 0]", "[0, 16]", "[-1, 0]", "[0, -1]",
        "[4]"};
    for (int i = 0; i < 5; i++) {
        snprintf(text, sizeof text, "%stip = %s\n", start, tips[i]);
        EXPECT_TRUE(refuses(text));
    }
    /* The last pixel of the cell is one. */
    Cursor cursor;
    snprintf(text, sizeof text, "%stip = [31, 15]\n", start);
    EXPECT_TRUE(reads(text, &cursor));
}

UTEST(data_cursor, refuses_a_cell_that_cannot_be)
{
    EXPECT_TRUE(refuses("sheet = \"s.png\"\nalpha = \"a.png\"\n"
        "cell = [0, 32]\ntip = [0, 0]\n"));
    EXPECT_TRUE(refuses("sheet = \"s.png\"\nalpha = \"a.png\"\n"
        "cell = [4097, 32]\ntip = [0, 0]\n"));
}
