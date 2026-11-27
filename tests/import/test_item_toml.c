#include "import/item_toml.h"

#include <string.h>

#include "import/items.h"
#include "make_databin.h"
#include "utest.h"

/* A data.bin with two classes, two materials, two modifiers, one shape
 * in each of the three tables, and two magic items. */
static void make_items(ByteBuffer *b)
{
    const char *const factor_columns[] = {"Price"};
    const char *const modifier_columns[] = {"cost.MP"};
    const char *const shape_columns[] = {
        "Price", "weight", "Slot", "@.physicalMin", "Mystery"
    };
    const char *const magic_columns[] = {"Price", "weight", "Effects"};
    const char *const empty[] = {"a"};
    const int32_t dagger[] = {100, 5, 1, 18, 7};
    const int32_t buckler[] = {12, 10, 2, 0, 7};
    const int32_t ring[] = {50, 3, 4, 0, 7};
    const int32_t book[] = {1, 10};
    const int32_t scroll[] = {100, 1};
    int32_t modifier[28];
    for (int i = 0; i < 28; i++) {
        modifier[i] = i + 1;
    }

    /* Classes, which the file calls shapes, and materials: 9 fractions
     * each, the first two 0. */
    put_header(b, factor_columns, 1);
    put_i32(b, 2);
    put_text(b, "Common");
    for (int i = 0; i < 9; i++) {
        put_fraction(b, i < 2 ? 0 : 1);
    }
    put_text(b, "Rare");
    for (int i = 0; i < 9; i++) {
        put_fraction(b, i < 2 ? 0 : 2);
    }
    put_i32(b, 2);
    put_text(b, "Iron");
    for (int i = 0; i < 9; i++) {
        put_fraction(b, i < 2 ? 0 : 1);
    }
    put_text(b, "Gold");
    for (int i = 0; i < 9; i++) {
        put_fraction(b, i == 4 ? 2 : (i < 2 ? 0 : 1));
    }

    put_header(b, modifier_columns, 1);
    put_i32(b, 2);
    put_row(b, "", NULL, 0);
    put_row(b, "body", modifier, 28);

    /* Armour, shields, weapons: the counts are one more than the rows. */
    put_header(b, shape_columns, 5);
    put_i32(b, 2);
    put_row(b, "Ring", ring, 5);
    for (int i = 0; i < 8; i++) {
        put_u16(b, 0);
    }
    put_i32(b, 2);
    put_row(b, "Buckler", buckler, 5);
    for (int i = 0; i < 8; i++) {
        put_u16(b, i == 0 ? 3 : 0);
    }
    put_i32(b, 2);
    put_row(b, "Dagger", dagger, 5);
    for (int i = 0; i < 8; i++) {
        put_u16(b, i == 0 ? 3 : (i == 1 ? 2 : 0));
    }

    put_header(b, magic_columns, 3);
    put_i32(b, 3);
    put_row(b, "Book Air", book, 2);
    buffer_push(b, 0);
    put_row(b, "Scroll Fire Arrow", scroll, 2);
    buffer_push(b, 1);
    put_text(b, "castSpell=Fire_Arrow:50");

    put_header(b, empty, 1);  /* monsters */
    put_i32(b, 1);
    put_header(b, empty, 1);  /* humans */
    put_i32(b, 1);
    put_empty_last_tables(b);
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

/* Reads the first row of a table. */
static bool first_row(const DataBin *bin, DataBinTable table,
    DataBinRow *row)
{
    DataBinRows rows = databin_rows(bin, table);
    return databin_next(&rows, row);
}

UTEST(item_toml, write_shape_gives_the_numbers_and_the_variants)
{
    static DataBinRow row;
    ByteBuffer file = {0};
    ByteBuffer b = {0};
    DataBin bin;
    make_items(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));
    ASSERT_TRUE(first_row(&bin, DATABIN_WEAPONS, &row));

    ASSERT_TRUE(item_toml_write_shape(&b, &bin, DATABIN_WEAPONS, &row, 2,
        0, 0, NULL));

    /* The column ROM2 does not know is left out. Class 0 in materials 0
     * and 1, class 1 in material 1. */
    ASSERT_TRUE(holds(&b,
        "# Dagger\n"
        "id = 2\n"
        "name = \"Dagger\"\n"
        "kind = \"weapon\"\n"
        "price = 100\n"
        "weight = 5\n"
        "slot = 1\n"
        "damage_min = 18\n"
        "\n[[variant]]\n"
        "folder = \"iron_common\"\n"
        "material = 0\n"
        "class = 0\n"
        "\n[[variant]]\n"
        "folder = \"gold_common\"\n"
        "material = 1\n"
        "class = 0\n"
        "\n[[variant]]\n"
        "folder = \"gold_rare\"\n"
        "material = 1\n"
        "class = 1\n"));
    buffer_free(&b);
    buffer_free(&file);
}

