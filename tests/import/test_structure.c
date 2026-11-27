#include "import/structure.h"

#include <string.h>

#include "expect.h"
#include "make_databin.h"
#include "make_reg.h"
#include "make_sprite.h"
#include "utest.h"

/* ======== structures ======== */

/* A registry with one structure, ID 5, of the given numbers. A mask of
 * NULL leaves "AnimMask" out, and `variable` adds "VariableSize". */
static size_t make_structure(MakeReg *m, int32_t width, int32_t height,
    int32_t phases, const char *mask, bool variable)
{
    make_reg_start(m);
    make_reg_section(m, "Global");
    make_reg_int(m, "Count", 1);
    make_reg_section(m, "Structure0");
    make_reg_int(m, "ID", 5);
    make_reg_text(m, "File", "Mill1\\house");
    make_reg_int(m, "TileWidth", width);
    make_reg_int(m, "FullHeight", height);
    make_reg_int(m, "Phases", phases);
    if (mask != NULL) {
        make_reg_text(m, "AnimMask", mask);
    }
    if (variable) {
        make_reg_int(m, "VariableSize", 1);
    }
    return make_reg_finish(m);
}

/* ---- structures_sprite_path ---- */

UTEST(structures, sprite_path_is_the_file_text_in_lower_case)
{
    static MakeReg m;
    Reg reg;
    char path[64] = "untouched";
    ASSERT_TRUE(reg_open(m.bytes, make_structure(&m, 3, 3, 1, "", false),
        &reg));

    ASSERT_TRUE(structures_sprite_path(&reg, 5, path, sizeof path));
    ASSERT_STREQ("structures/mill1/house.256", path);
    /* No such structure, and no room. */
    ASSERT_FALSE(structures_sprite_path(&reg, 6, path, sizeof path));
    ASSERT_FALSE(structures_sprite_path(&reg, 5, path, 26));
    ASSERT_STREQ("structures/mill1/house.256", path);
    ASSERT_TRUE(structures_sprite_path(&reg, 5, path, 27));
}

UTEST(structures, sprite_path_turns_every_capital_into_a_small_letter)
{
    static MakeReg m;
    Reg reg;
    char path[64];
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "Count", 1);
    make_reg_section(&m, "Structure0");
    make_reg_int(&m, "ID", 1);
    make_reg_text(&m, "File", "AZaz09_Inn\\HouZA");
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));

    ASSERT_TRUE(structures_sprite_path(&reg, 1, path, sizeof path));
    ASSERT_STREQ("structures/azaz09_inn/houza.256", path);
}

/* ---- structures_layout ---- */

UTEST(structures, layout_of_a_structure_that_stands_still)
{
    static MakeReg m;
    Reg reg;
    StructureLayout l;
    ASSERT_TRUE(reg_open(m.bytes, make_structure(&m, 3, 2, 1, "", false),
        &reg));

    ASSERT_TRUE(structures_layout(&reg, 5, 6, &l));

    ASSERT_EQ(3u, l.width);
    ASSERT_EQ(2u, l.height);
    ASSERT_EQ(1u, l.phases);
    ASSERT_FALSE(l.has_ruin);
    ASSERT_FALSE(l.is_pieces);
    ASSERT_EQ(0u, l.animated_count);
}

UTEST(structures, layout_with_twice_the_tiles_has_a_ruin)
{
    static MakeReg m;
    Reg reg;
    StructureLayout l;
    ASSERT_TRUE(reg_open(m.bytes, make_structure(&m, 3, 2, 1, "", false),
        &reg));

    ASSERT_TRUE(structures_layout(&reg, 5, 12, &l));

    ASSERT_EQ(1u, l.phases);
    ASSERT_TRUE(l.has_ruin);
}

