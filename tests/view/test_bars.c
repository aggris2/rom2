#include "view/bars.h"

#include <string.h>

#include "utest.h"
#include "view/frame.h"

/* A flat map of 10 by 10 squares and one type of unit, type 3, whose
 * selection box is 32 wide and starts 30 above its anchor. */
typedef struct MakeBars {
    Map map;
    uint8_t heights[100];
    Units live;
    UnitSlot slots[3];
    UnitLook looks[UNIT_LOOKS];
    BarsLook bars;
    Selection selected;
    uint32_t chosen[UNITS_MOST + 1];
    bool all;
    Sprite sprites[80];
} MakeBars;

static void make_bars(MakeBars *m)
{
    memset(m, 0, sizeof *m);
    m->map.info.width = 10;
    m->map.info.height = 10;
    m->map.heights = m->heights;
    m->live.slots = m->slots;
    m->live.room = 3;
    m->looks[3].known = true;
    m->looks[3].selection = (SpriteBox){-16, -30, 16, 12};
    m->bars.known = true;
    m->bars.sheet = 9;
    m->selected.chosen = m->chosen;
}

/* Puts a unit in the middle of the square (x, y) with this health of
 * 100 and no mana, and returns it. */
static Unit *put(MakeBars *m, int x, int y, int32_t health)
{
    Unit unit;
    UnitId id;
    memset(&unit, 0, sizeof unit);
    unit.type = 3;
    unit.x = x * 256 + 128;
    unit.y = y * 256 + 128;
    unit.health = health;
    unit.health_most = 100;
    return units_add(&m->live, &unit, &id) ? units_get(&m->live, id) : NULL;
}

static void select_slot(MakeBars *m, uint16_t slot)
{
    selection_put(&m->selected, (UnitId){slot, 0}, true);
}

/* The sprites for a screen that shows the map from its corner, or
 * 9999 if more were written than bars_most() allows. */
static size_t sprites_for(MakeBars *m)
{
    Camera camera = {0, 0, 320, 320};
    size_t count = bars_sprites(&m->map, &m->live, m->looks, &m->bars,
        &m->selected, m->all, 0, &camera, m->sprites);
    return count <= bars_most(&m->live) ? count : 9999;
}

UTEST(bars, the_selected_unit_has_a_bar_between_two_balls)
{
    static MakeBars m;
    make_bars(&m);
    /* He stands at (144, 176): the top of his box is at 146, and it
     * goes from 128 to 160. */
    put(&m, 4, 5, 100);
    select_slot(&m, 0);
    ASSERT_EQ(10u, sprites_for(&m));
    /* The grey ground, a line to a sprite: 24 long between the
     * balls. */
    const BarLine ground[4] = {BAR_GROUND_DARK, BAR_GROUND_LIGHT,
        BAR_GROUND_MIDDLE, BAR_GROUND_DARK};
    const BarLine green[4] = {BAR_GREEN_DARK, BAR_GREEN_BRIGHT,
        BAR_GREEN_MIDDLE, BAR_GREEN_DARK};
    for (int i = 0; i < 4; i++) {
        const Sprite *back = &m.sprites[i];
        const Sprite *line = &m.sprites[4 + i];
        EXPECT_EQ(9u, back->sheet);
        EXPECT_EQ(0u, back->source_x);
        EXPECT_EQ((unsigned)ground[i], back->source_y);
        EXPECT_EQ(24u, back->width);
        EXPECT_EQ(1u, back->height);
        EXPECT_EQ(132, back->x);
        EXPECT_EQ(144 + i, back->y);
        EXPECT_EQ((unsigned)FRAME_LIGHT_WHOLE, back->light);
        EXPECT_EQ(0u, back->palette);
        EXPECT_EQ((unsigned)SPRITE_ORDER_BARS, back->order);
        EXPECT_EQ((unsigned)green[i], line->source_y);
        EXPECT_EQ(24u, line->width);
        EXPECT_EQ(132, line->x);
        EXPECT_EQ(144 + i, line->y);
    }
    /* The balls, at the two ends. */
    for (int i = 8; i < 10; i++) {
        EXPECT_EQ(0u, m.sprites[i].source_x);
        EXPECT_EQ(2u * BAR_LINES, m.sprites[i].source_y);
        EXPECT_EQ(4u, m.sprites[i].width);
        EXPECT_EQ(4u, m.sprites[i].height);
        EXPECT_EQ(144, m.sprites[i].y);
    }
    EXPECT_EQ(128, m.sprites[8].x);
    EXPECT_EQ(156, m.sprites[9].x);
}

