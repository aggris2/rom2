#include "data/sprite_sheet.h"

#include <string.h>

#include "utest.h"

/* Reads a definition written as text. */
static bool reads(const char *text, SpriteSheet *out)
{
    TomlTable *top = NULL;
    if (!toml_read((const uint8_t *)text, strlen(text), &top, NULL)) {
        return false;
    }
    bool ok = sprite_sheet_read(top, out);
    toml_free(top);
    return ok;
}

/* True if the definition is refused and `out` is left as it was. */
static bool refuses(const char *text)
{
    SpriteSheet def;
    memset(&def, 0x5a, sizeof def);
    SpriteSheet before = def;
    return !reads(text, &def) && memcmp(&def, &before, sizeof def) == 0;
}

UTEST(sprite_sheet, reads_the_sheet_the_cell_and_the_anchor)
{
    SpriteSheet def;
    ASSERT_TRUE(reads(
        "id = 1\n"
        "[sprite]\n"
        "sheet = \"sprites.png\"\n"
        "edges = \"edges.png\"\n"
        "cell = [64, 48]\n"
        "anchor = [32, 46]\n", &def));
    EXPECT_STREQ("sprites.png", def.file);
    EXPECT_STREQ("edges.png", def.edges);
    EXPECT_EQ(64u, def.cell_width);
    EXPECT_EQ(48u, def.cell_height);
    EXPECT_EQ(32, def.anchor_x);
    EXPECT_EQ(46, def.anchor_y);
}

UTEST(sprite_sheet, an_anchor_may_lie_outside_the_cell)
{
    SpriteSheet def;
    ASSERT_TRUE(reads("[sprite]\nsheet = \"s.png\"\ncell = [1, 4096]\n"
        "anchor = [-4096, 4096]\n", &def));
    EXPECT_EQ(1u, def.cell_width);
    EXPECT_EQ(4096u, def.cell_height);
    EXPECT_EQ(-4096, def.anchor_x);
    EXPECT_EQ(4096, def.anchor_y);
}

UTEST(sprite_sheet, takes_the_longest_name_there_is_room_for)
{
    char text[200] = "[sprite]\ncell = [8, 8]\nanchor = [0, 0]\nsheet = \"";
    size_t start = strlen(text);
    memset(text + start, 'n', 63);
    strcpy(text + start + 63, "\"\n");
    SpriteSheet def;
    ASSERT_TRUE(reads(text, &def));
    EXPECT_EQ(63u, strlen(def.file));
    /* One more is one too many. */
    memset(text + start, 'n', 64);
    strcpy(text + start + 64, "\"\n");
    EXPECT_TRUE(refuses(text));
}

UTEST(sprite_sheet, refuses_a_definition_that_lacks_a_part)
{
    EXPECT_TRUE(refuses("id = 1\n"));
    EXPECT_TRUE(refuses("sprite = 1\n"));
    EXPECT_TRUE(refuses("[sprite]\ncell = [8, 8]\nanchor = [0, 0]\n"));
    EXPECT_TRUE(refuses("[sprite]\nsheet = \"s.png\"\nanchor = [0, 0]\n"));
    EXPECT_TRUE(refuses("[sprite]\nsheet = \"s.png\"\ncell = [8, 8]\n"));
}

UTEST(sprite_sheet, refuses_a_part_of_the_wrong_kind_or_length)
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

UTEST(sprite_sheet, refuses_a_cell_with_no_size_or_too_large)
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

UTEST(sprite_sheet, refuses_an_anchor_too_far_from_the_cell)
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

UTEST(sprite_sheet, refuses_a_sheet_that_is_not_a_plain_file_name)
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

UTEST(sprite_sheet, a_sheet_need_not_have_edges)
{
    SpriteSheet def;
    ASSERT_TRUE(reads("[sprite]\nsheet = \"s.png\"\ncell = [8, 8]\n"
        "anchor = [0, 0]\n", &def));
    EXPECT_STREQ("", def.edges);
}

UTEST(sprite_sheet, refuses_edges_that_are_not_a_plain_file_name)
{
    EXPECT_TRUE(refuses("[sprite]\nsheet = \"s.png\"\nedges = \"\"\n"
        "cell = [8, 8]\nanchor = [0, 0]\n"));
    EXPECT_TRUE(refuses("[sprite]\nsheet = \"s.png\"\nedges = \"../e.png\"\n"
        "cell = [8, 8]\nanchor = [0, 0]\n"));
    EXPECT_TRUE(refuses("[sprite]\nsheet = \"s.png\"\nedges = 5\n"
        "cell = [8, 8]\nanchor = [0, 0]\n"));
}