UTEST(structures, layout_counts_only_the_marked_tiles_for_later_pictures)
{
    /* 2 by 2, 3 pictures, the two right tiles change: 4 + 2 * 2 tiles. */
    static MakeReg m;
    Reg reg;
    StructureLayout l;
    ASSERT_TRUE(reg_open(m.bytes, make_structure(&m, 2, 2, 3, "-+-+", false),
        &reg));

    ASSERT_TRUE(structures_layout(&reg, 5, 8, &l));
    ASSERT_EQ(3u, l.phases);
    ASSERT_EQ(2u, l.animated_count);
    ASSERT_FALSE(l.animated[0]);
    ASSERT_TRUE(l.animated[1]);
    ASSERT_FALSE(l.animated[2]);
    ASSERT_TRUE(l.animated[3]);
    ASSERT_FALSE(l.has_ruin);

    /* With 4 more tiles there is a ruin. */
    ASSERT_TRUE(structures_layout(&reg, 5, 12, &l));
    ASSERT_EQ(3u, l.phases);
    ASSERT_TRUE(l.has_ruin);
}

UTEST(structures, layout_without_a_mask_changes_every_tile)
{
    static MakeReg m;
    Reg reg;
    StructureLayout l;
    ASSERT_TRUE(reg_open(m.bytes, make_structure(&m, 2, 1, 3, NULL, false),
        &reg));

    ASSERT_TRUE(structures_layout(&reg, 5, 6, &l));
    ASSERT_EQ(3u, l.phases);
    ASSERT_EQ(2u, l.animated_count);

    /* The same with an empty mask. */
    ASSERT_TRUE(reg_open(m.bytes, make_structure(&m, 2, 1, 3, "", false),
        &reg));
    ASSERT_TRUE(structures_layout(&reg, 5, 6, &l));
    ASSERT_EQ(2u, l.animated_count);
}

UTEST(structures, layout_with_the_tiles_of_one_picture_stands_still)
{
    /* The registry says 2 pictures, and the file has the tiles of one. */
    static MakeReg m;
    Reg reg;
    StructureLayout l;
    ASSERT_TRUE(reg_open(m.bytes, make_structure(&m, 3, 4, 2, "", false),
        &reg));

    ASSERT_TRUE(structures_layout(&reg, 5, 12, &l));

    ASSERT_EQ(1u, l.phases);
    ASSERT_FALSE(l.has_ruin);
    ASSERT_EQ(0u, l.animated_count);
    ASSERT_FALSE(l.animated[0]);
}

UTEST(structures, layout_of_variable_size_is_pieces)
{
    static MakeReg m;
    Reg reg;
    StructureLayout l;
    ASSERT_TRUE(reg_open(m.bytes, make_structure(&m, 1, 1, 1, "", true),
        &reg));

    ASSERT_TRUE(structures_layout(&reg, 5, 9, &l));

    ASSERT_TRUE(l.is_pieces);
    ASSERT_EQ(9u, l.pieces);
    ASSERT_EQ(1u, l.width);
    ASSERT_EQ(1u, l.height);
    ASSERT_EQ(1u, l.phases);
    ASSERT_FALSE(l.has_ruin);
    /* A file with no tiles has no pieces. */
    ASSERT_FALSE(structures_layout(&reg, 5, 0, &l));
}

UTEST(structures, layout_refuses_a_file_whose_tiles_fit_nothing)
{
    static MakeReg m;
    Reg reg;
    StructureLayout l;
    memset(&l, 0x55, sizeof l);
    ASSERT_TRUE(reg_open(m.bytes, make_structure(&m, 2, 2, 3, "-+-+", false),
        &reg));

    /* 8 is without a ruin, 12 with one, 4 is one picture. */
    ASSERT_FALSE(structures_layout(&reg, 5, 0, &l));
    ASSERT_FALSE(structures_layout(&reg, 5, 3, &l));
    ASSERT_FALSE(structures_layout(&reg, 5, 7, &l));
    ASSERT_FALSE(structures_layout(&reg, 5, 9, &l));
    ASSERT_FALSE(structures_layout(&reg, 5, 16, &l));
    ASSERT_EQ(0x55555555u, l.width);
}

UTEST(structures, layout_refuses_a_mask_of_the_wrong_length)
{
    static MakeReg m;
    Reg reg;
    StructureLayout l;
    ASSERT_TRUE(reg_open(m.bytes, make_structure(&m, 2, 2, 3, "-+-", false),
        &reg));
    ASSERT_FALSE(structures_layout(&reg, 5, 8, &l));
    /* Not even with the 6 tiles that a mask of one '+' would give. */
    ASSERT_FALSE(structures_layout(&reg, 5, 6, &l));

    ASSERT_TRUE(reg_open(m.bytes, make_structure(&m, 2, 2, 3, "-+-+-", false),
        &reg));
    ASSERT_FALSE(structures_layout(&reg, 5, 8, &l));
}

