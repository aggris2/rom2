#include "import/object_toml.h"

#include <string.h>

#include "make_reg.h"
#include "utest.h"

/* Three objects:
 *
 *     Object0   ID 0   a pine, which states everything it has
 *     Object1   ID 2   the dead pine
 *     Object2   ID 7   a child of the pine with a name of its own
 */
static size_t make_objects(MakeReg *m)
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

static bool holds(const ByteBuffer *b, const char *expected)
{
    size_t length = strlen(expected);
    return b->size == length && memcmp(b->data, expected, length) == 0;
}

static bool has(ByteBuffer *b, const char *part)
{
    if (!buffer_push(b, 0)) {
        return false;
    }
    b->size--;
    return strstr((const char *)b->data, part) != NULL;
}

UTEST(object_toml, folder_name_is_the_id_and_the_name)
{
    static MakeReg m;
    Reg reg;
    char name[NAME_FOLDER_SIZE] = "untouched";
    ASSERT_TRUE(reg_open(m.bytes, make_objects(&m), &reg));

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
    ASSERT_TRUE(reg_open(m.bytes, make_objects(&m), &reg));

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
    ASSERT_TRUE(reg_open(m.bytes, make_objects(&m), &reg));

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
    ASSERT_TRUE(reg_open(m.bytes, make_objects(&m), &reg));

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
    ASSERT_TRUE(reg_open(m.bytes, make_objects(&m), &reg));

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
    ASSERT_TRUE(reg_open(m.bytes, make_objects(&m), &reg));

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