UTEST(item_toml, write_shape_names_the_kind_by_its_table)
{
    static DataBinRow row;
    ByteBuffer file = {0};
    ByteBuffer b = {0};
    DataBin bin;
    make_items(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));

    ASSERT_TRUE(first_row(&bin, DATABIN_SHIELDS, &row));
    ASSERT_TRUE(item_toml_write_shape(&b, &bin, DATABIN_SHIELDS, &row, 1,
        0, 0, NULL));
    ASSERT_TRUE(has(&b, "name = \"Buckler\"\nkind = \"shield\"\n"));
    /* The variants are listed even when no picture is asked for. */
    ASSERT_TRUE(has(&b, "\n[[variant]]\nfolder = \"iron_common\"\n"
        "material = 0\nclass = 0\n"));
    ASSERT_TRUE(has(&b, "\n[[variant]]\nfolder = \"gold_common\"\n"));
    ASSERT_FALSE(has(&b, "[icon]"));
    b.size = 0;

    /* A shape without variants has none listed, and no pictures even
     * when asked for them. */
    ASSERT_TRUE(first_row(&bin, DATABIN_ARMOUR, &row));
    ASSERT_TRUE(item_toml_write_shape(&b, &bin, DATABIN_ARMOUR, &row, 1,
        80, 80, NULL));
    ASSERT_TRUE(has(&b, "name = \"Ring\"\nkind = \"armour\"\n"));
    ASSERT_FALSE(has(&b, "variant"));
    ASSERT_FALSE(has(&b, "icon"));
    b.size = 0;

    /* Any other table is not a table of shapes. */
    ASSERT_FALSE(item_toml_write_shape(&b, &bin, DATABIN_SPELLS, &row, 1,
        0, 0, NULL));
    ASSERT_EQ(0u, b.size);
    buffer_free(&b);
    buffer_free(&file);
}

UTEST(item_toml, write_shape_names_the_folder_of_each_variant)
{
    static DataBinRow row;
    ByteBuffer file = {0};
    ByteBuffer b = {0};
    DataBin bin;
    make_items(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));
    ASSERT_TRUE(first_row(&bin, DATABIN_SHIELDS, &row));

    ASSERT_TRUE(item_toml_write_shape(&b, &bin, DATABIN_SHIELDS, &row, 1,
        80, 64, NULL));

    /* The buckler is common, in iron and in gold. */
    /* The pictures' files first, then the variants. */
    ASSERT_TRUE(has(&b,
        "\n"
        "[icon]\n"
        "sheet = \"icon.png\"\n"
        "alpha = \"icon_alpha.png\"\n"
        "size = [80, 64]\n"
        "\n[[variant]]\n"
        "folder = \"iron_common\"\n"
        "material = 0\n"
        "class = 0\n"
        "\n[[variant]]\n"
        "folder = \"gold_common\"\n"
        "material = 1\n"
        "class = 0\n"));
    buffer_free(&b);
    buffer_free(&file);
}

UTEST(item_toml, write_shape_named_gives_each_variant_the_name_shown)
{
    static DataBinRow row;
    ByteBuffer file = {0};
    ByteBuffer b = {0};
    DataBin bin;
    ItemNames names;
    int32_t slot = -1;
    make_items(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));
    ASSERT_TRUE(first_row(&bin, DATABIN_SHIELDS, &row));
    ASSERT_TRUE(items_slot(&bin, DATABIN_SHIELDS, &row, &slot));

    /* The buckler is place 1, common, in iron and in gold: materials 0
     * and 1. The files name the one in gold, and one in iron at another
     * place, so only the gold one gets a name. */
    uint8_t numbers[4];
    uint32_t gold = (1u << 12) | ((uint32_t)slot << 8) | 1u;
    uint32_t other = ((uint32_t)slot << 8) | 2u;
    numbers[0] = (uint8_t)(other & 0xff);
    numbers[1] = (uint8_t)(other >> 8);
    numbers[2] = (uint8_t)(gold & 0xff);
    numbers[3] = (uint8_t)(gold >> 8);
    ASSERT_TRUE(item_names_open(numbers, sizeof numbers,
        (const uint8_t *)"a\nb\n", 4,
        (const uint8_t *)"Other\nGolden Buckler\n", 21, &names));

    ASSERT_TRUE(item_toml_write_shape_named(&b, &bin, DATABIN_SHIELDS,
        &row, 1, 0, 0, NULL, &names));

    ASSERT_TRUE(has(&b,
        "\n[[variant]]\n"
        "folder = \"iron_common\"\n"
        "material = 0\n"
        "class = 0\n"
        "\n[[variant]]\n"
        "folder = \"gold_common\"\n"
        "display_name = \"Golden Buckler\"\n"
        "material = 1\n"
        "class = 0\n"));
    buffer_free(&b);
    buffer_free(&file);
}

