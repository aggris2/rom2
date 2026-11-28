#include "view/units.h"

#include <string.h>

#include "utest.h"
#include "view/light.h"

/* A map of 10 by 10 squares, flat, with room for a few units, and one
 * type in the plain dress: type 3 is a swordsman in cells of 64 by 48
 * with his anchor between his feet. */
typedef struct MakeUnits {
    Map map;
    uint8_t heights[100];
    Units live;
    UnitSlot slots[4];
    MapPlayer players[3];
    UnitLook looks[UNIT_LOOKS];
    UnitKind kinds[UNIT_KIND_IDS];
    uint64_t tick;
    Sprite sprites[4];
} MakeUnits;

static void make_units(MakeUnits *m)
{
    memset(m, 0, sizeof *m);
    m->map.info.width = 10;
    m->map.info.height = 10;
    m->map.heights = m->heights;
    /* Room for the units, with nothing to give back afterwards. */
    m->live.slots = m->slots;
    m->live.room = 4;
    /* Three players, with the colours 0, 7 and 2. */
    m->map.players = m->players;
    m->map.player_count = 3;
    m->players[1].colour = 7;
    m->players[2].colour = 2;
    m->looks[3].known = true;
    m->looks[3].sheet = 30;
    m->looks[3].cell_width = 64;
    m->looks[3].cell_height = 48;
    m->looks[3].anchor_x = 32;
    m->looks[3].anchor_y = 40;
}

/* Puts a unit of type `type` in the middle of the square (x, y): its
 * place is in 256ths of a square. */
static Unit *put(MakeUnits *m, int x, int y, uint16_t type)
{
    Unit unit;
    UnitId id;
    memset(&unit, 0, sizeof unit);
    unit.x = x * 256 + 128;
    unit.y = y * 256 + 128;
    unit.type = type;
    return units_add(&m->live, &unit, &id) ? units_get(&m->live, id) : NULL;
}

/* The sprites for a camera, or 9999 if more were written than
 * units_most() allows. */
static size_t sprites_for(MakeUnits *m, int32_t left, int32_t top,
                          int32_t width, int32_t height)
{
    Camera camera = {left, top, width, height};
    size_t count = units_sprites(&m->map, &m->live, m->looks, m->kinds,
        m->tick, &camera, m->sprites);
    return count <= units_most(&m->live) ? count : 9999;
}

