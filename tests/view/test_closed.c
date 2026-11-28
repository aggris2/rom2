#include "view/closed.h"

#include "utest.h"

UTEST(closed, a_square_closed_to_the_one_asked_for_is_marked)
{
    uint8_t cost[6] = {8, 8, 8, 8, 8, 8};
    uint8_t closed[6] = {0, TERRAIN_NO_WALK, TERRAIN_NO_ONE,
        TERRAIN_NO_HOVER, TERRAIN_NO_WALK | TERRAIN_NO_HOVER,
        TERRAIN_NO_FLY};
    Terrain terrain = {3, 2, cost, closed};
    uint8_t marks[7] = {9, 9, 9, 9, 9, 9, 9};
    closed_marks(&terrain, TERRAIN_NO_WALK, marks);
    EXPECT_EQ(0u, marks[0]);
    EXPECT_EQ((unsigned)CLOSED_MARK, marks[1]);
    EXPECT_EQ((unsigned)CLOSED_MARK, marks[2]);
    /* Closed to what hovers or flies alone is open to what walks. */
    EXPECT_EQ(0u, marks[3]);
    EXPECT_EQ((unsigned)CLOSED_MARK, marks[4]);
    EXPECT_EQ(0u, marks[5]);
    /* And nothing past the table. */
    EXPECT_EQ(9u, marks[6]);
    /* What hovers stops at other squares, and what flies at fewer. */
    const unsigned hover[6] = {0, 0, CLOSED_MARK, CLOSED_MARK, CLOSED_MARK,
        0};
    const unsigned fly[6] = {0, 0, CLOSED_MARK, 0, 0, CLOSED_MARK};
    closed_marks(&terrain, TERRAIN_NO_HOVER, marks);
    for (int i = 0; i < 6; i++) {
        EXPECT_EQ(hover[i], marks[i]);
    }
    closed_marks(&terrain, TERRAIN_NO_FLY, marks);
    for (int i = 0; i < 6; i++) {
        EXPECT_EQ(fly[i], marks[i]);
    }
}
