#include "import/structure_toml.h"

#include <string.h>

#include "make_databin.h"
#include "make_reg.h"
#include "utest.h"

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
