#include "import/object.h"

#include <string.h>

#include "expect.h"
#include "make_reg.h"
#include "make_sprite.h"
#include "utest.h"

/* ======== objects ======== */

/* Four objects over two sprite files:
 *
 *     Object0   ID 0    a pine: file 0, 7 frames from frame 0
 *     Object1   ID 24   a rock: file 1, 1 frame at frame 3
 *     Object2   ID 2    a dead pine, which says 7 frames of a file that
 *                       will have 1
 *     Object3   ID 30   a child of the rock that states nothing more
 */
static size_t make_objects(MakeReg *m)
{
    make_reg_start(m);
    make_reg_section(m, "Global");
    make_reg_int(m, "ObjectCount", 4);
    make_reg_section(m, "Files");
    make_reg_text(m, "File0", "pine1\\sprites");
    make_reg_text(m, "File1", "stones\\sprites");

    make_reg_section(m, "Object0");
    make_reg_int(m, "ID", 0);
    make_reg_int(m, "File", 0);
    make_reg_int(m, "Index", 0);
    make_reg_int(m, "Phases", 7);

    make_reg_section(m, "Object1");
    make_reg_int(m, "ID", 24);
    make_reg_int(m, "File", 1);
    make_reg_int(m, "Index", 3);
    make_reg_int(m, "Phases", 1);

    make_reg_section(m, "Object2");
    make_reg_int(m, "ID", 2);
    make_reg_int(m, "File", 0);
    make_reg_int(m, "Phases", 7);

    make_reg_section(m, "Object3");
    make_reg_int(m, "ID", 30);
    make_reg_int(m, "Parent", 24);
    return make_reg_finish(m);
}

/* ---- objects_sprite_path ---- */

UTEST(objects, sprite_path_is_the_file_text_under_objects)
{
    static MakeReg m;
    Reg reg;
    char path[64];
    ASSERT_TRUE(reg_open(m.bytes, make_objects(&m), &reg));

    ASSERT_TRUE(objects_sprite_path(&reg, 0, path, sizeof path));
    ASSERT_STREQ("objects/pine1/sprites.256", path);
    ASSERT_TRUE(objects_sprite_path(&reg, 24, path, sizeof path));
    ASSERT_STREQ("objects/stones/sprites.256", path);
    /* The child uses its parent's file. */
    ASSERT_TRUE(objects_sprite_path(&reg, 30, path, sizeof path));
    ASSERT_STREQ("objects/stones/sprites.256", path);
}

UTEST(objects, sprite_path_refuses_what_it_cannot_build)
{
    static MakeReg m;
    Reg reg;
    char path[64] = "untouched";
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "ObjectCount", 2);
    make_reg_section(&m, "Files");
    make_reg_text(&m, "File0", "pine1\\sprites");
    make_reg_section(&m, "Object0");
    make_reg_int(&m, "ID", 0);
    make_reg_int(&m, "File", 5);
    make_reg_section(&m, "Object1");
    make_reg_int(&m, "ID", 1);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));

    /* A file number the registry has no text for, no file at all, and no
     * such object. */
    ASSERT_FALSE(objects_sprite_path(&reg, 0, path, sizeof path));
    ASSERT_FALSE(objects_sprite_path(&reg, 1, path, sizeof path));
    ASSERT_FALSE(objects_sprite_path(&reg, 9, path, sizeof path));
    ASSERT_STREQ("untouched", path);
}

UTEST(objects, sprite_path_refuses_a_path_that_does_not_fit)
{
    static MakeReg m;
    Reg reg;
    char path[32];
    ASSERT_TRUE(reg_open(m.bytes, make_objects(&m), &reg));
    memset(path, 'x', sizeof path);

    /* "objects/pine1/sprites.256" is 25 characters: 26 bytes fit it. */
    ASSERT_FALSE(objects_sprite_path(&reg, 0, path, 25));
    ASSERT_EQ('x', path[0]);
    ASSERT_TRUE(objects_sprite_path(&reg, 0, path, 26));
    ASSERT_EQ('x', path[26]);
}

/* ---- objects_frame_range ---- */

