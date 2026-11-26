#include "import/databin.h"

#include <string.h>

#include "base/buffer.h"
#include "make_databin.h"
#include "utest.h"

/* A data.bin built by hand. Every table is there, with as few columns
 * and rows as its test needs. */

/* Everything up to the monsters: two shapes, one material, one modifier,
 * one piece of armour, no shields, one weapon and two magic items. */
static void put_first_tables(ByteBuffer *b)
{
    const char *const shape_columns[] = {"Price", "Weight"};
    const char *const modifier_columns[] = {"cost", "min"};
    const char *const item_columns[] = {"Shape", "Price"};
    const char *const magic_columns[] = {"Price", "Weight", "Effects"};
    const int32_t two[] = {7, -8};

    put_header(b, shape_columns, 2);
    put_i32(b, 2);
    put_text(b, "Common");
    for (int i = 0; i < 9; i++) {
        put_fraction(b, i % 3);
    }
    put_text(b, "Rare");
    for (int i = 0; i < 9; i++) {
        put_fraction(b, 2);
    }
    put_i32(b, 1);
    put_text(b, "Iron");
    for (int i = 0; i < 9; i++) {
        put_fraction(b, 1);
    }

    put_header(b, modifier_columns, 2);
    put_i32(b, 1);
    put_row(b, "body", two, 2);

    /* The counts from here on are one more than the rows. */
    put_header(b, item_columns, 2);
    put_i32(b, 2);
    put_row(b, "Helm", two, 2);
    for (uint32_t i = 0; i < 8; i++) {
        put_u16(b, 60000 + i);
    }
    put_i32(b, 1);
    put_i32(b, 2);
    put_row(b, "Sword", two, 2);
    for (uint32_t i = 0; i < 8; i++) {
        put_u16(b, i);
    }

    put_header(b, magic_columns, 3);
    put_i32(b, 3);
    put_row(b, "Book", two, 2);
    buffer_push(b, 0);
    put_row(b, "Potion", two, 2);
    buffer_push(b, 1);
    put_text(b, "heal=5");
}

/* Three monsters, the second empty; two humans, the first empty; one
 * building; three spells, the second empty. */
static void put_last_tables(ByteBuffer *b)
{
    const char *const monster_columns[] = {"healthMax", "typeID", "face"};
    const char *const human_columns[] = {"Body", "typeID"};
    const char *const building_columns[] = {"sizeX", "sizeY"};
    const char *const spell_columns[] = {"Mana Cost", "Effects"};
    const int32_t goblin[] = {40, 64, 1};
    const int32_t goblin2[] = {80, 64, 2};
    const int32_t peasant[] = {12, 1};
    const int32_t hut[] = {3, 2};
    const int32_t arrow[] = {3};

    put_header(b, monster_columns, 3);
    put_i32(b, 4);
    put_row(b, "Goblin", goblin, 3);
    put_text(b, "Bronze Pike");
    put_text(b, "");
    put_row(b, "", NULL, 0);
    put_text(b, "");
    put_text(b, "");
    put_row(b, "Goblin.2", goblin2, 3);
    put_text(b, "Iron Pike");
    put_text(b, "Wooden Shield");

    put_header(b, human_columns, 2);
    put_i32(b, 3);
    put_row(b, "", NULL, 0);
    for (int i = 0; i < 10; i++) {
        put_text(b, "");
    }
    put_row(b, "Peasant", peasant, 2);
    put_text(b, "Wooden Club");
    for (int i = 1; i < 10; i++) {
        put_text(b, "");
    }

    put_header(b, building_columns, 2);
    put_i32(b, 2);
    put_row(b, "Hut", hut, 2);

    put_header(b, spell_columns, 2);
    put_i32(b, 5);
    put_row(b, "Fire Arrow", arrow, 1);
    put_text(b, "");
    put_row(b, "", NULL, 0);
    put_u16(b, 0);
    put_row(b, "Heal", arrow, 1);
    put_text(b, "heal=10");
}

static void make_databin(ByteBuffer *b)
{
    put_first_tables(b);
    put_last_tables(b);
}

/* ---- databin_open ---- */