UTEST(structures, layout_refuses_numbers_that_are_not_usable)
{
    static MakeReg m;
    Reg reg;
    StructureLayout l;
    ASSERT_TRUE(reg_open(m.bytes, make_structure(&m, 0, 2, 1, "", false),
        &reg));
    ASSERT_FALSE(structures_layout(&reg, 5, 2, &l));
    ASSERT_TRUE(reg_open(m.bytes, make_structure(&m, 2, -1, 1, "", false),
        &reg));
    ASSERT_FALSE(structures_layout(&reg, 5, 2, &l));
    ASSERT_TRUE(reg_open(m.bytes, make_structure(&m, 2, 2, 0, "", false),
        &reg));
    ASSERT_FALSE(structures_layout(&reg, 5, 4, &l));
    /* More tiles than STRUCTURE_MAX_TILES. */
    ASSERT_TRUE(reg_open(m.bytes, make_structure(&m, 17, 16, 1, "", false),
        &reg));
    ASSERT_FALSE(structures_layout(&reg, 5, 272, &l));
    ASSERT_TRUE(reg_open(m.bytes, make_structure(&m, 16, 16, 1, "", false),
        &reg));
    ASSERT_TRUE(structures_layout(&reg, 5, 256, &l));
    /* And a structure that is not there. */
    ASSERT_FALSE(structures_layout(&reg, 9, 256, &l));
}

/* ---- structures_tile, structures_ruin_tile ---- */

UTEST(structures, tile_of_the_first_picture_is_its_own_number)
{
    static MakeReg m;
    Reg reg;
    StructureLayout l;
    uint32_t tile = 99;
    ASSERT_TRUE(reg_open(m.bytes, make_structure(&m, 2, 2, 3, "-+-+", false),
        &reg));
    ASSERT_TRUE(structures_layout(&reg, 5, 12, &l));

    for (uint32_t i = 0; i < 4; i++) {
        ASSERT_TRUE(structures_tile(&l, 0, i, &tile));
        ASSERT_EQ(i, tile);
    }
}

UTEST(structures, tile_of_a_later_picture_is_new_only_where_marked)
{
    /* Tiles 0 to 3 are the first picture. 4 and 5 are tiles 1 and 3 of
     * the second, 6 and 7 of the third. The ruin is 8 to 11. */
    static MakeReg m;
    Reg reg;
    StructureLayout l;
    uint32_t tile = 99;
    ASSERT_TRUE(reg_open(m.bytes, make_structure(&m, 2, 2, 3, "-+-+", false),
        &reg));
    ASSERT_TRUE(structures_layout(&reg, 5, 12, &l));

    ASSERT_TRUE(structures_tile(&l, 1, 0, &tile));
    ASSERT_EQ(0u, tile);
    ASSERT_TRUE(structures_tile(&l, 1, 1, &tile));
    ASSERT_EQ(4u, tile);
    ASSERT_TRUE(structures_tile(&l, 1, 2, &tile));
    ASSERT_EQ(2u, tile);
    ASSERT_TRUE(structures_tile(&l, 1, 3, &tile));
    ASSERT_EQ(5u, tile);
    ASSERT_TRUE(structures_tile(&l, 2, 1, &tile));
    ASSERT_EQ(6u, tile);
    ASSERT_TRUE(structures_tile(&l, 2, 3, &tile));
    ASSERT_EQ(7u, tile);

    ASSERT_TRUE(structures_ruin_tile(&l, 0, &tile));
    ASSERT_EQ(8u, tile);
    ASSERT_TRUE(structures_ruin_tile(&l, 3, &tile));
    ASSERT_EQ(11u, tile);
}