UTEST(objects, frame_range_is_index_and_phases)
{
    static MakeReg m;
    Reg reg;
    uint32_t first = 99;
    uint32_t count = 99;
    ASSERT_TRUE(reg_open(m.bytes, make_objects(&m), &reg));

    ASSERT_TRUE(objects_frame_range(&reg, 0, 7, &first, &count));
    ASSERT_EQ(0u, first);
    ASSERT_EQ(7u, count);
    ASSERT_TRUE(objects_frame_range(&reg, 24, 10, &first, &count));
    ASSERT_EQ(3u, first);
    ASSERT_EQ(1u, count);
}

UTEST(objects, frame_range_without_index_or_phases_is_one_frame_at_0)
{
    static MakeReg m;
    Reg reg;
    uint32_t first = 99;
    uint32_t count = 99;
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "ObjectCount", 1);
    make_reg_section(&m, "Object0");
    make_reg_int(&m, "ID", 4);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));

    ASSERT_TRUE(objects_frame_range(&reg, 4, 5, &first, &count));
    ASSERT_EQ(0u, first);
    ASSERT_EQ(1u, count);
}

UTEST(objects, frame_range_gives_no_more_frames_than_the_file_has)
{
    static MakeReg m;
    Reg reg;
    uint32_t first = 99;
    uint32_t count = 99;
    ASSERT_TRUE(reg_open(m.bytes, make_objects(&m), &reg));

    /* The dead pine says 7 and its file has 1. */
    ASSERT_TRUE(objects_frame_range(&reg, 2, 1, &first, &count));
    ASSERT_EQ(0u, first);
    ASSERT_EQ(1u, count);
    /* The rock at frame 3 of a file of 4 keeps its one frame. */
    ASSERT_TRUE(objects_frame_range(&reg, 24, 4, &first, &count));
    ASSERT_EQ(3u, first);
    ASSERT_EQ(1u, count);
}

UTEST(objects, frame_range_refuses_a_first_frame_outside_the_file)
{
    static MakeReg m;
    Reg reg;
    uint32_t first = 99;
    uint32_t count = 99;
    ASSERT_TRUE(reg_open(m.bytes, make_objects(&m), &reg));

    ASSERT_FALSE(objects_frame_range(&reg, 24, 3, &first, &count));
    ASSERT_FALSE(objects_frame_range(&reg, 0, 0, &first, &count));
    /* And an object that is not there. */
    ASSERT_FALSE(objects_frame_range(&reg, 77, 10, &first, &count));
    ASSERT_EQ(99u, first);
    ASSERT_EQ(99u, count);
}

UTEST(objects, frame_range_refuses_negative_numbers)
{
    static MakeReg m;
    Reg reg;
    uint32_t first = 99;
    uint32_t count = 99;
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "ObjectCount", 2);
    make_reg_section(&m, "Object0");
    make_reg_int(&m, "ID", 1);
    make_reg_int(&m, "Index", -1);
    make_reg_section(&m, "Object1");
    make_reg_int(&m, "ID", 2);
    make_reg_int(&m, "Phases", -3);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));

    ASSERT_FALSE(objects_frame_range(&reg, 1, 10, &first, &count));
    ASSERT_FALSE(objects_frame_range(&reg, 2, 10, &first, &count));
    ASSERT_EQ(99u, first);
}

/* ======== object_sheet ======== */

/* The pixel at `x`, `y` inside cell `column` of a sheet of one row. */
static uint8_t cell_at(const Sheet *s, uint32_t column, uint32_t x,
    uint32_t y)
{
    return s->pixels[(size_t)y * sheet_width(s)
        + (size_t)column * s->cell_width + x];
}

