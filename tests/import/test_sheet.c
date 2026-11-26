#include "import/sheet.h"

#include "utest.h"

/* The pixel of a sheet at `x`, `y` counted over the whole sheet. */
static uint8_t at(const Sheet *s, uint32_t x, uint32_t y)
{
    return s->pixels[(size_t)y * sheet_width(s) + x];
}

/* ---- sheet_create and sheet_free ---- */

UTEST(sheet, create_gives_a_transparent_sheet_of_the_right_size)
{
    Sheet s;

    ASSERT_TRUE(sheet_create(&s, 3, 2, 4, 5));

    ASSERT_EQ(3u, s.cell_width);
    ASSERT_EQ(2u, s.cell_height);
    ASSERT_EQ(4u, s.columns);
    ASSERT_EQ(5u, s.rows);
    ASSERT_EQ(12u, sheet_width(&s));
    ASSERT_EQ(10u, sheet_height(&s));
    for (uint32_t y = 0; y < 10; y++) {
        for (uint32_t x = 0; x < 12; x++) {
            ASSERT_EQ(0, at(&s, x, y));
        }
    }
    sheet_free(&s);
}

UTEST(sheet, create_refuses_a_size_of_zero_or_more_than_the_limit)
{
    Sheet s;
    s.pixels = NULL;
    s.columns = 12345;

    ASSERT_FALSE(sheet_create(&s, 0, 2, 4, 5));
    ASSERT_FALSE(sheet_create(&s, 3, 0, 4, 5));
    ASSERT_FALSE(sheet_create(&s, 3, 2, 0, 5));
    ASSERT_FALSE(sheet_create(&s, 3, 2, 4, 0));
    /* One pixel more than 16384 wide, then high. */
    ASSERT_FALSE(sheet_create(&s, 16385, 1, 1, 1));
    ASSERT_FALSE(sheet_create(&s, 1, 1, 1, 16385));
    ASSERT_FALSE(sheet_create(&s, 129, 1, 128, 1));
    /* Sizes whose product wraps round in 32 bits. */
    ASSERT_FALSE(sheet_create(&s, 0x10000u, 1, 0x10000u, 1));

    ASSERT_TRUE(s.pixels == NULL);
    ASSERT_EQ(12345u, s.columns);
}

UTEST(sheet, create_accepts_a_sheet_exactly_at_the_limit)
{
    Sheet s;

    ASSERT_TRUE(sheet_create(&s, 128, 1, 128, 1));

    ASSERT_EQ(16384u, sheet_width(&s));
    sheet_free(&s);
}

UTEST(sheet, free_clears_the_sheet)
{
    Sheet s;
    ASSERT_TRUE(sheet_create(&s, 3, 2, 4, 5));

    sheet_free(&s);

    ASSERT_TRUE(s.pixels == NULL);
    ASSERT_EQ(0u, s.columns);
    ASSERT_EQ(0u, sheet_width(&s));
    sheet_free(&s); /* a second time must be harmless */
}

/* ---- sheet_put ---- */

UTEST(sheet, put_places_a_frame_in_its_cell_at_its_offset)
{
    /* Cells of 4 by 3. A 2 by 2 frame goes into the cell at column 2,
     * row 1, one pixel in from its left and one down from its top. */
    const uint8_t frame[4] = {
        1, 2,
        3, 4,
    };
    Sheet s;
    ASSERT_TRUE(sheet_create(&s, 4, 3, 3, 2));

    ASSERT_TRUE(sheet_put(&s, 2, 1, frame, 2, 2, 1, 1, false));

    /* The cell starts at pixel 8, 3 of the sheet. */
    ASSERT_EQ(1, at(&s, 9, 4));
    ASSERT_EQ(2, at(&s, 10, 4));
    ASSERT_EQ(3, at(&s, 9, 5));
    ASSERT_EQ(4, at(&s, 10, 5));
    /* Nothing else was touched. */
    size_t filled = 0;
    for (uint32_t y = 0; y < sheet_height(&s); y++) {
        for (uint32_t x = 0; x < sheet_width(&s); x++) {
            filled += at(&s, x, y) != 0;
        }
    }
    ASSERT_EQ(4u, filled);
    sheet_free(&s);
}

