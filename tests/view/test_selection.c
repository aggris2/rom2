#include "view/selection.h"

#include <string.h>

#include "utest.h"
#include "view/frame.h"

/* A flat map of 10 by 10 squares and one type of unit, type 3, whose
 * selection box is 32 wide and 42 high around its anchor, on a screen
 * of 640 by 480 that shows the map from its corner. */
typedef struct MakeSelection {
    Map map;
    uint8_t heights[100];
    Units live;
    UnitSlot slots[4];
    UnitLook looks[UNIT_LOOKS];
    Selection selection;
    uint32_t chosen[UNITS_MOST + 1];
    Camera camera;
    Sprite sprites[16];
} MakeSelection;

static void make_selection(MakeSelection *m)
{
    memset(m, 0, sizeof *m);
    m->map.info.width = 10;
    m->map.info.height = 10;
    m->map.heights = m->heights;
    m->live.slots = m->slots;
    m->live.room = 4;
    m->looks[3].known = true;
    m->looks[3].selection = (SpriteBox){-16, -30, 16, 12};
    m->selection.chosen = m->chosen;
    m->camera = (Camera){0, 0, 640, 480};
}

/* Puts a unit of a player in the middle of the square (x, y) and
 * returns its id. One on (4, 5) stands at (144, 176): its box is 128
 * to 159 across and 146 to 187 down. */
static UnitId put(MakeSelection *m, int x, int y, int32_t player)
{
    Unit unit;
    UnitId id = {9999, 9999};
    memset(&unit, 0, sizeof unit);
    unit.type = 3;
    unit.player = player;
    unit.x = x * 256 + 128;
    unit.y = y * 256 + 128;
    units_add(&m->live, &unit, &id);
    return id;
}

/* Selects with a box between two points, for a player. */
static bool box(MakeSelection *m, int32_t from_x, int32_t from_y,
                int32_t to_x, int32_t to_y, int32_t player, bool toggle)
{
    SelectionDrag drag = {from_x, from_y, to_x, to_y};
    return selection_box(&m->selection, &m->map, &m->live, m->looks, 0,
        &m->camera, &drag, player, toggle);
}

UTEST(selection, a_unit_that_is_put_in_is_selected_until_it_is_let_go)
{
    static MakeSelection m;
    make_selection(&m);
    UnitId one = put(&m, 1, 1, 1);
    UnitId other = put(&m, 3, 1, 1);
    EXPECT_FALSE(selection_has(&m.selection, one));
    selection_put(&m.selection, one, true);
    EXPECT_TRUE(selection_has(&m.selection, one));
    EXPECT_FALSE(selection_has(&m.selection, other));
    EXPECT_EQ(1u, selection_count(&m.selection, &m.live));
    selection_put(&m.selection, other, true);
    EXPECT_EQ(2u, selection_count(&m.selection, &m.live));
    selection_put(&m.selection, one, false);
    EXPECT_FALSE(selection_has(&m.selection, one));
    EXPECT_TRUE(selection_has(&m.selection, other));
    selection_clear(&m.selection);
    EXPECT_EQ(0u, selection_count(&m.selection, &m.live));
    /* Nobody is selected where there is no selection. */
    EXPECT_FALSE(selection_has(NULL, one));
}

UTEST(selection, the_unit_that_takes_the_slot_of_one_selected_is_not)
{
    static MakeSelection m;
    make_selection(&m);
    UnitId gone = put(&m, 1, 1, 1);
    selection_put(&m.selection, gone, true);
    units_remove(&m.live, gone);
    EXPECT_EQ(0u, selection_count(&m.selection, &m.live));
    UnitId next = put(&m, 1, 1, 1);
    ASSERT_EQ(gone.slot, next.slot);
    EXPECT_FALSE(selection_has(&m.selection, next));
    /* Letting go of the one that is gone does not let go of this. */
    selection_put(&m.selection, next, true);
    selection_put(&m.selection, gone, false);
    EXPECT_TRUE(selection_has(&m.selection, next));
}