UTEST(structures, tile_refuses_what_the_layout_does_not_have)
{
    static MakeReg m;
    Reg reg;
    StructureLayout l;
    uint32_t tile = 99;
    ASSERT_TRUE(reg_open(m.bytes, make_structure(&m, 2, 2, 3, "-+-+", false),
        &reg));
    ASSERT_TRUE(structures_layout(&reg, 5, 8, &l));

    ASSERT_FALSE(structures_tile(&l, 3, 0, &tile));
    ASSERT_FALSE(structures_tile(&l, 0, 4, &tile));
    /* No ruin in a file of 8. */
    ASSERT_FALSE(structures_ruin_tile(&l, 0, &tile));
    ASSERT_TRUE(structures_layout(&reg, 5, 12, &l));
    ASSERT_FALSE(structures_ruin_tile(&l, 4, &tile));
    ASSERT_EQ(99u, tile);

    /* Pieces are not a picture. */
    ASSERT_TRUE(reg_open(m.bytes, make_structure(&m, 1, 1, 1, "", true),
        &reg));
    ASSERT_TRUE(structures_layout(&reg, 5, 9, &l));
    ASSERT_FALSE(structures_tile(&l, 0, 0, &tile));
}

/* ======== structure_sheet ======== */

/* Adds a tile of 32 by 32 filled with one value. */
static void add_tile(MakeSprite *m, uint8_t value)
{
    static uint8_t pixels[32 * 32];
    memset(pixels, value, sizeof pixels);
    make_sprite_frame(m, 32, 32, pixels);
}

/* The pixel at `x`, `y` of the picture in cell `column`. */
static uint8_t at(const Sheet *s, uint32_t column, uint32_t x, uint32_t y)
{
    return s->pixels[(size_t)y * sheet_width(s)
        + (size_t)column * s->cell_width + x];
}

/* A layout of 2 by 2 with 3 pictures in which tiles 1 and 3 change. */
static StructureLayout make_layout(bool ruin)
{
    StructureLayout l;
    memset(&l, 0, sizeof l);
    l.width = 2;
    l.height = 2;
    l.phases = 3;
    l.animated[1] = true;
    l.animated[3] = true;
    l.animated_count = 2;
    l.has_ruin = ruin;
    return l;
}

UTEST(structure_sheet, build_puts_the_tiles_together_into_pictures)
{
    /* 12 tiles, each filled with its number from 1 up. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 12; i++) {
        add_tile(&m, (uint8_t)(i + 1));
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    StructureLayout layout = make_layout(true);
    StructureSheet s;

    ASSERT_TRUE(structure_sheet_build(&layout, &sprite, NULL, &s));

    ASSERT_EQ(64u, s.sheet.cell_width);
    ASSERT_EQ(64u, s.sheet.cell_height);
    ASSERT_EQ(3u, s.sheet.columns);
    ASSERT_EQ(1u, s.sheet.rows);
    /* The first picture: tiles 1 2 / 3 4. */
    ASSERT_EQ(1, at(&s.sheet, 0, 0, 0));
    ASSERT_EQ(1, at(&s.sheet, 0, 31, 31));
    ASSERT_EQ(2, at(&s.sheet, 0, 32, 0));
    ASSERT_EQ(3, at(&s.sheet, 0, 0, 32));
    ASSERT_EQ(4, at(&s.sheet, 0, 63, 63));
    /* The second: the right tiles are new, 5 and 6. */
    ASSERT_EQ(1, at(&s.sheet, 1, 0, 0));
    ASSERT_EQ(5, at(&s.sheet, 1, 32, 0));
    ASSERT_EQ(3, at(&s.sheet, 1, 0, 32));
    ASSERT_EQ(6, at(&s.sheet, 1, 32, 32));
    /* The third: 7 and 8. */
    ASSERT_EQ(7, at(&s.sheet, 2, 63, 31));
    ASSERT_EQ(8, at(&s.sheet, 2, 63, 32));
    ASSERT_EQ(3, at(&s.sheet, 2, 31, 63));
    /* The ruin: tiles 9 10 / 11 12. */
    ASSERT_EQ(1u, s.ruin.columns);
    ASSERT_EQ(64u, s.ruin.cell_width);
    ASSERT_EQ(9, at(&s.ruin, 0, 0, 0));
    ASSERT_EQ(10, at(&s.ruin, 0, 32, 31));
    ASSERT_EQ(11, at(&s.ruin, 0, 31, 32));
    ASSERT_EQ(12, at(&s.ruin, 0, 63, 63));
    ASSERT_TRUE(s.edges.pixels == NULL);
    ASSERT_TRUE(s.ruin_edges.pixels == NULL);
    structure_sheet_free(&s);
    ASSERT_TRUE(s.sheet.pixels == NULL);
    ASSERT_TRUE(s.ruin.pixels == NULL);
}