UTEST(units, a_hero_is_drawn_in_the_look_of_his_type_and_his_dress)
{
    MakeUnits m;
    make_units(&m);
    UnitLook *armour = &m.looks[unit_look_at(3, UNIT_DRESS_HERO_ARMOUR)];
    *armour = m.looks[3];
    armour->sheet = 77;
    armour->cell_width = 88;
    armour->anchor_x = 44;
    put(&m, 4, 5, 3)->dress = UNIT_DRESS_HERO_ARMOUR;
    put(&m, 6, 5, 3);
    ASSERT_EQ(2u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(77u, m.sprites[0].sheet);
    EXPECT_EQ(88u, m.sprites[0].width);
    EXPECT_EQ(4 * 32 + 16 - 44, m.sprites[0].x);
    EXPECT_EQ(30u, m.sprites[1].sheet);
    EXPECT_EQ(64u, m.sprites[1].width);
}

UTEST(units, a_dress_without_a_look_or_that_there_is_not_is_not_drawn)
{
    MakeUnits m;
    make_units(&m);
    put(&m, 4, 5, 3)->dress = UNIT_DRESS_HERO_LIGHT;
    put(&m, 6, 5, 3)->dress = UNIT_DRESSES;
    EXPECT_EQ(0u, sprites_for(&m, 0, 0, 320, 320));
}

UTEST(units, a_unit_stands_with_its_anchor_on_its_place)
{
    MakeUnits m;
    make_units(&m);
    put(&m, 4, 5, 3);
    ASSERT_EQ(1u, sprites_for(&m, 100, 100, 200, 200));
    const Sprite *s = &m.sprites[0];
    EXPECT_EQ(30u, s->sheet);
    EXPECT_EQ(64u, s->width);
    EXPECT_EQ(48u, s->height);
    /* The middle of square (4, 5) is the pixel (144, 176). */
    EXPECT_EQ(144 - 32 - 100, s->x);
    EXPECT_EQ(176 - 40 - 100, s->y);
    EXPECT_EQ((unsigned)LIGHT_STANDING, s->light);
    EXPECT_EQ(sprite_order(4, 5), s->order);
}

UTEST(units, a_place_is_in_256ths_of_a_square)
{
    MakeUnits m;
    make_units(&m);
    Unit *unit = put(&m, 4, 5, 3);
    /* A quarter of a square in, and three quarters down. */
    unit->x = 4 * 256 + 64;
    unit->y = 5 * 256 + 192;
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(4 * 32 + 8 - 32, m.sprites[0].x);
    EXPECT_EQ(5 * 32 + 24 - 40, m.sprites[0].y);
    EXPECT_EQ(sprite_order(4, 5), m.sprites[0].order);
}

UTEST(units, the_way_a_unit_faces_picks_its_frame_of_the_first_row)
{
    MakeUnits m;
    make_units(&m);
    Unit *unit = put(&m, 4, 5, 3);
    /* A sixteenth of a turn is 16 of the game's 256 steps, and the
     * frame of a facing is the one half a turn further on. */
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(8u * 64, m.sprites[0].source_x);
    EXPECT_EQ(0u, m.sprites[0].source_y);
    unit->facing = 5 * 16;
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(13u * 64, m.sprites[0].source_x);
    EXPECT_EQ(0u, m.sprites[0].source_y);
    unit->facing = 8 * 16;
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(0u, m.sprites[0].source_x);
    unit->facing = 255;
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(7u * 64, m.sprites[0].source_x);
}

UTEST(units, a_facing_between_two_pictures_shows_the_one_before)
{
    MakeUnits m;
    make_units(&m);
    Unit *unit = put(&m, 4, 5, 3);
    unit->facing = 5 * 16 + 15;
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(13u * 64, m.sprites[0].source_x);
    unit->facing = 6 * 16;
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(14u * 64, m.sprites[0].source_x);
}

/* Gives type 3 an idle animation in the columns from 18: frame 0 for
 * two ticks, then 1 and 2 for one each. */
static void make_it_flap(MakeUnits *m)
{
    m->looks[3].idle.column = 18;
    m->looks[3].idle.round.length = 4;
    m->looks[3].idle.round.frame[0] = 0;
    m->looks[3].idle.round.frame[1] = 0;
    m->looks[3].idle.round.frame[2] = 1;
    m->looks[3].idle.round.frame[3] = 2;
}

UTEST(units, a_unit_with_an_idle_animation_shows_its_frame_of_the_tick)
{
    MakeUnits m;
    make_units(&m);
    make_it_flap(&m);
    put(&m, 4, 5, 3);
    const uint16_t columns[8] = {18, 18, 19, 20, 18, 18, 19, 20};
    for (uint64_t tick = 0; tick < 8; tick++) {
        m.tick = tick;
        ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
        EXPECT_EQ(columns[tick] * 64, m.sprites[0].source_x);
    }
}

UTEST(units, an_idle_animation_is_shown_from_the_row_of_the_way_faced)
{
    MakeUnits m;
    make_units(&m);
    make_it_flap(&m);
    Unit *unit = put(&m, 4, 5, 3);
    /* The first row is the standing frames, and the rows of an
     * animation come after it. The first way faced is half a turn on:
     * the fifth of the eight rows. */
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(5u * 48, m.sprites[0].source_y);
    /* Half a turn round is the first row of the animation. */
    unit->facing = 128;
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(1u * 48, m.sprites[0].source_y);
    /* Two of the 16 standing ways share a row: an eighth of a turn
     * is 32 steps. */
    unit->facing = 128 + 31;
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(1u * 48, m.sprites[0].source_y);
    unit->facing = 128 + 32;
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(2u * 48, m.sprites[0].source_y);
    unit->facing = 127;
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(8u * 48, m.sprites[0].source_y);
}

UTEST(units, a_unit_without_an_idle_animation_stands_still_at_any_tick)
{
    MakeUnits m;
    make_units(&m);
    put(&m, 4, 5, 3);
    m.tick = 12345;
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(8u * 64, m.sprites[0].source_x);
    EXPECT_EQ(0u, m.sprites[0].source_y);
}

UTEST(units, a_unit_that_is_gone_is_not_drawn)
{
    MakeUnits m;
    make_units(&m);
    put(&m, 1, 1, 3);
    put(&m, 4, 5, 3);
    UnitId first = {0, 0};
    units_remove(&m.live, first);
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(144 - 32, m.sprites[0].x);
}

UTEST(units, a_unit_is_raised_by_the_ground_it_stands_on)
{
    MakeUnits m;
    make_units(&m);
    put(&m, 4, 5, 3);
    /* The four corners of its square, all at 40. */
    m.heights[5 * 10 + 4] = 40;
    m.heights[5 * 10 + 5] = 40;
    m.heights[6 * 10 + 4] = 40;
    m.heights[6 * 10 + 5] = 40;
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(176 - 40 - 40, m.sprites[0].y);
}

UTEST(units, on_uneven_ground_a_unit_is_raised_by_the_height_at_its_point)
{
    MakeUnits m;
    make_units(&m);
    Unit *unit = put(&m, 4, 5, 3);
    /* In the middle of its square, the mean of the four corners,
     * rounded: 51 and a half is 52. */
    m.heights[5 * 10 + 4] = 59;
    m.heights[5 * 10 + 5] = 59;
    m.heights[6 * 10 + 4] = 55;
    m.heights[6 * 10 + 5] = 33;
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(176 - 40 - 52, m.sprites[0].y);
    /* On the top left corner of the square, the height of that
     * corner. */
    unit->x = 4 * 256;
    unit->y = 5 * 256;
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(160 - 40 - 59, m.sprites[0].y);
}

UTEST(units, a_unit_whose_sheet_goes_by_the_player_has_its_players_colour)
{
    MakeUnits m;
    make_units(&m);
    m.looks[3].by_player = true;
    /* Players are counted from 1. */
    put(&m, 1, 1, 3)->player = 2;
    put(&m, 2, 2, 3)->player = 3;
    put(&m, 3, 3, 3)->player = 1;
    ASSERT_EQ(3u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(7u, m.sprites[0].palette);
    EXPECT_EQ(2u, m.sprites[1].palette);
    EXPECT_EQ(0u, m.sprites[2].palette);
}

UTEST(units, a_unit_whose_sheet_goes_by_the_kind_has_its_kinds_colouring)
{
    MakeUnits m;
    make_units(&m);
    /* Faces are counted from 1, and palettes from 0. */
    m.kinds[600].face = 1;
    m.kinds[601].face = 4;
    Unit *first = put(&m, 1, 1, 3);
    first->kind = 600;
    first->player = 2;
    put(&m, 2, 2, 3)->kind = 601;
    ASSERT_EQ(2u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(0u, m.sprites[0].palette);
    EXPECT_EQ(3u, m.sprites[1].palette);
    /* And not when its sheet goes by the player. */
    m.looks[3].by_player = true;
    ASSERT_EQ(2u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(7u, m.sprites[0].palette);
}

UTEST(units, a_unit_of_a_kind_that_is_not_known_has_the_first_palette)
{
    MakeUnits m;
    make_units(&m);
    put(&m, 1, 1, 3)->kind = 602;
    put(&m, 2, 2, 3)->kind = -1;
    put(&m, 3, 3, 3)->kind = UNIT_KIND_IDS;
    ASSERT_EQ(3u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(0u, m.sprites[0].palette);
    EXPECT_EQ(0u, m.sprites[1].palette);
    EXPECT_EQ(0u, m.sprites[2].palette);
}

UTEST(units, a_unit_of_a_player_the_map_does_not_have_has_the_first_palette)
{
    MakeUnits m;
    make_units(&m);
    m.looks[3].by_player = true;
    put(&m, 1, 1, 3)->player = 0;
    put(&m, 2, 2, 3)->player = 4;
    put(&m, 3, 3, 3)->player = -1;
    ASSERT_EQ(3u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(0u, m.sprites[0].palette);
    EXPECT_EQ(0u, m.sprites[1].palette);
    EXPECT_EQ(0u, m.sprites[2].palette);
    /* Nor does a colour that is no number of a palette pick one. */
    m.players[1].colour = -3;
    units_at(&m.live, 0, NULL)->player = 2;
    ASSERT_EQ(3u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(0u, m.sprites[0].palette);
}

UTEST(units, a_type_that_is_not_known_is_not_drawn)
{
    MakeUnits m;
    make_units(&m);
    put(&m, 1, 1, 4);
    put(&m, 2, 2, 7000);
    put(&m, 5, 5, 3);
    ASSERT_EQ(1u, sprites_for(&m, 0, 0, 320, 320));
    EXPECT_EQ(5 * 32 + 16 - 32, m.sprites[0].x);
}

UTEST(units, a_unit_off_the_screen_is_left_out)
{
    MakeUnits m;
    make_units(&m);
    /* Its cell covers the pixels 112 to 175 across and 136 to 183
     * down. */
    put(&m, 4, 5, 3);
    EXPECT_EQ(0u, sprites_for(&m, 176, 0, 100, 320));
    EXPECT_EQ(1u, sprites_for(&m, 175, 0, 100, 320));
    EXPECT_EQ(0u, sprites_for(&m, 0, 0, 112, 320));
    EXPECT_EQ(1u, sprites_for(&m, 0, 0, 113, 320));
    EXPECT_EQ(0u, sprites_for(&m, 0, 184, 320, 100));
    EXPECT_EQ(1u, sprites_for(&m, 0, 183, 320, 100));
    EXPECT_EQ(0u, sprites_for(&m, 0, 0, 320, 136));
    EXPECT_EQ(1u, sprites_for(&m, 0, 0, 320, 137));
}

UTEST(units, most_is_every_unit_there_is)
{
    MakeUnits m;
    make_units(&m);
    put(&m, 1, 1, 3);
    put(&m, 5, 5, 3);
    put(&m, 8, 8, 4);
    EXPECT_EQ(3u, units_most(&m.live));
    EXPECT_EQ(2u, sprites_for(&m, 0, 0, 320, 320));
}
