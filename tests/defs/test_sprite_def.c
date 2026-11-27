#include "defs/sprite_def.h"

#include <string.h>

#include "utest.h"

/* Reads a definition written as text. */
static bool reads(const char *text, SpriteDef *out)
{
    TomlTable *top = NULL;
    if (!toml_read((const uint8_t *)text, strlen(text), &top, NULL)) {
        return false;
    }
    bool ok = sprite_def_read(top, out);
    toml_free(top);
    return ok;
}

/* True if the definition is refused and `out` is left as it was. */
static bool refuses(const char *text)
{
    SpriteDef def;
    memset(&def, 0x5a, sizeof def);
    SpriteDef before = def;
    return !reads(text, &def) && memcmp(&def, &before, sizeof def) == 0;
}

UTEST(sprite_def, reads_the_sheet_the_cell_and_the_anchor)
{
    SpriteDef def;
    ASSERT_TRUE(reads(
        "id = 1\n"
        "[sprite]\n"
        "sheet = \"sprites.png\"\n"
        "edges = \"edges.png\"\n"
        "cell = [64, 48]\n"
        "anchor = [32, 46]\n", &def));
    EXPECT_STREQ("sprites.png", def.sheet);
    EXPECT_EQ(64u, def.cell_width);
    EXPECT_EQ(48u, def.cell_height);
    EXPECT_EQ(32, def.anchor_x);
    EXPECT_EQ(46, def.anchor_y);
}

UTEST(sprite_def, an_anchor_may_lie_outside_the_cell)
{
    SpriteDef def;
    ASSERT_TRUE(reads("[sprite]\nsheet = \"s.png\"\ncell = [1, 4096]\n"
        "anchor = [-4096, 4096]\n", &def));
    EXPECT_EQ(1u, def.cell_width);
    EXPECT_EQ(4096u, def.cell_height);
    EXPECT_EQ(-4096, def.anchor_x);
    EXPECT_EQ(4096, def.anchor_y);
}

UTEST(sprite_def, takes_the_longest_name_there_is_room_for)
{
    char text[200] = "[sprite]\ncell = [8, 8]\nanchor = [0, 0]\nsheet = \"";
    size_t start = strlen(text);
    memset(text + start, 'n', 63);
    strcpy(text + start + 63, "\"\n");
    SpriteDef def;
    ASSERT_TRUE(reads(text, &def));
    EXPECT_EQ(63u, strlen(def.sheet));
    /* One more is one too many. */
    memset(text + start, 'n', 64);
    strcpy(text + start + 64, "\"\n");
    EXPECT_TRUE(refuses(text));
}

UTEST(sprite_def, refuses_a_definition_that_lacks_a_part)
{
    EXPECT_TRUE(refuses("id = 1\n"));
    EXPECT_TRUE(refuses("sprite = 1\n"));
    EXPECT_TRUE(refuses("[sprite]\ncell = [8, 8]\nanchor = [0, 0]\n"));
    EXPECT_TRUE(refuses("[sprite]\nsheet = \"s.png\"\nanchor = [0, 0]\n"));
    EXPECT_TRUE(refuses("[sprite]\nsheet = \"s.png\"\ncell = [8, 8]\n"));
}

UTEST(sprite_def, refuses_a_part_of_the_wrong_kind_or_length)
{
    EXPECT_TRUE(refuses(
        "[sprite]\nsheet = 5\ncell = [8, 8]\nanchor = [0, 0]\n"));
    EXPECT_TRUE(refuses(
        "[sprite]\nsheet = \"s.png\"\ncell = 8\nanchor = [0, 0]\n"));
    EXPECT_TRUE(refuses(
        "[sprite]\nsheet = \"s.png\"\ncell = [8]\nanchor = [0, 0]\n"));
    EXPECT_TRUE(refuses(
        "[sprite]\nsheet = \"s.png\"\ncell = [8, 8, 8]\nanchor = [0, 0]\n"));
    EXPECT_TRUE(refuses(
        "[sprite]\nsheet = \"s.png\"\ncell = [8, 8]\nanchor = [0]\n"));
    EXPECT_TRUE(refuses(
        "[sprite]\nsheet = \"s.png\"\ncell = [8, 8]\nanchor = \"x\"\n"));
}

UTEST(sprite_def, refuses_a_cell_with_no_size_or_too_large)
{
    EXPECT_TRUE(refuses(
        "[sprite]\nsheet = \"s.png\"\ncell = [0, 8]\nanchor = [0, 0]\n"));
    EXPECT_TRUE(refuses(
        "[sprite]\nsheet = \"s.png\"\ncell = [8, 0]\nanchor = [0, 0]\n"));
    EXPECT_TRUE(refuses(
        "[sprite]\nsheet = \"s.png\"\ncell = [-8, 8]\nanchor = [0, 0]\n"));
    EXPECT_TRUE(refuses(
        "[sprite]\nsheet = \"s.png\"\ncell = [4097, 8]\nanchor = [0, 0]\n"));
    EXPECT_TRUE(refuses(
        "[sprite]\nsheet = \"s.png\"\ncell = [8, 4097]\nanchor = [0, 0]\n"));
}

UTEST(sprite_def, refuses_an_anchor_too_far_from_the_cell)
{
    EXPECT_TRUE(refuses(
        "[sprite]\nsheet = \"s.png\"\ncell = [8, 8]\nanchor = [4097, 0]\n"));
    EXPECT_TRUE(refuses(
        "[sprite]\nsheet = \"s.png\"\ncell = [8, 8]\nanchor = [0, 4097]\n"));
    EXPECT_TRUE(refuses(
        "[sprite]\nsheet = \"s.png\"\ncell = [8, 8]\nanchor = [-4097, 0]\n"));
    EXPECT_TRUE(refuses(
        "[sprite]\nsheet = \"s.png\"\ncell = [8, 8]\nanchor = [0, -4097]\n"));
}

UTEST(sprite_def, refuses_a_sheet_that_is_not_a_plain_file_name)
{
    EXPECT_TRUE(refuses(
        "[sprite]\nsheet = \"\"\ncell = [8, 8]\nanchor = [0, 0]\n"));
    EXPECT_TRUE(refuses(
        "[sprite]\nsheet = \"a/b.png\"\ncell = [8, 8]\nanchor = [0, 0]\n"));
    EXPECT_TRUE(refuses(
        "[sprite]\nsheet = \"a\\\\b.png\"\ncell = [8, 8]\nanchor = [0, 0]\n"));
    EXPECT_TRUE(refuses(
        "[sprite]\nsheet = \"..\"\ncell = [8, 8]\nanchor = [0, 0]\n"));
    EXPECT_TRUE(refuses(
        "[sprite]\nsheet = \".\"\ncell = [8, 8]\nanchor = [0, 0]\n"));
}