/* A picture of 4 by 1 with a palette whose colour 0 is see-through, as
 * the importer writes a sheet. */
static Png picture_of(uint8_t *pixels)
{
    Png png;
    memset(&png, 0, sizeof png);
    png.width = 4;
    png.height = 1;
    png.kind = PNG_INDEXED;
    png.pixels = pixels;
    memset(png.alpha, 255, sizeof png.alpha);
    png.alpha[0] = 0;
    return png;
}

UTEST(sprite_sheet, join_takes_the_figure_then_the_edge_then_nothing)
{
    uint8_t figure_pixels[4] = {7, 0, 0, 9};
    uint8_t edge_pixels[4] = {0, 5, 0, 6};
    Png figure = picture_of(figure_pixels);
    Png edges = picture_of(edge_pixels);
    uint8_t colours[4];
    uint8_t solid[4];
    ASSERT_TRUE(sprite_sheet_join(&figure, &edges, colours, solid));
    /* The figure. */
    EXPECT_EQ(7u, colours[0]);
    EXPECT_EQ(255u, solid[0]);
    /* The edge, where the figure is empty. */
    EXPECT_EQ(5u, colours[1]);
    EXPECT_EQ(128u, solid[1]);
    /* Nothing. */
    EXPECT_EQ(0u, colours[2]);
    EXPECT_EQ(0u, solid[2]);
    /* The figure wins where both have a pixel. */
    EXPECT_EQ(9u, colours[3]);
    EXPECT_EQ(255u, solid[3]);
}

UTEST(sprite_sheet, join_without_edges_keeps_the_figure_alone)
{
    uint8_t figure_pixels[4] = {7, 0, 3, 0};
    Png figure = picture_of(figure_pixels);
    uint8_t colours[4];
    uint8_t solid[4];
    ASSERT_TRUE(sprite_sheet_join(&figure, NULL, colours, solid));
    EXPECT_EQ(7u, colours[0]);
    EXPECT_EQ(255u, solid[0]);
    EXPECT_EQ(0u, solid[1]);
    EXPECT_EQ(3u, colours[2]);
    EXPECT_EQ(255u, solid[2]);
    EXPECT_EQ(0u, solid[3]);
}

UTEST(sprite_sheet, join_goes_by_what_the_palette_makes_see_through)
{
    /* Colour 4 is see-through in the figure, and colour 0 is not. */
    uint8_t figure_pixels[4] = {4, 0, 4, 2};
    uint8_t edge_pixels[4] = {1, 1, 0, 1};
    Png figure = picture_of(figure_pixels);
    Png edges = picture_of(edge_pixels);
    figure.alpha[0] = 255;
    figure.alpha[4] = 0;
    uint8_t colours[4];
    uint8_t solid[4];
    ASSERT_TRUE(sprite_sheet_join(&figure, &edges, colours, solid));
    EXPECT_EQ(1u, colours[0]);
    EXPECT_EQ(128u, solid[0]);
    EXPECT_EQ(0u, colours[1]);
    EXPECT_EQ(255u, solid[1]);
    EXPECT_EQ(0u, solid[2]);
    EXPECT_EQ(2u, colours[3]);
    EXPECT_EQ(255u, solid[3]);
}

UTEST(sprite_sheet, join_refuses_pictures_it_cannot_join)
{
    uint8_t figure_pixels[4] = {7, 0, 0, 9};
    uint8_t edge_pixels[4] = {0, 5, 0, 6};
    uint8_t colours[4] = {1, 1, 1, 1};
    uint8_t solid[4] = {2, 2, 2, 2};
    Png figure = picture_of(figure_pixels);
    Png edges = picture_of(edge_pixels);
    Png wide = edges;
    wide.width = 2;
    wide.height = 2;
    Png tall = edges;
    tall.height = 2;
    Png coloured = edges;
    coloured.kind = PNG_RGB;
    Png grey = figure;
    grey.kind = PNG_GREY;
    EXPECT_FALSE(sprite_sheet_join(&figure, &wide, colours, solid));
    EXPECT_FALSE(sprite_sheet_join(&figure, &tall, colours, solid));
    EXPECT_FALSE(sprite_sheet_join(&figure, &coloured, colours, solid));
    EXPECT_FALSE(sprite_sheet_join(&grey, &edges, colours, solid));
    EXPECT_FALSE(sprite_sheet_join(&grey, NULL, colours, solid));
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(1u, colours[i]);
        EXPECT_EQ(2u, solid[i]);
    }
}