UTEST(selection, it_is_the_players_to_command_if_its_first_unit_is_his)
{
    static MakeSelection m;
    make_selection(&m);
    UnitId theirs = put(&m, 1, 1, 2);
    UnitId mine = put(&m, 3, 1, 1);
    /* Nobody is selected. */
    EXPECT_FALSE(selection_commands(&m.selection, &m.live, 1));
    selection_put(&m.selection, mine, true);
    EXPECT_TRUE(selection_commands(&m.selection, &m.live, 1));
    EXPECT_FALSE(selection_commands(&m.selection, &m.live, 2));
    /* The one in the lower slot decides. */
    selection_put(&m.selection, theirs, true);
    EXPECT_FALSE(selection_commands(&m.selection, &m.live, 1));
    EXPECT_TRUE(selection_commands(&m.selection, &m.live, 2));
}

UTEST(selection, a_drag_longer_than_a_64th_of_the_width_is_a_box)
{
    Camera camera = {0, 0, 640, 480};
    SelectionDrag drag = {100, 100, 110, 110};
    EXPECT_FALSE(selection_drag_is_box(&drag, &camera));
    drag.to_x = 111;
    EXPECT_TRUE(selection_drag_is_box(&drag, &camera));
    /* Down is enough, and so is the other way. */
    drag = (SelectionDrag){100, 100, 100, 89};
    EXPECT_TRUE(selection_drag_is_box(&drag, &camera));
    /* A wider screen takes a longer drag. */
    camera.width = 1920;
    drag = (SelectionDrag){100, 100, 130, 100};
    EXPECT_FALSE(selection_drag_is_box(&drag, &camera));
    drag.to_x = 131;
    EXPECT_TRUE(selection_drag_is_box(&drag, &camera));
}

UTEST(selection, a_box_selects_the_units_it_covers_and_lets_go_of_others)
{
    static MakeSelection m;
    make_selection(&m);
    UnitId left = put(&m, 4, 5, 1);
    UnitId right = put(&m, 6, 5, 1);
    UnitId far = put(&m, 8, 8, 1);
    selection_put(&m.selection, far, true);
    EXPECT_TRUE(box(&m, 120, 140, 230, 190, 1, false));
    EXPECT_TRUE(selection_has(&m.selection, left));
    EXPECT_TRUE(selection_has(&m.selection, right));
    EXPECT_FALSE(selection_has(&m.selection, far));
    /* Drawn from the other corner it is the same box. */
    selection_clear(&m.selection);
    EXPECT_TRUE(box(&m, 180, 190, 120, 140, 1, false));
    EXPECT_TRUE(selection_has(&m.selection, left));
    EXPECT_FALSE(selection_has(&m.selection, right));
}

UTEST(selection, a_box_takes_a_unit_it_shares_more_than_ten_pixels_with)
{
    static MakeSelection m;
    make_selection(&m);
    UnitId id = put(&m, 4, 5, 1);
    /* The corner of his box is at (128, 146): 2 by 5 pixels of it. */
    EXPECT_FALSE(box(&m, 100, 100, 130, 151, 1, false));
    EXPECT_FALSE(selection_has(&m.selection, id));
    /* 2 by 6. */
    EXPECT_TRUE(box(&m, 100, 100, 130, 152, 1, false));
    EXPECT_TRUE(selection_has(&m.selection, id));
}

UTEST(selection, a_box_takes_only_the_units_of_the_player)
{
    static MakeSelection m;
    make_selection(&m);
    UnitId mine = put(&m, 4, 5, 1);
    UnitId theirs = put(&m, 6, 5, 2);
    EXPECT_TRUE(box(&m, 120, 140, 230, 190, 1, false));
    EXPECT_TRUE(selection_has(&m.selection, mine));
    EXPECT_FALSE(selection_has(&m.selection, theirs));
    /* For no player in particular it takes them all. */
    EXPECT_TRUE(box(&m, 120, 140, 230, 190, 0, false));
    EXPECT_TRUE(selection_has(&m.selection, mine));
    EXPECT_TRUE(selection_has(&m.selection, theirs));
}