UTEST(structure_sheet, build_without_a_ruin_has_no_ruin_sheet)
{
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 8; i++) {
        add_tile(&m, (uint8_t)(i + 1));
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    StructureLayout layout = make_layout(false);
    StructureSheet s;

    ASSERT_TRUE(structure_sheet_build(&layout, &sprite, NULL, &s));

    ASSERT_EQ(3u, s.sheet.columns);
    ASSERT_TRUE(s.ruin.pixels == NULL);
    structure_sheet_free(&s);
}

UTEST(structure_sheet, build_puts_the_edges_on_sheets_of_their_own)
{
    static MakeSprite m;
    static MakeSprite e;
    make_sprite_start(&m);
    make_sprite_start(&e);
    for (uint8_t i = 0; i < 12; i++) {
        add_tile(&m, (uint8_t)(i + 1));
        add_tile(&e, (uint8_t)(i + 101));
    }
    Sprite256 sprite;
    Sprite256 edges;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ASSERT_TRUE(make_sprite_finish(&e, &edges));
    StructureLayout layout = make_layout(true);
    StructureSheet s;

    ASSERT_TRUE(structure_sheet_build(&layout, &sprite, &edges, &s));

    ASSERT_EQ(3u, s.edges.columns);
    ASSERT_EQ(64u, s.edges.cell_width);
    ASSERT_EQ(101, at(&s.edges, 0, 0, 0));
    ASSERT_EQ(105, at(&s.edges, 1, 32, 0));
    ASSERT_EQ(108, at(&s.edges, 2, 32, 32));
    ASSERT_EQ(112, at(&s.ruin_edges, 0, 63, 63));
    /* The figure's own sheets are as without edges. */
    ASSERT_EQ(5, at(&s.sheet, 1, 32, 0));
    ASSERT_EQ(12, at(&s.ruin, 0, 63, 63));
    structure_sheet_free(&s);
    ASSERT_TRUE(s.edges.pixels == NULL);
    ASSERT_TRUE(s.ruin_edges.pixels == NULL);
}

UTEST(structure_sheet, build_of_pieces_gives_one_tile_in_each_cell)
{
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 5; i++) {
        add_tile(&m, (uint8_t)(i + 1));
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    StructureLayout layout;
    memset(&layout, 0, sizeof layout);
    layout.width = 1;
    layout.height = 1;
    layout.phases = 1;
    layout.is_pieces = true;
    layout.pieces = 5;
    StructureSheet s;

    ASSERT_TRUE(structure_sheet_build(&layout, &sprite, NULL, &s));

    ASSERT_EQ(5u, s.sheet.columns);
    ASSERT_EQ(32u, s.sheet.cell_width);
    ASSERT_EQ(32u, s.sheet.cell_height);
    for (uint32_t i = 0; i < 5; i++) {
        ASSERT_EQ(i + 1, at(&s.sheet, i, 0, 0));
        ASSERT_EQ(i + 1, at(&s.sheet, i, 31, 31));
    }
    ASSERT_TRUE(s.ruin.pixels == NULL);
    structure_sheet_free(&s);
}

