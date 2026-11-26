#include "import/items.h"

#include <string.h>

#include "make_databin.h"
#include "utest.h"

/* A row as a shape's is: `plain` numbers and then the 8 masks. */
static DataBinRow make_row(uint32_t plain, const int32_t masks[8])
{
    static DataBinRow row;
    memset(&row, 0, sizeof row);
    row.number_count = plain + 8;
    for (uint32_t i = 0; i < 8; i++) {
        row.numbers[plain + i] = masks[i];
    }
    return row;
}

/* ---- items_variants ---- */

UTEST(items, variants_lists_each_material_a_class_is_allowed_in)
{
    /* Class 0 in materials 0 and 1, class 2 in materials 3, 5 and 6. */
    const int32_t masks[8] = {3, 0, 104, 0, 0, 0, 0, 0};
    DataBinRow row = make_row(17, masks);
    ItemVariant v[ITEM_MAX_VARIANTS];

    ASSERT_EQ(5u, items_variants(&row, v));

    ASSERT_EQ(0, v[0].material);
    ASSERT_EQ(0, v[0].item_class);
    ASSERT_EQ(1, v[1].material);
    ASSERT_EQ(0, v[1].item_class);
    ASSERT_EQ(3, v[2].material);
    ASSERT_EQ(2, v[2].item_class);
    ASSERT_EQ(5, v[3].material);
    ASSERT_EQ(6, v[4].material);
    ASSERT_EQ(2, v[4].item_class);
}

UTEST(items, variants_reads_the_highest_material_and_the_last_class)
{
    /* 32768 is material 15. The masks are the last 8 numbers, however
     * many come before them. */
    const int32_t masks[8] = {0, 0, 0, 0, 0, 0, 0, 32768};
    DataBinRow row = make_row(3, masks);
    ItemVariant v[ITEM_MAX_VARIANTS];

    ASSERT_EQ(1u, items_variants(&row, v));
    ASSERT_EQ(15, v[0].material);
    ASSERT_EQ(7, v[0].item_class);
}

UTEST(items, variants_of_a_shape_that_exists_in_everything)
{
    const int32_t masks[8] = {
        65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535
    };
    DataBinRow row = make_row(17, masks);
    ItemVariant v[ITEM_MAX_VARIANTS];

    ASSERT_EQ((uint32_t)ITEM_MAX_VARIANTS, items_variants(&row, v));
    ASSERT_EQ(15, v[ITEM_MAX_VARIANTS - 1].material);
    ASSERT_EQ(7, v[ITEM_MAX_VARIANTS - 1].item_class);
}

UTEST(items, variants_of_a_row_without_masks_or_with_none_set)
{
    const int32_t none[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    DataBinRow row = make_row(17, none);
    ItemVariant v[ITEM_MAX_VARIANTS];
    ASSERT_EQ(0u, items_variants(&row, v));

    /* A row of 7 numbers is too short to end in 8 masks. */
    memset(&row, 0, sizeof row);
    row.number_count = 7;
    row.numbers[0] = 65535;
    ASSERT_EQ(0u, items_variants(&row, v));
}

/* ---- items_slot ---- */

UTEST(items, slot_reads_the_column_called_slot)
{
    static DataBinRow row;
    const char *const empty[] = {"a"};
    const char *const columns[] = {"Price", "Slot"};
    const int32_t dagger[] = {100, 1};
    ByteBuffer file = {0};
    DataBin bin;
    int32_t slot = 99;
    put_header(&file, empty, 1);  /* shapes and materials */
    put_i32(&file, 0);
    put_i32(&file, 0);
    put_header(&file, empty, 1);  /* modifiers */
    put_i32(&file, 0);
    put_header(&file, columns, 2);
    put_i32(&file, 1);            /* armour */
    put_i32(&file, 1);            /* shields */
    put_i32(&file, 2);            /* weapons */
    put_row(&file, "Dagger", dagger, 2);
    for (int i = 0; i < 8; i++) {
        put_u16(&file, 0);
    }
    put_header(&file, empty, 1);  /* magic items */
    put_i32(&file, 1);
    put_header(&file, empty, 1);  /* monsters */
    put_i32(&file, 1);
    put_header(&file, empty, 1);  /* humans */
    put_i32(&file, 1);
    put_empty_last_tables(&file);
    ASSERT_TRUE(databin_open(file.data, file.size, &bin));
    DataBinRows rows = databin_rows(&bin, DATABIN_WEAPONS);
    ASSERT_TRUE(databin_next(&rows, &row));

    ASSERT_TRUE(items_slot(&bin, DATABIN_WEAPONS, &row, &slot));
    ASSERT_EQ(1, slot);
    /* The buildings have no such column. */
    ASSERT_FALSE(items_slot(&bin, DATABIN_BUILDINGS, &row, &slot));
    /* A row too short to have the number. */
    row.number_count = 1;
    ASSERT_FALSE(items_slot(&bin, DATABIN_WEAPONS, &row, &slot));
    ASSERT_EQ(1, slot);
    buffer_free(&file);
}

/* ---- items_file_name ---- */

UTEST(items, file_name_is_material_slot_class_and_place_in_seven_digits)
{
    char name[ITEM_FILE_NAME_SIZE];
    ASSERT_TRUE(items_file_name(0, 1, 0, 2, name));
    ASSERT_STREQ("0001002", name);
    ASSERT_TRUE(items_file_name(15, 12, 4, 27, name));
    ASSERT_STREQ("1512427", name);
    ASSERT_TRUE(items_file_name(0, 14, 0, 96, name));
    ASSERT_STREQ("0014096", name);
    ASSERT_TRUE(items_file_name(99, 99, 9, 99, name));
    ASSERT_STREQ("9999999", name);
}

UTEST(items, file_name_refuses_a_number_that_does_not_fit_its_digits)
{
    char name[ITEM_FILE_NAME_SIZE] = "before";
    ASSERT_FALSE(items_file_name(100, 1, 0, 2, name));
    ASSERT_FALSE(items_file_name(0, 100, 0, 2, name));
    ASSERT_FALSE(items_file_name(0, 1, 10, 2, name));
    ASSERT_FALSE(items_file_name(0, 1, 0, 100, name));
    ASSERT_FALSE(items_file_name(-1, 1, 0, 2, name));
    ASSERT_FALSE(items_file_name(0, -1, 0, 2, name));
    ASSERT_FALSE(items_file_name(0, 1, -1, 2, name));
    ASSERT_FALSE(items_file_name(0, 1, 0, -1, name));
    ASSERT_STREQ("before", name);
}
