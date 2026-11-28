#include "view/marks.h"

#include <string.h>

#include "utest.h"
#include "view/frame.h"

UTEST(marks, a_square_closed_to_the_one_asked_for_is_marked)
{
    uint8_t cost[6] = {8, 8, 8, 8, 8, 8};
    uint8_t closed[6] = {0, TERRAIN_NO_WALK, TERRAIN_NO_ONE,
        TERRAIN_NO_HOVER, TERRAIN_NO_WALK | TERRAIN_NO_HOVER,
        TERRAIN_NO_FLY};
    Terrain terrain = {3, 2, cost, closed, NULL};
    uint8_t marks[7] = {9, 9, 9, 9, 9, 9, 9};
    const unsigned yes = FRAME_MARK_CLOSED;
    const unsigned no = FRAME_MARK_NONE;
    const unsigned walk[6] = {no, yes, yes, no, yes, no};
    /* What hovers stops at other squares, and what flies at fewer. */
    const unsigned hover[6] = {no, no, yes, yes, yes, no};
    const unsigned fly[6] = {no, no, yes, no, no, yes};
    marks_closed(&terrain, TERRAIN_NO_WALK, marks);
    for (int i = 0; i < 6; i++) {
        EXPECT_EQ(walk[i], marks[i]);
    }
    /* And nothing past the table. */
    EXPECT_EQ(9u, marks[6]);
    marks_closed(&terrain, TERRAIN_NO_HOVER, marks);
    for (int i = 0; i < 6; i++) {
        EXPECT_EQ(hover[i], marks[i]);
    }
    marks_closed(&terrain, TERRAIN_NO_FLY, marks);
    for (int i = 0; i < 6; i++) {
        EXPECT_EQ(fly[i], marks[i]);
    }
}

UTEST(marks, the_squares_a_unit_stands_on_are_marked_and_given_back)
{
    Units units;
    ASSERT_TRUE(units_make(8, 6, &units));
    Unit unit;
    memset(&unit, 0, sizeof unit);
    unit.x = 3 * 256 + 128;
    unit.y = 2 * 256 + 128;
    unit.size = 2;
    UnitId id;
    ASSERT_TRUE(units_add(&units, &unit, &id));
    uint8_t under[48];
    uint8_t marks[48];
    memset(under, FRAME_MARK_NONE, sizeof under);
    under[3 * 8 + 4] = FRAME_MARK_CLOSED;
    memcpy(marks, under, sizeof marks);
    marks_held(&units, id, true, under, marks);
    int held = 0;
    for (int i = 0; i < 48; i++) {
        held += marks[i] == FRAME_MARK_HELD ? 1 : 0;
    }
    EXPECT_EQ(4, held);
    EXPECT_EQ((unsigned)FRAME_MARK_HELD, marks[2 * 8 + 3]);
    EXPECT_EQ((unsigned)FRAME_MARK_HELD, marks[2 * 8 + 4]);
    EXPECT_EQ((unsigned)FRAME_MARK_HELD, marks[3 * 8 + 3]);
    EXPECT_EQ((unsigned)FRAME_MARK_HELD, marks[3 * 8 + 4]);
    /* Given back, each has what was under it. */
    marks_held(&units, id, false, under, marks);
    EXPECT_EQ(0, memcmp(marks, under, sizeof marks));
    /* In the first half of a step, those it reaches for as well. */
    ASSERT_TRUE(units_step(&units, id, 5, 2));
    marks_held(&units, id, true, under, marks);
    held = 0;
    for (int i = 0; i < 48; i++) {
        held += marks[i] == FRAME_MARK_HELD ? 1 : 0;
    }
    EXPECT_EQ(8, held);
    EXPECT_EQ((unsigned)FRAME_MARK_HELD, marks[3 * 8 + 6]);
    marks_held(&units, id, false, under, marks);
    EXPECT_EQ(0, memcmp(marks, under, sizeof marks));
    /* A unit that is gone marks nothing. */
    units_remove(&units, id);
    marks_held(&units, id, true, under, marks);
    EXPECT_EQ(0, memcmp(marks, under, sizeof marks));
    units_free(&units);
}

UTEST(marks, a_square_that_is_seen_is_marked_unless_it_has_a_mark)
{
    const uint8_t seen[5] = {0, 1, 1, 0, 1};
    uint8_t marks[5] = {FRAME_MARK_NONE, FRAME_MARK_NONE,
        FRAME_MARK_CLOSED, FRAME_MARK_HELD, FRAME_MARK_NONE};
    marks_seen(seen, 4, marks);
    EXPECT_EQ((unsigned)FRAME_MARK_NONE, marks[0]);
    EXPECT_EQ((unsigned)FRAME_MARK_SEEN, marks[1]);
    EXPECT_EQ((unsigned)FRAME_MARK_CLOSED, marks[2]);
    EXPECT_EQ((unsigned)FRAME_MARK_HELD, marks[3]);
    /* And nothing past those asked for. */
    EXPECT_EQ((unsigned)FRAME_MARK_NONE, marks[4]);
}