UTEST(item_toml, write_magic_named_gives_the_name_shown)
{
    static DataBinRow row;
    ByteBuffer file = {0};
    ByteBuffer b = {0};
    DataBin bin;
    ItemNames names;
    const uint8_t numbers[] = {0x01, 0x0e};
    make_items(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));
    DataBinRows rows = databin_rows(&bin, DATABIN_MAGIC_ITEMS);
    ASSERT_TRUE(item_names_open(numbers, sizeof numbers,
        (const uint8_t *)"Book Air\n", 9, (const uint8_t *)"Book\n", 5,
        &names));

    /* The server knows the book by the name of its row. */
    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_TRUE(item_toml_write_magic_named(&b, &row, 1, 0, 0, &names));
    ASSERT_TRUE(holds(&b,
        "# Book Air\n"
        "id = 1\n"
        "name = \"Book Air\"\n"
        "display_name = \"Book\"\n"
        "kind = \"magic\"\n"
        "price = 1\n"
        "weight = 10\n"));
    b.size = 0;

    /* The scroll is not in the files, and is written without. */
    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_TRUE(item_toml_write_magic_named(&b, &row, 2, 0, 0, &names));
    ASSERT_FALSE(has(&b, "display_name"));
    buffer_free(&b);
    buffer_free(&file);
}

UTEST(item_toml, write_shape_says_which_characters_it_is_drawn_on)
{
    static DataBinRow row;
    const char *const characters[] = {"fighter_man", "fighter_woman"};
    ItemTomlWorn worn = {characters, 2, true, 160, 240};
    ByteBuffer file = {0};
    ByteBuffer b = {0};
    DataBin bin;
    make_items(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));
    ASSERT_TRUE(first_row(&bin, DATABIN_SHIELDS, &row));

    ASSERT_TRUE(item_toml_write_shape(&b, &bin, DATABIN_SHIELDS, &row, 1,
        80, 80, &worn));
    ASSERT_TRUE(has(&b,
        "size = [80, 80]\n"
        "\n"
        "[worn]\n"
        "characters = [\"fighter_man\", \"fighter_woman\"]\n"
        "second = true\n"
        "size = [160, 240]\n"
        "\n[[variant]]\n"));
    b.size = 0;

    /* Without a second picture that line is left out, and without any
     * character the whole part. */
    worn.has_second = false;
    ASSERT_TRUE(item_toml_write_shape(&b, &bin, DATABIN_SHIELDS, &row, 1,
        80, 80, &worn));
    ASSERT_TRUE(has(&b, "[worn]\n"
        "characters = [\"fighter_man\", \"fighter_woman\"]\n"
        "size = [160, 240]\n"));
    b.size = 0;
    worn.character_count = 0;
    ASSERT_TRUE(item_toml_write_shape(&b, &bin, DATABIN_SHIELDS, &row, 1,
        80, 80, &worn));
    ASSERT_FALSE(has(&b, "[worn]"));
    buffer_free(&b);
    buffer_free(&file);
}

UTEST(item_toml, write_shape_refuses_a_variant_the_tables_do_not_have)
{
    /* A dagger that exists in class 2 and material 3, where the tables
     * have two classes and two materials. */
    static DataBinRow row;
    ByteBuffer file = {0};
    ByteBuffer b = {0};
    DataBin bin;
    make_items(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));
    ASSERT_TRUE(first_row(&bin, DATABIN_WEAPONS, &row));
    row.numbers[row.number_count - 8 + 2] = 8;

    ASSERT_FALSE(item_toml_write_shape(&b, &bin, DATABIN_WEAPONS, &row, 2,
        80, 80, NULL));
    buffer_free(&b);
    buffer_free(&file);
}