UTEST(bars, the_bar_is_as_long_as_the_health_that_is_left)
{
    static MakeBars m;
    make_bars(&m);
    put(&m, 4, 5, 50);
    select_slot(&m, 0);
    ASSERT_EQ(10u, sprites_for(&m));
    EXPECT_EQ(24u, m.sprites[0].width);
    EXPECT_EQ(12u, m.sprites[4].width);
    /* Cut down to whole pixels, but never to none while any is left. */
    m.slots[0].unit.health = 1;
    ASSERT_EQ(10u, sprites_for(&m));
    EXPECT_EQ(1u, m.sprites[4].width);
    /* With none left there is the ground and the balls alone. */
    m.slots[0].unit.health = 0;
    ASSERT_EQ(6u, sprites_for(&m));
    m.slots[0].unit.health = -30;
    EXPECT_EQ(6u, sprites_for(&m));
    /* And more than the most is the whole bar. */
    m.slots[0].unit.health = 500;
    ASSERT_EQ(10u, sprites_for(&m));
    EXPECT_EQ(24u, m.sprites[4].width);
}

UTEST(bars, health_is_green_then_yellow_under_half_and_red_under_a_quarter)
{
    static MakeBars m;
    make_bars(&m);
    put(&m, 4, 5, 50);
    select_slot(&m, 0);
    const int32_t health[6] = {100, 50, 49, 25, 24, 1};
    const BarLine bright[6] = {BAR_GREEN_BRIGHT, BAR_GREEN_BRIGHT,
        BAR_YELLOW_BRIGHT, BAR_YELLOW_BRIGHT, BAR_RED_BRIGHT,
        BAR_RED_BRIGHT};
    for (int i = 0; i < 6; i++) {
        m.slots[0].unit.health = health[i];
        ASSERT_EQ(10u, sprites_for(&m));
        EXPECT_EQ((unsigned)bright[i] + 2, m.sprites[4].source_y);
        EXPECT_EQ((unsigned)bright[i], m.sprites[5].source_y);
        EXPECT_EQ((unsigned)bright[i] + 1, m.sprites[6].source_y);
        EXPECT_EQ((unsigned)bright[i] + 2, m.sprites[7].source_y);
    }
}

UTEST(bars, a_unit_with_mana_has_a_blue_bar_under_the_first)
{
    static MakeBars m;
    make_bars(&m);
    Unit *mage = put(&m, 4, 5, 100);
    mage->mana = 30;
    mage->mana_most = 60;
    select_slot(&m, 0);
    ASSERT_EQ(20u, sprites_for(&m));
    EXPECT_EQ((unsigned)BAR_GROUND_DARK, m.sprites[10].source_y);
    EXPECT_EQ(148, m.sprites[10].y);
    EXPECT_EQ((unsigned)BAR_BLUE_DARK, m.sprites[14].source_y);
    EXPECT_EQ((unsigned)BAR_BLUE_BRIGHT, m.sprites[15].source_y);
    EXPECT_EQ((unsigned)BAR_BLUE_MIDDLE, m.sprites[16].source_y);
    EXPECT_EQ(12u, m.sprites[15].width);
    EXPECT_EQ(148, m.sprites[18].y);
    /* One with mana and no health has that bar alone. */
    mage->health_most = 0;
    EXPECT_EQ(10u, sprites_for(&m));
    mage->mana_most = -1;
    EXPECT_EQ(0u, sprites_for(&m));
}