UTEST(structure_sheet, build_refuses_a_file_with_too_few_tiles)
{
    /* The layout with a ruin needs 12 tiles. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 11; i++) {
        add_tile(&m, 1);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    StructureLayout layout = make_layout(true);
    StructureSheet s;
    memset(&s, 0, sizeof s);

    ASSERT_FALSE(structure_sheet_build(&layout, &sprite, NULL, &s));

    ASSERT_TRUE(s.sheet.pixels == NULL);
    ASSERT_TRUE(s.ruin.pixels == NULL);
}

UTEST(structure_sheet, build_refuses_a_tile_that_is_not_32_by_32)
{
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 7; i++) {
        add_tile(&m, 1);
    }
    make_sprite_solid(&m, 8, 8, 1);
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    StructureLayout layout = make_layout(false);
    StructureSheet s;
    memset(&s, 0, sizeof s);

    ASSERT_FALSE(structure_sheet_build(&layout, &sprite, NULL, &s));
    ASSERT_TRUE(s.sheet.pixels == NULL);
}

UTEST(structure_sheet, build_refuses_edges_with_another_number_of_tiles)
{
    static MakeSprite m;
    static MakeSprite e;
    make_sprite_start(&m);
    make_sprite_start(&e);
    for (uint8_t i = 0; i < 8; i++) {
        add_tile(&m, 1);
    }
    for (uint8_t i = 0; i < 7; i++) {
        add_tile(&e, 1);
    }
    Sprite256 sprite;
    Sprite256 edges;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ASSERT_TRUE(make_sprite_finish(&e, &edges));
    StructureLayout layout = make_layout(false);
    StructureSheet s;
    memset(&s, 0, sizeof s);

    ASSERT_FALSE(structure_sheet_build(&layout, &sprite, &edges, &s));
    ASSERT_TRUE(s.sheet.pixels == NULL);
    ASSERT_TRUE(s.edges.pixels == NULL);
}

/* ======== structure_toml ======== */

/* Two structures:
 *
 *     Structure0   ID 2   a windmill, which turns
 *     Structure1   ID 1   a hut, which has an empty text where the
 *                         windmill has its lists
 */
static size_t make_structures(MakeReg *m)
{
    const int32_t frames[] = {0, 1, 2};
    const int32_t times[] = {2, 2, 3};

    make_reg_start(m);
    make_reg_section(m, "Global");
    make_reg_int(m, "Count", 2);

    make_reg_section(m, "Structure0");
    make_reg_int(m, "ID", 2);
    make_reg_text(m, "DescText", "Windmill 1");
    make_reg_text(m, "Picture", "windmill");
    make_reg_int(m, "IconID", 230);
    make_reg_int(m, "TileWidth", 3);
    make_reg_int(m, "TileHeight", 2);
    make_reg_int(m, "FullHeight", 3);
    make_reg_int(m, "SelectionX1", 14);
    make_reg_int(m, "SelectionX2", 80);
    make_reg_int(m, "SelectionY1", 6);
    make_reg_int(m, "SelectionY2", 83);
    make_reg_int(m, "ShadowY", 40);
    make_reg_int_list(m, "AnimFrame", frames, 3);
    make_reg_int_list(m, "AnimTime", times, 3);

    make_reg_section(m, "Structure1");
    make_reg_int(m, "ID", 1);
    make_reg_text(m, "DescText", "Goblin's Hut 1");
    make_reg_text(m, "Picture", "");
    make_reg_int(m, "TileWidth", 3);
    make_reg_int(m, "TileHeight", 2);
    make_reg_int(m, "FullHeight", 3);
    make_reg_text(m, "AnimFrame", "");
    make_reg_text(m, "AnimTime", "");
    make_reg_int(m, "Indestructible", 1);
    make_reg_int(m, "Usable", 1);
    return make_reg_finish(m);
}

/* A layout and sheets of `columns` pictures of 3 by 3 tiles. No pixels:
 * the writer only looks whether a sheet is there. */
static StructureSheet make_sheet(StructureLayout *layout, uint32_t columns)
{
    StructureSheet s;
    memset(&s, 0, sizeof s);
    memset(layout, 0, sizeof *layout);
    layout->width = 3;
    layout->height = 3;
    layout->phases = columns;
    s.sheet.cell_width = 96;
    s.sheet.cell_height = 96;
    s.sheet.columns = columns;
    s.sheet.rows = 1;
    return s;
}

/* A data.bin whose buildings are a hut and a mill, in that order: rows
 * 1 and 2. */
static void make_buildings(ByteBuffer *b)
{
    const char *const columns[] = {
        "sizeX", "sizeY", "scanRange", "healthMax", "Passability",
        "BuildingPresent"
    };
    const char *const empty[] = {"a"};
    const int32_t hut[] = {3, 2, 4, 300, 55, 63};
    const int32_t mill[] = {3, 3, 5, 1000, 186, 511};

    put_empty_first_tables(b);
    put_header(b, empty, 1);  /* monsters */
    put_i32(b, 1);
    put_header(b, empty, 1);  /* humans */
    put_i32(b, 1);
    put_header(b, columns, 6);
    put_i32(b, 3);
    put_row(b, "Goblin Hut", hut, 6);
    put_row(b, "Mill", mill, 6);
    put_header(b, empty, 1);  /* spells */
    put_i32(b, 2);
}

UTEST(structure_toml, folder_name_is_the_id_and_the_name)
{
    static MakeReg m;
    Reg reg;
    char name[NAME_FOLDER_SIZE] = "untouched";
    ASSERT_TRUE(reg_open(m.bytes, make_structures(&m), &reg));

    ASSERT_TRUE(structure_toml_folder_name(&reg, 1, name));
    ASSERT_STREQ("001_goblin_s_hut_1", name);
    ASSERT_TRUE(structure_toml_folder_name(&reg, 2, name));
    ASSERT_STREQ("002_windmill_1", name);
    ASSERT_FALSE(structure_toml_folder_name(&reg, 3, name));
    ASSERT_STREQ("002_windmill_1", name);
}

UTEST(structure_toml, write_gives_the_whole_file_of_a_structure)
{
    static MakeReg m;
    static uint8_t pixels[1];
    Reg reg;
    StructureLayout layout;
    StructureSheet sheet = make_sheet(&layout, 3);
    sheet.edges.pixels = pixels;
    sheet.ruin.pixels = pixels;
    sheet.ruin_edges.pixels = pixels;
    ByteBuffer file = {0};
    ByteBuffer b = {0};
    DataBin bin;
    make_buildings(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));
    ASSERT_TRUE(reg_open(m.bytes, make_structures(&m), &reg));

    ASSERT_TRUE(structure_toml_write(&b, &reg, 2, &layout, &sheet, &bin));

    ASSERT_TRUE(holds(&b,
        "# Windmill 1\n"
        "id = 2\n"
        "name = \"Windmill 1\"\n"
        "picture = \"windmill\"\n"
        "icon_id = 230\n"
        "size = [3, 2]\n"
        "height = 3\n"
        "selection = [14, 6, 80, 83]\n"
        "shadow_y = 40\n"
        "scan_range = 5\n"
        "health = 1000\n"
        "passability = 186\n"
        "building_present = 511\n"
        "\n"
        "[sprite]\n"
        "sheet = \"sprites.png\"\n"
        "edges = \"edges.png\"\n"
        "cell = [96, 96]\n"
        "columns = 3\n"
        "frames = [0, 1, 2]\n"
        "ticks = [2, 2, 3]\n"
        "ruin = \"ruin.png\"\n"
        "ruin_edges = \"ruin_edges.png\"\n"));
    buffer_free(&b);
    buffer_free(&file);
}