UTEST(object_sheet, build_puts_the_objects_frames_in_one_row)
{
    /* Five frames of 2 by 2, each filled with its number from 1 up. The
     * object is frames 1, 2 and 3. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 5; i++) {
        make_sprite_solid(&m, 2, 2, (uint8_t)(i + 1));
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ObjectSheet o;

    ASSERT_TRUE(object_sheet_build(&sprite, NULL, 1, 3, 8, 6, 4, 5, &o));

    ASSERT_EQ(3u, o.sheet.columns);
    ASSERT_EQ(1u, o.sheet.rows);
    ASSERT_EQ(2u, o.sheet.cell_width);
    ASSERT_EQ(2u, o.sheet.cell_height);
    /* The frames sit at 3, 2 in the box, and so does the cell. */
    ASSERT_EQ(1, o.anchor_x);
    ASSERT_EQ(3, o.anchor_y);
    ASSERT_EQ(2, cell_at(&o.sheet, 0, 0, 0));
    ASSERT_EQ(3, cell_at(&o.sheet, 1, 1, 1));
    ASSERT_EQ(4, cell_at(&o.sheet, 2, 0, 1));
    ASSERT_TRUE(o.edges.pixels == NULL);
    object_sheet_free(&o);
    ASSERT_TRUE(o.sheet.pixels == NULL);
}

UTEST(object_sheet, build_keeps_frames_of_different_sizes_in_place)
{
    /* A frame of 6 by 2 and one of 2 by 4 in a box of 8 by 8: the cell
     * is 6 wide and 4 high, from 1, 2. */
    static MakeSprite m;
    make_sprite_start(&m);
    make_sprite_solid(&m, 6, 2, 9);
    make_sprite_solid(&m, 2, 4, 5);
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ObjectSheet o;

    ASSERT_TRUE(object_sheet_build(&sprite, NULL, 0, 2, 8, 8, 4, 6, &o));

    ASSERT_EQ(6u, o.sheet.cell_width);
    ASSERT_EQ(4u, o.sheet.cell_height);
    ASSERT_EQ(3, o.anchor_x);
    ASSERT_EQ(4, o.anchor_y);
    ASSERT_EQ(0, cell_at(&o.sheet, 0, 0, 0));
    ASSERT_EQ(9, cell_at(&o.sheet, 0, 0, 1));
    ASSERT_EQ(9, cell_at(&o.sheet, 0, 5, 2));
    ASSERT_EQ(0, cell_at(&o.sheet, 1, 1, 0));
    ASSERT_EQ(5, cell_at(&o.sheet, 1, 2, 0));
    ASSERT_EQ(5, cell_at(&o.sheet, 1, 3, 3));
    object_sheet_free(&o);
}

UTEST(object_sheet, build_is_sized_by_the_objects_own_frames_only)
{
    /* The file is shared: frame 0 belongs to another object and is
     * larger than this object's box. Frame 1 is this object's. */
    static MakeSprite m;
    make_sprite_start(&m);
    make_sprite_solid(&m, 8, 6, 1);
    make_sprite_solid(&m, 2, 2, 7);
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ObjectSheet o;

    ASSERT_TRUE(object_sheet_build(&sprite, NULL, 1, 1, 4, 4, 2, 2, &o));

    ASSERT_EQ(2u, o.sheet.cell_width);
    ASSERT_EQ(2u, o.sheet.cell_height);
    ASSERT_EQ(7, cell_at(&o.sheet, 0, 0, 0));
    object_sheet_free(&o);

    /* Asking for the large frame with that box is refused. */
    memset(&o, 0, sizeof o);
    ASSERT_FALSE(object_sheet_build(&sprite, NULL, 0, 1, 4, 4, 2, 2, &o));
    ASSERT_TRUE(o.sheet.pixels == NULL);
}

UTEST(object_sheet, build_puts_the_edges_on_a_sheet_with_the_same_cells)
{
    /* The object is 2 by 2 and its edge 4 by 4 around it. */
    static MakeSprite m;
    static MakeSprite e;
    make_sprite_start(&m);
    make_sprite_start(&e);
    make_sprite_solid(&m, 2, 2, 5);
    make_sprite_solid(&e, 4, 4, 9);
    Sprite256 sprite;
    Sprite256 edges;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ASSERT_TRUE(make_sprite_finish(&e, &edges));
    ObjectSheet o;

    ASSERT_TRUE(object_sheet_build(&sprite, &edges, 0, 1, 8, 8, 4, 6, &o));

    ASSERT_EQ(4u, o.sheet.cell_width);
    ASSERT_EQ(4u, o.edges.cell_width);
    ASSERT_EQ(4u, o.edges.cell_height);
    ASSERT_EQ(2, o.anchor_x);
    ASSERT_EQ(4, o.anchor_y);
    ASSERT_EQ(0, cell_at(&o.sheet, 0, 0, 0));
    ASSERT_EQ(5, cell_at(&o.sheet, 0, 1, 1));
    ASSERT_EQ(9, cell_at(&o.edges, 0, 0, 0));
    ASSERT_EQ(9, cell_at(&o.edges, 0, 3, 3));
    object_sheet_free(&o);
    ASSERT_TRUE(o.edges.pixels == NULL);
}