UTEST(databin, open_finds_the_number_of_rows_of_every_table)
{
    ByteBuffer b = {0};
    DataBin bin;
    make_databin(&b);

    ASSERT_TRUE(databin_open(b.data, b.size, &bin));

    ASSERT_EQ(2u, bin.tables[DATABIN_SHAPES].row_count);
    ASSERT_EQ(1u, bin.tables[DATABIN_MATERIALS].row_count);
    ASSERT_EQ(1u, bin.tables[DATABIN_MODIFIERS].row_count);
    ASSERT_EQ(1u, bin.tables[DATABIN_ARMOUR].row_count);
    ASSERT_EQ(0u, bin.tables[DATABIN_SHIELDS].row_count);
    ASSERT_EQ(1u, bin.tables[DATABIN_WEAPONS].row_count);
    ASSERT_EQ(2u, bin.tables[DATABIN_MAGIC_ITEMS].row_count);
    ASSERT_EQ(3u, bin.tables[DATABIN_MONSTERS].row_count);
    ASSERT_EQ(2u, bin.tables[DATABIN_HUMANS].row_count);
    ASSERT_EQ(1u, bin.tables[DATABIN_BUILDINGS].row_count);
    ASSERT_EQ(3u, bin.tables[DATABIN_SPELLS].row_count);
    buffer_free(&b);
}

UTEST(databin, open_refuses_a_file_cut_short_anywhere)
{
    ByteBuffer b = {0};
    DataBin bin;
    make_databin(&b);
    memset(&bin, 0x55, sizeof bin);

    for (size_t size = 0; size < b.size; size++) {
        ASSERT_FALSE(databin_open(b.data, size, &bin));
    }
    ASSERT_EQ(0x55555555u, bin.tables[0].row_count);
    buffer_free(&b);
}

UTEST(databin, open_refuses_a_count_smaller_than_its_extra)
{
    /* The armour count is one more than its rows, so it cannot be 0. */
    const char *const columns[] = {"a"};
    ByteBuffer b = {0};
    DataBin bin;
    put_header(&b, columns, 1);
    put_i32(&b, 0);
    put_i32(&b, 0);
    put_header(&b, columns, 1);
    put_i32(&b, 0);
    put_header(&b, columns, 1);
    put_i32(&b, 0);
    for (int i = 0; i < 64; i++) {
        buffer_push(&b, 0);
    }

    ASSERT_FALSE(databin_open(b.data, b.size, &bin));
    buffer_free(&b);
}

/* A whole file whose only row is a modifier with `count` numbers. */
static void make_one_modifier(ByteBuffer *b, uint32_t count)
{
    const char *const columns[] = {"a"};
    put_header(b, columns, 1);  /* shapes and materials */
    put_i32(b, 0);
    put_i32(b, 0);
    put_header(b, columns, 1);  /* modifiers */
    put_i32(b, 1);
    put_text(b, "big");
    put_u16(b, count);
    for (uint32_t i = 0; i < count; i++) {
        put_i32(b, (int32_t)i);
    }
    put_header(b, columns, 1);  /* armour, shields and weapons */
    put_i32(b, 1);
    put_i32(b, 1);
    put_i32(b, 1);
    put_header(b, columns, 1);  /* magic items */
    put_i32(b, 1);
    put_header(b, columns, 1);  /* monsters */
    put_i32(b, 1);
    put_header(b, columns, 1);  /* humans */
    put_i32(b, 1);
    put_empty_last_tables(b);
}

UTEST(databin, open_accepts_a_row_with_the_most_numbers_allowed)
{
    static DataBinRow row;
    ByteBuffer b = {0};
    DataBin bin;
    make_one_modifier(&b, DATABIN_MAX_NUMBERS);

    ASSERT_TRUE(databin_open(b.data, b.size, &bin));
    DataBinRows rows = databin_rows(&bin, DATABIN_MODIFIERS);
    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_EQ((uint32_t)DATABIN_MAX_NUMBERS, row.number_count);
    ASSERT_EQ(DATABIN_MAX_NUMBERS - 1, row.numbers[DATABIN_MAX_NUMBERS - 1]);
    buffer_free(&b);
}

UTEST(databin, open_refuses_a_row_with_too_many_numbers)
{
    ByteBuffer b = {0};
    DataBin bin;
    make_one_modifier(&b, DATABIN_MAX_NUMBERS + 1);

    ASSERT_FALSE(databin_open(b.data, b.size, &bin));
    buffer_free(&b);
}

UTEST(databin, open_refuses_a_count_larger_than_the_file)
{
    const char *const columns[] = {"a"};
    ByteBuffer b = {0};
    DataBin bin;
    put_header(&b, columns, 1);
    put_i32(&b, 0x7fffffff);

    ASSERT_FALSE(databin_open(b.data, b.size, &bin));
    buffer_free(&b);
}