UTEST(structure_toml, write_of_a_structure_that_stands_still)
{
    /* The hut: empty texts for its lists and its picture, no edges, no
     * ruin, and row 1 of the buildings. */
    static MakeReg m;
    Reg reg;
    StructureLayout layout;
    StructureSheet sheet = make_sheet(&layout, 1);
    ByteBuffer file = {0};
    ByteBuffer b = {0};
    DataBin bin;
    make_buildings(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));
    ASSERT_TRUE(reg_open(m.bytes, make_structures(&m), &reg));

    ASSERT_TRUE(structure_toml_write(&b, &reg, 1, &layout, &sheet, &bin));

    ASSERT_TRUE(holds(&b,
        "# Goblin's Hut 1\n"
        "id = 1\n"
        "name = \"Goblin's Hut 1\"\n"
        "size = [3, 2]\n"
        "height = 3\n"
        "indestructible = 1\n"
        "usable = 1\n"
        "scan_range = 4\n"
        "health = 300\n"
        "passability = 55\n"
        "building_present = 63\n"
        "\n"
        "[sprite]\n"
        "sheet = \"sprites.png\"\n"
        "cell = [96, 96]\n"
        "columns = 1\n"));
    buffer_free(&b);
    buffer_free(&file);
}

UTEST(structure_toml, write_without_data_bin_or_without_its_row)
{
    static MakeReg m;
    Reg reg;
    StructureLayout layout;
    StructureSheet sheet = make_sheet(&layout, 3);
    ByteBuffer b = {0};
    ASSERT_TRUE(reg_open(m.bytes, make_structures(&m), &reg));

    ASSERT_TRUE(structure_toml_write(&b, &reg, 2, &layout, &sheet, NULL));
    ASSERT_FALSE(has(&b, "health"));
    ASSERT_TRUE(has(&b, "shadow_y = 40\n\n[sprite]"));
    buffer_free(&b);

    /* A data.bin with no buildings at all. */
    ByteBuffer file = {0};
    const char *const empty[] = {"a"};
    DataBin bin;
    put_empty_first_tables(&file);
    put_header(&file, empty, 1);
    put_i32(&file, 1);
    put_header(&file, empty, 1);
    put_i32(&file, 1);
    put_empty_last_tables(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));
    ASSERT_TRUE(structure_toml_write(&b, &reg, 2, &layout, &sheet, &bin));
    ASSERT_FALSE(has(&b, "health"));
    buffer_free(&b);
    buffer_free(&file);
}