UTEST(selection, a_box_that_catches_nobody_changes_nothing)
{
    static MakeSelection m;
    make_selection(&m);
    UnitId mine = put(&m, 4, 5, 1);
    UnitId theirs = put(&m, 6, 5, 2);
    selection_put(&m.selection, mine, true);
    /* On empty ground, and on another player's unit alone. */
    EXPECT_FALSE(box(&m, 300, 300, 400, 400, 1, false));
    EXPECT_FALSE(box(&m, 190, 140, 230, 190, 1, false));
    EXPECT_TRUE(selection_has(&m.selection, mine));
    EXPECT_FALSE(selection_has(&m.selection, theirs));
}

UTEST(selection, a_box_that_toggles_turns_each_unit_it_catches_over)
{
    static MakeSelection m;
    make_selection(&m);
    UnitId left = put(&m, 4, 5, 1);
    UnitId right = put(&m, 6, 5, 1);
    UnitId far = put(&m, 8, 8, 1);
    UnitId theirs = put(&m, 5, 5, 2);
    selection_put(&m.selection, left, true);
    selection_put(&m.selection, far, true);
    EXPECT_TRUE(box(&m, 120, 140, 230, 190, 1, true));
    EXPECT_FALSE(selection_has(&m.selection, left));
    EXPECT_TRUE(selection_has(&m.selection, right));
    /* The others stay as they were. */
    EXPECT_TRUE(selection_has(&m.selection, far));
    EXPECT_FALSE(selection_has(&m.selection, theirs));
}

UTEST(selection, a_unit_that_is_not_drawn_is_not_caught)
{
    static MakeSelection m;
    make_selection(&m);
    Unit *odd = units_get(&m.live, put(&m, 4, 5, 1));
    odd->type = 8;
    EXPECT_FALSE(box(&m, 0, 0, 600, 400, 1, false));
}

UTEST(selection, the_outline_is_a_white_line_along_each_side_of_the_box)
{
    static MakeSelection m;
    make_selection(&m);
    SelectionDrag drag = {50, 90, 20, 30};
    ASSERT_EQ(4u, selection_outline(7, &drag, &m.camera, m.sprites));
    const int places[4][4] = {{20, 30, 31, 1}, {20, 90, 31, 1},
        {20, 30, 1, 61}, {50, 30, 1, 61}};
    for (int i = 0; i < 4; i++) {
        const Sprite *sprite = &m.sprites[i];
        EXPECT_EQ(7u, sprite->sheet);
        EXPECT_EQ(places[i][0], sprite->x);
        EXPECT_EQ(places[i][1], sprite->y);
        EXPECT_EQ((unsigned)places[i][2], sprite->width);
        EXPECT_EQ((unsigned)places[i][3], sprite->height);
        EXPECT_EQ((unsigned)FRAME_LIGHT_WHOLE, sprite->light);
        EXPECT_EQ((unsigned)SPRITE_ORDER_BOX, sprite->order);
    }
}

UTEST(selection, a_side_longer_than_the_sheet_is_made_of_several_sprites)
{
    static MakeSelection m;
    make_selection(&m);
    /* 601 across: two of 256 and one of 89, on each of two sides. */
    SelectionDrag drag = {10, 10, 610, 20};
    size_t count = selection_outline(7, &drag, &m.camera, m.sprites);
    ASSERT_EQ(8u, count);
    EXPECT_TRUE(count <= selection_outline_most(&m.camera));
    EXPECT_EQ(256u, m.sprites[0].width);
    EXPECT_EQ(266, m.sprites[1].x);
    EXPECT_EQ(89u, m.sprites[2].width);
    EXPECT_EQ(522, m.sprites[2].x);
}

UTEST(selection, the_outline_is_kept_to_the_screen)
{
    static MakeSelection m;
    make_selection(&m);
    SelectionDrag drag = {-20, 400, 100, 900};
    ASSERT_EQ(4u, selection_outline(7, &drag, &m.camera, m.sprites));
    EXPECT_EQ(0, m.sprites[0].x);
    EXPECT_EQ(101u, m.sprites[0].width);
    EXPECT_EQ(479, m.sprites[1].y);
    EXPECT_EQ(80u, m.sprites[2].height);
}
