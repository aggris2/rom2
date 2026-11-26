#include "import/spell_toml.h"

#include <string.h>

#include "make_databin.h"
#include "utest.h"

/* A data.bin with three spells, the second an empty row. The columns
 * are a few of the real ones, one that ROM2 does not know, and the
 * text. */
static void make_spells(ByteBuffer *b)
{
    const char *const columns[] = {
        "Complication Level", "Mana Cost", "Something New",
        "\"Radius, Length/2\"", "damageMin", "Effects"
    };
    const char *const empty[] = {"a"};
    const int32_t arrow[] = {3, 5, 99, 0, 7};
    const int32_t haste[] = {10, 12, 99, 2, -1};

    put_empty_first_tables(b);
    put_header(b, empty, 1);  /* monsters */
    put_i32(b, 1);
    put_header(b, empty, 1);  /* humans */
    put_i32(b, 1);
    put_header(b, empty, 1);  /* buildings */
    put_i32(b, 1);
    put_header(b, columns, 6);
    put_i32(b, 5);
    put_row(b, "Fire Arrow", arrow, 5);
    put_text(b, "");
    put_row(b, "", NULL, 0);
    put_u16(b, 0);
    put_row(b, "Haste", haste, 5);
    put_text(b, "speed=+2: duration 30");
}

static bool holds(const ByteBuffer *b, const char *expected)
{
    size_t length = strlen(expected);
    return b->size == length && memcmp(b->data, expected, length) == 0;
}

/* Reads row `number`, counted from 1, of the spells. */
static bool read_row(const DataBin *bin, int number, DataBinRow *row)
{
    DataBinRows rows = databin_rows(bin, DATABIN_SPELLS);
    for (int i = 0; i < number; i++) {
        if (!databin_next(&rows, row)) {
            return false;
        }
    }
    return true;
}

UTEST(spell_toml, folder_name_is_the_number_and_the_name)
{
    static DataBinRow row;
    ByteBuffer file = {0};
    DataBin bin;
    char name[NAME_FOLDER_SIZE] = "untouched";
    make_spells(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));
    ASSERT_TRUE(read_row(&bin, 1, &row));

    ASSERT_TRUE(spell_toml_folder_name(1, &row, name));
    ASSERT_STREQ("001_fire_arrow", name);
    ASSERT_FALSE(spell_toml_folder_name(1000, &row, name));
    ASSERT_STREQ("001_fire_arrow", name);
    buffer_free(&file);
}

UTEST(spell_toml, write_gives_the_numbers_under_their_names)
{
    static DataBinRow row;
    ByteBuffer file = {0};
    ByteBuffer b = {0};
    DataBin bin;
    make_spells(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));
    ASSERT_TRUE(read_row(&bin, 1, &row));

    ASSERT_TRUE(spell_toml_write(&b, &bin, 1, &row));

    /* The column ROM2 does not know is left out, and so is the text of
     * a spell that has none. */
    ASSERT_TRUE(holds(&b,
        "# Fire Arrow\n"
        "id = 1\n"
        "name = \"Fire Arrow\"\n"
        "level = 3\n"
        "mana_cost = 5\n"
        "radius = 0\n"
        "damage_min = 7\n"));
    buffer_free(&b);
    buffer_free(&file);
}

UTEST(spell_toml, write_adds_the_effects_of_a_spell_that_has_them)
{
    static DataBinRow row;
    ByteBuffer file = {0};
    ByteBuffer b = {0};
    DataBin bin;
    make_spells(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));
    ASSERT_TRUE(read_row(&bin, 3, &row));

    ASSERT_TRUE(spell_toml_write(&b, &bin, 3, &row));

    ASSERT_TRUE(holds(&b,
        "# Haste\n"
        "id = 3\n"
        "name = \"Haste\"\n"
        "level = 10\n"
        "mana_cost = 12\n"
        "radius = 2\n"
        "damage_min = -1\n"
        "effects = \"speed=+2: duration 30\"\n"));
    buffer_free(&b);
    buffer_free(&file);
}

UTEST(spell_toml, write_of_an_empty_row_has_only_its_number_and_name)
{
    static DataBinRow row;
    ByteBuffer file = {0};
    ByteBuffer b = {0};
    DataBin bin;
    make_spells(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));
    ASSERT_TRUE(read_row(&bin, 2, &row));

    ASSERT_TRUE(spell_toml_write(&b, &bin, 2, &row));

    ASSERT_TRUE(holds(&b, "# \nid = 2\nname = \"\"\n"));
    buffer_free(&b);
    buffer_free(&file);
}