UTEST(bars, without_all_only_the_selected_unit_shows_its_bars)
{
    static MakeBars m;
    make_bars(&m);
    put(&m, 4, 5, 100);
    put(&m, 6, 5, 100);
    EXPECT_EQ(0u, sprites_for(&m));
    select_slot(&m, 1);
    ASSERT_EQ(10u, sprites_for(&m));
    EXPECT_EQ(6 * 32 + 16 - 16 + 4, m.sprites[0].x);
    /* An id of a unit that was in the slot before is not this one. */
    selection_clear(&m.selected);
    selection_put(&m.selected, (UnitId){1, 3}, true);
    EXPECT_EQ(0u, sprites_for(&m));
}

UTEST(bars, with_all_the_others_are_faint_and_have_no_ground)
{
    static MakeBars m;
    make_bars(&m);
    put(&m, 4, 5, 100);
    put(&m, 6, 5, 40);
    m.all = true;
    select_slot(&m, 0);
    ASSERT_EQ(10u + 6u, sprites_for(&m));
    /* The second: its lines from the half see-through rows, then its
     * balls, which are as ever. */
    EXPECT_EQ((unsigned)BAR_LINES + BAR_YELLOW_DARK, m.sprites[10].source_y);
    EXPECT_EQ((unsigned)BAR_LINES + BAR_YELLOW_BRIGHT,
        m.sprites[11].source_y);
    EXPECT_EQ(9u, m.sprites[10].width);
    EXPECT_EQ(2u * BAR_LINES, m.sprites[14].source_y);
    /* With none selected all are faint. */
    selection_clear(&m.selected);
    EXPECT_EQ(12u, sprites_for(&m));
}

UTEST(bars, a_unit_off_the_screen_or_not_drawn_has_no_bars)
{
    static MakeBars m;
    make_bars(&m);
    put(&m, 4, 5, 100);
    Unit *odd = put(&m, 6, 5, 100);
    odd->type = 8;
    m.all = true;
    EXPECT_EQ(6u, sprites_for(&m));
    Camera far = {1000, 1000, 320, 320};
    EXPECT_EQ(0u, bars_sprites(&m.map, &m.live, m.looks, &m.bars, NULL,
        true, 0, &far, m.sprites));
    /* Nor is anything drawn without the sheet. */
    m.bars.known = false;
    EXPECT_EQ(0u, sprites_for(&m));
}

UTEST(bars, the_bar_stands_where_the_unit_is_drawn)
{
    static MakeBars m;
    make_bars(&m);
    put(&m, 4, 5, 100);
    select_slot(&m, 0);
    for (int i = 0; i < 100; i++) {
        m.heights[i] = 20;
    }
    ASSERT_EQ(10u, sprites_for(&m));
    EXPECT_EQ(144 - 20, m.sprites[0].y);
}

UTEST(bars, every_line_has_its_colour)
{
    Rgb green = bars_colour(BAR_GREEN_BRIGHT);
    EXPECT_EQ(0u, green.r);
    EXPECT_EQ(255u, green.g);
    EXPECT_EQ(0u, green.b);
    Rgb blue = bars_colour(BAR_BLUE_DARK);
    EXPECT_EQ(0u, blue.r);
    EXPECT_EQ(128u, blue.b);
    Rgb ground = bars_colour(BAR_GROUND_LIGHT);
    EXPECT_EQ(128u, ground.r);
    EXPECT_EQ(128u, ground.g);
    EXPECT_EQ(128u, ground.b);
    EXPECT_EQ(96u, bars_colour(BAR_GROUND_MIDDLE).g);
    EXPECT_EQ(64u, bars_colour(BAR_GROUND_DARK).g);
}