/* ---- databin_rows, databin_next ---- */

UTEST(databin, next_reads_the_fractions_of_shapes_and_materials)
{
    static DataBinRow row;
    ByteBuffer b = {0};
    DataBin bin;
    make_databin(&b);
    ASSERT_TRUE(databin_open(b.data, b.size, &bin));

    DataBinRows rows = databin_rows(&bin, DATABIN_SHAPES);
    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_STREQ("Common", row.name);
    ASSERT_EQ(9u, row.fraction_count);
    ASSERT_EQ(0u, row.number_count);
    ASSERT_TRUE(row.fractions[0] == 0.0);
    ASSERT_TRUE(row.fractions[1] == 1.0);
    ASSERT_TRUE(row.fractions[2] == 2.0);
    ASSERT_TRUE(row.fractions[8] == 2.0);
    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_STREQ("Rare", row.name);
    ASSERT_FALSE(databin_next(&rows, &row));

    rows = databin_rows(&bin, DATABIN_MATERIALS);
    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_STREQ("Iron", row.name);
    ASSERT_TRUE(row.fractions[4] == 1.0);
    ASSERT_FALSE(databin_next(&rows, &row));
    buffer_free(&b);
}

UTEST(databin, next_reads_the_numbers_of_a_row)
{
    static DataBinRow row;
    ByteBuffer b = {0};
    DataBin bin;
    make_databin(&b);
    ASSERT_TRUE(databin_open(b.data, b.size, &bin));

    DataBinRows rows = databin_rows(&bin, DATABIN_MODIFIERS);
    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_STREQ("body", row.name);
    ASSERT_EQ(2u, row.number_count);
    ASSERT_EQ(7, row.numbers[0]);
    ASSERT_EQ(-8, row.numbers[1]);
    ASSERT_EQ(0u, row.text_count);
    ASSERT_EQ(0u, row.fraction_count);

    rows = databin_rows(&bin, DATABIN_BUILDINGS);
    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_STREQ("Hut", row.name);
    ASSERT_EQ(3, row.numbers[0]);
    ASSERT_FALSE(databin_next(&rows, &row));
    buffer_free(&b);
}

UTEST(databin, next_puts_the_short_numbers_of_an_item_after_the_others)
{
    static DataBinRow row;
    ByteBuffer b = {0};
    DataBin bin;
    make_databin(&b);
    ASSERT_TRUE(databin_open(b.data, b.size, &bin));

    DataBinRows rows = databin_rows(&bin, DATABIN_ARMOUR);
    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_STREQ("Helm", row.name);
    ASSERT_EQ(10u, row.number_count);
    ASSERT_EQ(-8, row.numbers[1]);
    /* A short number has no sign: 60000 stays 60000. */
    ASSERT_EQ(60000, row.numbers[2]);
    ASSERT_EQ(60007, row.numbers[9]);

    rows = databin_rows(&bin, DATABIN_SHIELDS);
    ASSERT_FALSE(databin_next(&rows, &row));

    rows = databin_rows(&bin, DATABIN_WEAPONS);
    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_STREQ("Sword", row.name);
    ASSERT_EQ(7, row.numbers[9]);
    buffer_free(&b);
}

UTEST(databin, next_reads_the_text_of_a_magic_item_only_if_it_has_one)
{
    static DataBinRow row;
    ByteBuffer b = {0};
    DataBin bin;
    make_databin(&b);
    ASSERT_TRUE(databin_open(b.data, b.size, &bin));

    DataBinRows rows = databin_rows(&bin, DATABIN_MAGIC_ITEMS);
    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_STREQ("Book", row.name);
    ASSERT_EQ(0u, row.text_count);
    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_STREQ("Potion", row.name);
    ASSERT_EQ(1u, row.text_count);
    ASSERT_STREQ("heal=5", row.texts[0]);
    ASSERT_FALSE(databin_next(&rows, &row));
    buffer_free(&b);
}

