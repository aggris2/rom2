#include "import/structures.h"

#include <string.h>

#include "make_reg.h"
#include "utest.h"

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
