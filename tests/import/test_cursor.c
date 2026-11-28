#include "import/cursor.h"

#include "utest.h"

UTEST(cursor, a_cursor_has_the_tip_the_original_gives_it)
{
    int32_t tip[2] = {-1, -1};
    ASSERT_TRUE(cursor_tip("content/interface/cursors/default/sprites",
        tip));
    EXPECT_EQ(4, tip[0]);
    EXPECT_EQ(4, tip[1]);
    /* The arrow that points right has its tip on its right side. */
    ASSERT_TRUE(cursor_tip("out/interface/cursors/arrow2/sprites", tip));
    EXPECT_EQ(26, tip[0]);
    EXPECT_EQ(16, tip[1]);
    /* One of the small ones, which have no folder of their own. */
    ASSERT_TRUE(cursor_tip("out/interface/cursors/smove", tip));
    EXPECT_EQ(0, tip[0]);
    EXPECT_EQ(0, tip[1]);
}

UTEST(cursor, every_one_of_the_eight_arrows_has_a_tip)
{
    const char *paths[8] = {
        "c/cursors/arrow0/sprites", "c/cursors/arrow1/sprites",
        "c/cursors/arrow2/sprites", "c/cursors/arrow3/sprites",
        "c/cursors/arrow4/sprites", "c/cursors/arrow5/sprites",
        "c/cursors/arrow6/sprites", "c/cursors/arrow7/sprites"
    };
    /* Up, then round by the right: a tip lies on the side its arrow
     * points to. */
    const int32_t x[8] = {16, 23, 26, 23, 16, 8, 5, 8};
    const int32_t y[8] = {6, 9, 16, 24, 28, 24, 16, 9};
    for (int i = 0; i < 8; i++) {
        int32_t tip[2];
        ASSERT_TRUE(cursor_tip(paths[i], tip));
        EXPECT_EQ(x[i], tip[0]);
        EXPECT_EQ(y[i], tip[1]);
    }
}

UTEST(cursor, what_is_not_a_cursor_of_the_original_has_no_tip)
{
    int32_t tip[2] = {77, 88};
    EXPECT_FALSE(cursor_tip("content/interface/ball", tip));
    EXPECT_FALSE(cursor_tip("content/interface/cursors/l", tip));
    EXPECT_FALSE(cursor_tip("content/interface/cursors/default", tip));
    EXPECT_FALSE(cursor_tip("content/interface/cursors/default/sprites2",
        tip));
    EXPECT_FALSE(cursor_tip("", tip));
    EXPECT_EQ(77, tip[0]);
    EXPECT_EQ(88, tip[1]);
}

UTEST(cursor, the_folder_of_cursors_is_the_last_of_that_name_in_the_path)
{
    int32_t tip[2];
    ASSERT_TRUE(cursor_tip(
        "my/cursors/content/interface/cursors/attack/sprites", tip));
    EXPECT_EQ(3, tip[0]);
}