UTEST(object_sheet, build_refuses_frames_that_are_not_in_the_file)
{
    static MakeSprite m;
    make_sprite_start(&m);
    for (int i = 0; i < 3; i++) {
        make_sprite_solid(&m, 1, 1, 1);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ObjectSheet o;
    memset(&o, 0, sizeof o);
    o.anchor_x = 12345;

    /* The last frame is fine. One more is not. */
    ASSERT_TRUE(object_sheet_build(&sprite, NULL, 2, 1, 2, 2, 1, 1, &o));
    object_sheet_free(&o);
    o.anchor_x = 12345;
    ASSERT_FALSE(object_sheet_build(&sprite, NULL, 2, 2, 2, 2, 1, 1, &o));
    ASSERT_FALSE(object_sheet_build(&sprite, NULL, 3, 1, 2, 2, 1, 1, &o));
    ASSERT_FALSE(object_sheet_build(&sprite, NULL, 0, 4, 2, 2, 1, 1, &o));
    /* No frames at all. */
    ASSERT_FALSE(object_sheet_build(&sprite, NULL, 0, 0, 2, 2, 1, 1, &o));
    /* A count so large that first + count wraps round. */
    ASSERT_FALSE(object_sheet_build(&sprite, NULL, 2, 0xffffffffu, 2, 2, 1,
        1, &o));
    ASSERT_EQ(12345, o.anchor_x);
    ASSERT_TRUE(o.sheet.pixels == NULL);
}

UTEST(object_sheet, build_refuses_a_box_of_no_size_or_beyond_the_limit)
{
    static MakeSprite m;
    make_sprite_start(&m);
    make_sprite_solid(&m, 1, 1, 1);
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ObjectSheet o;
    memset(&o, 0, sizeof o);

    ASSERT_FALSE(object_sheet_build(&sprite, NULL, 0, 1, 0, 2, 0, 0, &o));
    ASSERT_FALSE(object_sheet_build(&sprite, NULL, 0, 1, 2, 0, 0, 0, &o));
    ASSERT_FALSE(object_sheet_build(&sprite, NULL, 0, 1, 2049, 2, 0, 0, &o));
    ASSERT_FALSE(object_sheet_build(&sprite, NULL, 0, 1, 2, 2049, 0, 0, &o));
    ASSERT_TRUE(o.sheet.pixels == NULL);
}

UTEST(object_sheet, build_refuses_edges_with_another_number_of_frames)
{
    static MakeSprite m;
    static MakeSprite e;
    make_sprite_start(&m);
    make_sprite_start(&e);
    make_sprite_solid(&m, 1, 1, 1);
    make_sprite_solid(&m, 1, 1, 1);
    make_sprite_solid(&e, 1, 1, 1);
    Sprite256 sprite;
    Sprite256 edges;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ASSERT_TRUE(make_sprite_finish(&e, &edges));
    ObjectSheet o;
    memset(&o, 0, sizeof o);

    ASSERT_FALSE(object_sheet_build(&sprite, &edges, 0, 1, 2, 2, 1, 1, &o));
    ASSERT_TRUE(o.sheet.pixels == NULL);
    ASSERT_TRUE(o.edges.pixels == NULL);
}

UTEST(object_sheet, build_of_an_object_whose_frames_are_empty)
{
    static MakeSprite m;
    make_sprite_start(&m);
    make_sprite_solid(&m, 0, 0, 0);
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ObjectSheet o;

    ASSERT_TRUE(object_sheet_build(&sprite, NULL, 0, 1, 8, 8, 4, 4, &o));

    ASSERT_EQ(1u, o.sheet.cell_width);
    ASSERT_EQ(1u, o.sheet.cell_height);
    ASSERT_EQ(0, cell_at(&o.sheet, 0, 0, 0));
    object_sheet_free(&o);
}

/* ======== object_toml ======== */

/* Three objects:
 *
 *     Object0   ID 0   a pine, which states everything it has
 *     Object1   ID 2   the dead pine
 *     Object2   ID 7   a child of the pine with a name of its own
 */
static size_t make_named_objects(MakeReg *m)
{
    const int32_t frames[] = {0, 1, 2};
    const int32_t times[] = {4, 4, 5};

    make_reg_start(m);
    make_reg_section(m, "Global");
    make_reg_int(m, "ObjectCount", 3);

    make_reg_section(m, "Object0");
    make_reg_int(m, "ID", 0);
    make_reg_text(m, "DescText", "Pine 1");
    make_reg_int(m, "InMapEditor", 1);
    make_reg_int(m, "IconID", 187);
    make_reg_int(m, "DeadObject", 2);
    make_reg_int_list(m, "AnimationFrame", frames, 3);
    make_reg_int_list(m, "AnimationTime", times, 3);

    make_reg_section(m, "Object1");
    make_reg_int(m, "ID", 2);
    make_reg_text(m, "DescText", "Pine 1 (dead)");
    make_reg_int(m, "DeadObject", -1);
    make_reg_int(m, "FireObject", -2);

    make_reg_section(m, "Object2");
    make_reg_int(m, "ID", 7);
    make_reg_int(m, "Parent", 0);
    make_reg_text(m, "DescText", "Pine 2");
    return make_reg_finish(m);
}

/* A sheet of `columns` cells of 64 by 112. No pixels: the writer does
 * not look at them. */
static ObjectSheet make_sheet(uint32_t columns)
{
    ObjectSheet s;
    memset(&s, 0, sizeof s);
    s.sheet.cell_width = 64;
    s.sheet.cell_height = 112;
    s.sheet.columns = columns;
    s.sheet.rows = 1;
    s.anchor_x = 32;
    s.anchor_y = 100;
    return s;
}

UTEST(object_toml, folder_name_is_the_id_and_the_name)
{
    static MakeReg m;
    Reg reg;
    char name[NAME_FOLDER_SIZE] = "untouched";
    ASSERT_TRUE(reg_open(m.bytes, make_named_objects(&m), &reg));

    ASSERT_TRUE(object_toml_folder_name(&reg, 0, name));
    ASSERT_STREQ("000_pine_1", name);
    ASSERT_TRUE(object_toml_folder_name(&reg, 2, name));
    ASSERT_STREQ("002_pine_1_dead", name);
    ASSERT_FALSE(object_toml_folder_name(&reg, 5, name));
    ASSERT_STREQ("002_pine_1_dead", name);
}

UTEST(object_toml, write_gives_the_whole_file_of_an_object)
{
    static MakeReg m;
    Reg reg;
    ObjectSheet sheet = make_sheet(3);
    ByteBuffer b = {0};
    ASSERT_TRUE(reg_open(m.bytes, make_named_objects(&m), &reg));

    ASSERT_TRUE(object_toml_write(&b, &reg, 0, &sheet));

    ASSERT_TRUE(holds(&b,
        "# Pine 1\n"
        "id = 0\n"
        "name = \"Pine 1\"\n"
        "in_map_editor = 1\n"
        "icon_id = 187\n"
        "dead_object = 2\n"
        "\n"
        "[sprite]\n"
        "sheet = \"sprites.png\"\n"
        "cell = [64, 112]\n"
        "anchor = [32, 100]\n"
        "columns = 3\n"
        "frames = [0, 1, 2]\n"
        "ticks = [4, 4, 5]\n"));
    buffer_free(&b);
}

UTEST(object_toml, write_of_an_object_that_stands_still)
{
    /* The dead pine: no lists, no dead object of its own, and the
     * registry's mark for a dead tree. */
    static MakeReg m;
    Reg reg;
    ObjectSheet sheet = make_sheet(1);
    ByteBuffer b = {0};
    ASSERT_TRUE(reg_open(m.bytes, make_named_objects(&m), &reg));

    ASSERT_TRUE(object_toml_write(&b, &reg, 2, &sheet));

    ASSERT_TRUE(holds(&b,
        "# Pine 1 (dead)\n"
        "id = 2\n"
        "name = \"Pine 1 (dead)\"\n"
        "fire_object = -2\n"
        "\n"
        "[sprite]\n"
        "sheet = \"sprites.png\"\n"
        "cell = [64, 112]\n"
        "anchor = [32, 100]\n"
        "columns = 1\n"));
    buffer_free(&b);
}

UTEST(object_toml, write_takes_missing_values_from_the_parent)
{
    static MakeReg m;
    Reg reg;
    ObjectSheet sheet = make_sheet(3);
    ByteBuffer b = {0};
    ASSERT_TRUE(reg_open(m.bytes, make_named_objects(&m), &reg));

    ASSERT_TRUE(object_toml_write(&b, &reg, 7, &sheet));

    ASSERT_TRUE(has(&b, "id = 7\nname = \"Pine 2\"\n"));
    ASSERT_TRUE(has(&b, "icon_id = 187\ndead_object = 2\n"));
    ASSERT_TRUE(has(&b, "ticks = [4, 4, 5]\n"));
    ASSERT_FALSE(has(&b, "parent"));
    buffer_free(&b);
}

UTEST(object_toml, write_names_the_edges_of_a_sheet_that_has_them)
{
    static MakeReg m;
    static uint8_t pixels[1];
    Reg reg;
    ObjectSheet sheet = make_sheet(3);
    sheet.edges.pixels = pixels;
    ByteBuffer b = {0};
    ASSERT_TRUE(reg_open(m.bytes, make_named_objects(&m), &reg));

    ASSERT_TRUE(object_toml_write(&b, &reg, 0, &sheet));

    ASSERT_TRUE(has(&b, "sheet = \"sprites.png\"\nedges = \"edges.png\"\n"
        "cell = [64, 112]\n"));
    buffer_free(&b);
}

UTEST(object_toml, write_refuses_a_frame_that_is_not_on_the_sheet)
{
    /* The pine's list shows cells 0, 1 and 2. A sheet of 2 has no 2. */
    static MakeReg m;
    Reg reg;
    ObjectSheet sheet = make_sheet(2);
    ByteBuffer b = {0};
    ASSERT_TRUE(reg_open(m.bytes, make_named_objects(&m), &reg));

    ASSERT_FALSE(object_toml_write(&b, &reg, 0, &sheet));
    buffer_free(&b);
}

UTEST(object_toml, write_refuses_lists_of_different_length_or_bad_frames)
{
    const int32_t frames[] = {0, 1, 2};
    const int32_t times[] = {4, 4};
    const int32_t negative[] = {0, -1};
    static MakeReg m;
    Reg reg;
    ObjectSheet sheet = make_sheet(3);
    ByteBuffer b = {0};
    make_reg_start(&m);
    make_reg_section(&m, "Global");
    make_reg_int(&m, "ObjectCount", 2);
    make_reg_section(&m, "Object0");
    make_reg_int(&m, "ID", 0);
    make_reg_int_list(&m, "AnimationFrame", frames, 3);
    make_reg_int_list(&m, "AnimationTime", times, 2);
    make_reg_section(&m, "Object1");
    make_reg_int(&m, "ID", 1);
    make_reg_int_list(&m, "AnimationFrame", negative, 2);
    make_reg_int_list(&m, "AnimationTime", times, 2);
    ASSERT_TRUE(reg_open(m.bytes, make_reg_finish(&m), &reg));

    ASSERT_FALSE(object_toml_write(&b, &reg, 0, &sheet));
    ASSERT_FALSE(object_toml_write(&b, &reg, 1, &sheet));
    /* And an object that is not there. */
    ASSERT_FALSE(object_toml_write(&b, &reg, 9, &sheet));
    buffer_free(&b);
}