UTEST(sheet, put_mirrored_flips_the_frame_left_to_right)
{
    const uint8_t frame[6] = {
        1, 2, 3,
        4, 5, 6,
    };
    Sheet s;
    ASSERT_TRUE(sheet_create(&s, 4, 2, 2, 1));

    ASSERT_TRUE(sheet_put(&s, 1, 0, frame, 3, 2, 0, 0, true));

    ASSERT_EQ(3, at(&s, 4, 0));
    ASSERT_EQ(2, at(&s, 5, 0));
    ASSERT_EQ(1, at(&s, 6, 0));
    ASSERT_EQ(6, at(&s, 4, 1));
    ASSERT_EQ(5, at(&s, 5, 1));
    ASSERT_EQ(4, at(&s, 6, 1));
    ASSERT_EQ(0, at(&s, 7, 0));
    sheet_free(&s);
}

UTEST(sheet, put_leaves_what_is_there_under_transparent_pixels)
{
    const uint8_t first[2] = {7, 8};
    const uint8_t second[2] = {0, 9};
    Sheet s;
    ASSERT_TRUE(sheet_create(&s, 2, 1, 1, 1));
    ASSERT_TRUE(sheet_put(&s, 0, 0, first, 2, 1, 0, 0, false));

    ASSERT_TRUE(sheet_put(&s, 0, 0, second, 2, 1, 0, 0, false));

    ASSERT_EQ(7, at(&s, 0, 0));
    ASSERT_EQ(9, at(&s, 1, 0));
    sheet_free(&s);
}

UTEST(sheet, put_accepts_a_frame_that_fills_the_cell_exactly)
{
    const uint8_t frame[6] = {1, 2, 3, 4, 5, 6};
    Sheet s;
    ASSERT_TRUE(sheet_create(&s, 3, 2, 2, 2));

    ASSERT_TRUE(sheet_put(&s, 1, 1, frame, 3, 2, 0, 0, false));

    ASSERT_EQ(1, at(&s, 3, 2));
    ASSERT_EQ(6, at(&s, 5, 3));
    sheet_free(&s);
}

UTEST(sheet, put_refuses_a_frame_that_does_not_fit_or_a_missing_cell)
{
    const uint8_t frame[6] = {1, 2, 3, 4, 5, 6};
    Sheet s;
    ASSERT_TRUE(sheet_create(&s, 3, 2, 2, 2));

    /* One pixel too far right, then too far down. */
    ASSERT_FALSE(sheet_put(&s, 0, 0, frame, 3, 2, 1, 0, false));
    ASSERT_FALSE(sheet_put(&s, 0, 0, frame, 3, 2, 0, 1, false));
    /* Wider or higher than a cell. */
    ASSERT_FALSE(sheet_put(&s, 0, 0, frame, 6, 1, 0, 0, false));
    ASSERT_FALSE(sheet_put(&s, 0, 0, frame, 1, 3, 0, 0, false));
    /* An offset that would wrap round. */
    ASSERT_FALSE(sheet_put(&s, 0, 0, frame, 3, 2, 0xffffffffu, 0, false));
    ASSERT_FALSE(sheet_put(&s, 0, 0, frame, 3, 2, 0, 0xffffffffu, false));
    /* A column or row that is not there. */
    ASSERT_FALSE(sheet_put(&s, 2, 0, frame, 3, 2, 0, 0, false));
    ASSERT_FALSE(sheet_put(&s, 0, 2, frame, 3, 2, 0, 0, false));

    for (uint32_t y = 0; y < sheet_height(&s); y++) {
        for (uint32_t x = 0; x < sheet_width(&s); x++) {
            ASSERT_EQ(0, at(&s, x, y));
        }
    }
    sheet_free(&s);
}