UTEST(structure_toml, write_leaves_the_lists_out_when_there_is_one_picture)
{
    /* The windmill's file turned out to hold one picture. */
    static MakeReg m;
    Reg reg;
    StructureLayout layout;
    StructureSheet sheet = make_sheet(&layout, 1);
    ByteBuffer b = {0};
    ASSERT_TRUE(reg_open(m.bytes, make_structures(&m), &reg));

    ASSERT_TRUE(structure_toml_write(&b, &reg, 2, &layout, &sheet, NULL));

    ASSERT_TRUE(has(&b, "columns = 1\n"));
    ASSERT_FALSE(has(&b, "frames"));
    ASSERT_FALSE(has(&b, "ticks"));
    buffer_free(&b);
}

UTEST(structure_toml, write_of_pieces_says_so_and_has_no_lists)
{
    static MakeReg m;
    Reg reg;
    StructureLayout layout;
    StructureSheet sheet = make_sheet(&layout, 9);
    layout.is_pieces = true;
    layout.pieces = 9;
    layout.phases = 1;
    sheet.sheet.cell_width = 32;
    sheet.sheet.cell_height = 32;
    ByteBuffer b = {0};
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "Count", 1);
    make_reg_section(&m, "Structure0");
    make_reg_int(&m, "ID", 33);
    make_reg_text(&m, "DescText", "Bridge");
    make_reg_int(&m, "TileWidth", 1);
    make_reg_int(&m, "TileHeight", 1);
    make_reg_int(&m, "VariableSize", 1);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));

    ASSERT_TRUE(structure_toml_write(&b, &reg, 33, &layout, &sheet, NULL));

    ASSERT_TRUE(has(&b, "variable_size = 1\n"));
    ASSERT_TRUE(has(&b, "cell = [32, 32]\ncolumns = 9\n"));
    ASSERT_FALSE(has(&b, "frames"));
    buffer_free(&b);
}

UTEST(structure_toml, write_refuses_a_frame_that_is_not_on_the_sheet)
{
    /* The windmill's list shows pictures 0, 1 and 2, and the sheet has
     * two. */
    static MakeReg m;
    Reg reg;
    StructureLayout layout;
    StructureSheet sheet = make_sheet(&layout, 2);
    ByteBuffer b = {0};
    ASSERT_TRUE(reg_open(m.bytes, make_structures(&m), &reg));

    ASSERT_FALSE(structure_toml_write(&b, &reg, 2, &layout, &sheet, NULL));
    /* And a structure that is not there. */
    ASSERT_FALSE(structure_toml_write(&b, &reg, 9, &layout, &sheet, NULL));
    buffer_free(&b);
}

UTEST(structure_toml, write_refuses_lists_of_different_length)
{
    const int32_t frames[] = {0, 1, 2};
    const int32_t times[] = {2, 2};
    static MakeReg m;
    Reg reg;
    StructureLayout layout;
    StructureSheet sheet = make_sheet(&layout, 3);
    ByteBuffer b = {0};
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "Count", 1);
    make_reg_section(&m, "Structure0");
    make_reg_int(&m, "ID", 1);
    make_reg_int_list(&m, "AnimFrame", frames, 3);
    make_reg_int_list(&m, "AnimTime", times, 2);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));

    ASSERT_FALSE(structure_toml_write(&b, &reg, 1, &layout, &sheet, NULL));
    buffer_free(&b);
}