UTEST(item_toml, variant_name_is_the_material_and_the_class)
{
    ByteBuffer file = {0};
    DataBin bin;
    char name[ITEM_TOML_VARIANT_NAME_SIZE] = "untouched";
    make_items(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));

    ASSERT_TRUE(item_toml_variant_name(&bin, 0, 0, name));
    ASSERT_STREQ("iron_common", name);
    ASSERT_TRUE(item_toml_variant_name(&bin, 1, 1, name));
    ASSERT_STREQ("gold_rare", name);
    /* No such material, and no such class. */
    ASSERT_FALSE(item_toml_variant_name(&bin, 2, 0, name));
    ASSERT_FALSE(item_toml_variant_name(&bin, 0, 2, name));
    ASSERT_STREQ("gold_rare", name);
    buffer_free(&file);
}

UTEST(item_toml, write_magic_gives_price_weight_and_effects)
{
    static DataBinRow row;
    ByteBuffer file = {0};
    ByteBuffer b = {0};
    DataBin bin;
    make_items(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));
    DataBinRows rows = databin_rows(&bin, DATABIN_MAGIC_ITEMS);

    /* The book does nothing by itself and has no text. */
    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_TRUE(item_toml_write_magic(&b, &row, 1, 0, 0));
    ASSERT_TRUE(holds(&b,
        "# Book Air\n"
        "id = 1\n"
        "name = \"Book Air\"\n"
        "kind = \"magic\"\n"
        "price = 1\n"
        "weight = 10\n"));
    b.size = 0;

    ASSERT_TRUE(databin_next(&rows, &row));
    ASSERT_TRUE(item_toml_write_magic(&b, &row, 2, 80, 80));
    ASSERT_TRUE(holds(&b,
        "# Scroll Fire Arrow\n"
        "id = 2\n"
        "name = \"Scroll Fire Arrow\"\n"
        "kind = \"magic\"\n"
        "price = 100\n"
        "weight = 1\n"
        "effects = \"castSpell=Fire_Arrow:50\"\n"
        "\n"
        "[icon]\n"
        "sheet = \"icon.png\"\n"
        "alpha = \"icon_alpha.png\"\n"
        "size = [80, 80]\n"));
    buffer_free(&b);
    buffer_free(&file);
}

UTEST(item_toml, write_materials_gives_an_entry_for_each_from_0)
{
    ByteBuffer file = {0};
    ByteBuffer b = {0};
    DataBin bin;
    make_items(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));

    ASSERT_TRUE(item_toml_write_materials(&b, &bin));

    /* The first two fractions of a row are left out. Gold's third,
     * which is its damage, is 2. */
    ASSERT_TRUE(has(&b,
        "\n[[material]]\n"
        "id = 0\n"
        "name = \"Iron\"\n"
        "price = 1.0\n"
        "weight = 1.0\n"
        "damage = 1.0\n"
        "to_hit = 1.0\n"
        "defence = 1.0\n"
        "absorption = 1.0\n"
        "magic = 1.0\n"
        "\n[[material]]\n"
        "id = 1\n"
        "name = \"Gold\"\n"
        "price = 1.0\n"
        "weight = 1.0\n"
        "damage = 2.0\n"));
    ASSERT_EQ('#', b.data[0]);
    buffer_free(&b);
    buffer_free(&file);
}

UTEST(item_toml, write_classes_reads_the_table_before_the_materials)
{
    ByteBuffer file = {0};
    ByteBuffer b = {0};
    DataBin bin;
    make_items(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));

    ASSERT_TRUE(item_toml_write_classes(&b, &bin));

    ASSERT_TRUE(has(&b, "\n[[class]]\nid = 0\nname = \"Common\"\n"
        "price = 1.0\n"));
    ASSERT_TRUE(has(&b, "\n[[class]]\nid = 1\nname = \"Rare\"\n"
        "price = 2.0\n"));
    ASSERT_FALSE(has(&b, "Iron"));
    buffer_free(&b);
    buffer_free(&file);
}

UTEST(item_toml, write_modifiers_splits_the_places_of_fighter_and_mage)
{
    ByteBuffer file = {0};
    ByteBuffer b = {0};
    DataBin bin;
    make_items(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));

    ASSERT_TRUE(item_toml_write_modifiers(&b, &bin));

    /* The first row is empty: a number and a name and nothing more. */
    ASSERT_TRUE(has(&b, "\n[[modifier]]\nid = 0\nname = \"\"\n"
        "\n[[modifier]]\n"
        "id = 1\n"
        "name = \"body\"\n"
        "mana_cost = 1\n"
        "affect_min = 2\n"
        "affect_max = 3\n"
        "usable_by = 4\n"
        "in_fighter = [5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]\n"
        "in_mage = [17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28]\n"));
    buffer_free(&b);
    buffer_free(&file);
}