UTEST(databin, next_reads_monsters_with_their_items)
{
    static DataBinRow row;
    ByteBuffer b = {0};
    DataBin bin;
    make_databin(&b);
    ASSERT_TRUE(databin_open(b.data, b.size, &bin));

    DataBinRows rows = databin_rows(&bin, DATABIN_MONSTERS);
    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_STREQ("Goblin", row.name);
    ASSERT_EQ(3u, row.number_count);
    ASSERT_EQ(40, row.numbers[0]);
    ASSERT_EQ(2u, row.text_count);
    ASSERT_STREQ("Bronze Pike", row.texts[0]);
    ASSERT_STREQ("", row.texts[1]);

    /* The empty row: no numbers and no texts. */
    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_STREQ("", row.name);
    ASSERT_EQ(0u, row.number_count);
    ASSERT_EQ(0u, row.text_count);

    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_STREQ("Goblin.2", row.name);
    ASSERT_EQ(80, row.numbers[0]);
    ASSERT_STREQ("Wooden Shield", row.texts[1]);
    ASSERT_FALSE(databin_next(&rows, &row));
    /* And it stays at the end. */
    ASSERT_FALSE(databin_next(&rows, &row));
    ASSERT_STREQ("Goblin.2", row.name);
    buffer_free(&b);
}

UTEST(databin, next_reads_humans_with_their_ten_items)
{
    static DataBinRow row;
    ByteBuffer b = {0};
    DataBin bin;
    make_databin(&b);
    ASSERT_TRUE(databin_open(b.data, b.size, &bin));

    DataBinRows rows = databin_rows(&bin, DATABIN_HUMANS);
    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_EQ(0u, row.number_count);
    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_STREQ("Peasant", row.name);
    ASSERT_EQ(12, row.numbers[0]);
    ASSERT_EQ(10u, row.text_count);
    ASSERT_STREQ("Wooden Club", row.texts[0]);
    ASSERT_STREQ("", row.texts[9]);
    ASSERT_FALSE(databin_next(&rows, &row));
    buffer_free(&b);
}

UTEST(databin, next_reads_spells_and_steps_over_an_empty_one)
{
    static DataBinRow row;
    ByteBuffer b = {0};
    DataBin bin;
    make_databin(&b);
    ASSERT_TRUE(databin_open(b.data, b.size, &bin));

    DataBinRows rows = databin_rows(&bin, DATABIN_SPELLS);
    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_STREQ("Fire Arrow", row.name);
    ASSERT_EQ(1u, row.text_count);
    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_EQ(0u, row.number_count);
    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_STREQ("Heal", row.name);
    ASSERT_STREQ("heal=10", row.texts[0]);
    ASSERT_FALSE(databin_next(&rows, &row));
    /* The spells are the last table: nothing is left after them. */
    ASSERT_EQ(0u, reader_remaining(&rows.reader));
    buffer_free(&b);
}

/* ---- databin_column ---- */

UTEST(databin, column_gives_the_place_of_a_number_by_its_name)
{
    ByteBuffer b = {0};
    DataBin bin;
    uint32_t index = 99;
    make_databin(&b);
    ASSERT_TRUE(databin_open(b.data, b.size, &bin));

    ASSERT_TRUE(databin_column(&bin, DATABIN_MONSTERS, "healthMax", &index));
    ASSERT_EQ(0u, index);
    ASSERT_TRUE(databin_column(&bin, DATABIN_MONSTERS, "face", &index));
    ASSERT_EQ(2u, index);
    ASSERT_TRUE(databin_column(&bin, DATABIN_HUMANS, "typeID", &index));
    ASSERT_EQ(1u, index);
    buffer_free(&b);
}

UTEST(databin, column_of_a_table_without_names_uses_the_table_before)
{
    ByteBuffer b = {0};
    DataBin bin;
    uint32_t index = 99;
    make_databin(&b);
    ASSERT_TRUE(databin_open(b.data, b.size, &bin));

    ASSERT_TRUE(databin_column(&bin, DATABIN_MATERIALS, "Weight", &index));
    ASSERT_EQ(1u, index);
    ASSERT_TRUE(databin_column(&bin, DATABIN_WEAPONS, "Price", &index));
    ASSERT_EQ(1u, index);
    buffer_free(&b);
}

UTEST(databin, column_refuses_a_name_the_table_does_not_have)
{
    ByteBuffer b = {0};
    DataBin bin;
    uint32_t index = 99;
    make_databin(&b);
    ASSERT_TRUE(databin_open(b.data, b.size, &bin));

    ASSERT_FALSE(databin_column(&bin, DATABIN_MONSTERS, "Body", &index));
    ASSERT_FALSE(databin_column(&bin, DATABIN_MONSTERS, "FACE", &index));
    ASSERT_FALSE(databin_column(&bin, DATABIN_MONSTERS, "", &index));
    /* The first column is the row's name, not a number. */
    ASSERT_FALSE(databin_column(&bin, DATABIN_MONSTERS, "Name", &index));
    ASSERT_EQ(99u, index);
    buffer_free(&b);
}
