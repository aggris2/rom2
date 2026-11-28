#include "mechanics/terrain.h"

#include <string.h>

#include "utest.h"

/* The costs the tests go by. */
static TerrainCosts some_costs(void)
{
    TerrainCosts costs = {{TERRAIN_COST_NONE, 8, 10, 9, 14, 6, 12, 11, 16,
        8, 4}};
    return costs;
}

/* A square's number: a pair of kinds, a set of shapes and a shape. */
static uint16_t number_of(unsigned pair, unsigned set, unsigned shape)
{
    return (uint16_t)(pair << 6 | set << 4 | shape);
}

/* The cost of a number, and into `*closed` whether it is closed. */
static unsigned cost_of(uint16_t number, bool *closed)
{
    TerrainCosts costs = some_costs();
    uint8_t cost = 77;
    *closed = terrain_of_number(number, &costs, &cost);
    return cost;
}

UTEST(terrain, a_square_all_of_one_kind_has_that_kinds_cost)
{
    bool closed = true;
    /* Pair 0 is grass coming into land. Shape 4 of set 1 has none of
     * the grass, and shape 1 of set 1 all of it. */
    EXPECT_EQ(8u, cost_of(number_of(0, 1, 4), &closed));
    EXPECT_FALSE(closed);
    EXPECT_EQ(10u, cost_of(number_of(0, 1, 1), &closed));
    EXPECT_FALSE(closed);
    /* Pair 12 is a road on land. */
    EXPECT_EQ(4u, cost_of(number_of(12, 3, 0), &closed));
    EXPECT_FALSE(closed);
}

UTEST(terrain, a_square_of_two_kinds_has_a_mean_of_their_costs)
{
    bool closed = true;
    /* Sand, 14, into land, 8: a quarter, a half and three quarters of
     * the sand. */
    EXPECT_EQ((14u + 8u * 3u) / 4u, cost_of(number_of(2, 0, 0), &closed));
    EXPECT_EQ((14u + 8u) / 2u, cost_of(number_of(2, 0, 1), &closed));
    EXPECT_EQ((14u * 3u + 8u) / 4u, cost_of(number_of(2, 0, 3), &closed));
    EXPECT_FALSE(closed);
    /* Cut down to a whole number: cracked, 6, into stones, 12. */
    EXPECT_EQ(10u, cost_of(number_of(5, 0, 0), &closed));
    EXPECT_EQ(7u, cost_of(number_of(5, 0, 3), &closed));
}

UTEST(terrain, a_mountain_is_closed_where_there_is_more_of_it)
{
    bool closed = false;
    /* Pair 7 is mountain coming into stones. */
    EXPECT_EQ(16u, cost_of(number_of(7, 3, 0), &closed));
    EXPECT_TRUE(closed);
    EXPECT_EQ(14u, cost_of(number_of(7, 0, 1), &closed));
    EXPECT_TRUE(closed);
    EXPECT_EQ(15u, cost_of(number_of(7, 0, 3), &closed));
    EXPECT_TRUE(closed);
    /* A quarter of mountain, or none, is stones to walk on. */
    EXPECT_EQ(13u, cost_of(number_of(7, 0, 0), &closed));
    EXPECT_FALSE(closed);
    EXPECT_EQ(12u, cost_of(number_of(7, 1, 4), &closed));
    EXPECT_FALSE(closed);
}

UTEST(terrain, water_is_closed_to_what_walks)
{
    bool closed = false;
    for (unsigned pair = 8; pair < 12; pair++) {
        EXPECT_EQ((unsigned)TERRAIN_COST_WATER,
            cost_of(number_of(pair, 0, 3), &closed));
        EXPECT_TRUE(closed);
        EXPECT_EQ((unsigned)TERRAIN_COST_WATER,
            cost_of(number_of(pair, 3, 7), &closed));
        EXPECT_TRUE(closed);
        /* Its higher shapes are no ground at all. */
        EXPECT_EQ((unsigned)TERRAIN_COST_NONE,
            cost_of(number_of(pair, 0, 8), &closed));
        EXPECT_TRUE(closed);
    }
}

UTEST(terrain, a_number_marked_closed_is_closed_whatever_it_is)
{
    bool closed = false;
    EXPECT_EQ(8u, cost_of(number_of(0, 1, 4) | 0x2000u, &closed));
    EXPECT_TRUE(closed);
    /* The other high bits say nothing of it. */
    EXPECT_EQ(8u, cost_of(number_of(0, 1, 4) | 0xd000u, &closed));
    EXPECT_FALSE(closed);
}

UTEST(terrain, a_number_that_names_no_ground_cannot_be_crossed)
{
    bool closed = false;
    /* A shape past the last, and a pair no ground uses. */
    EXPECT_EQ((unsigned)TERRAIN_COST_NONE,
        cost_of(number_of(0, 0, 14), &closed));
    EXPECT_TRUE(closed);
    EXPECT_EQ((unsigned)TERRAIN_COST_NONE,
        cost_of(number_of(0, 0, 15), &closed));
    EXPECT_TRUE(closed);
    closed = false;
    EXPECT_EQ((unsigned)TERRAIN_COST_NONE,
        cost_of(number_of(13, 0, 1), &closed));
    EXPECT_TRUE(closed);
}

/* A map of 24 by 22 squares of land, with room for objects and a few
 * structures, and the kinds of structure the tests put on it. */
typedef struct MakeTerrain {
    Map map;
    uint16_t ground[24 * 22];
    uint8_t objects[24 * 22];
    uint8_t heights[24 * 22];
    MapStructure structures[4];
    StructureKind kinds[STRUCTURE_KINDS];
    Terrain terrain;
} MakeTerrain;

#define LAND 0x14u      /* pair 0, set 1, shape 4: all land */
#define WATER 0x203u

static void make_terrain(MakeTerrain *m)
{
    memset(m, 0, sizeof *m);
    m->map.info.width = 24;
    m->map.info.height = 22;
    m->map.ground = m->ground;
    m->map.objects = m->objects;
    m->map.heights = m->heights;
    m->map.structures = m->structures;
    for (int i = 0; i < 24 * 22; i++) {
        m->ground[i] = LAND;
    }
    /* Kind 1 is a hut of 3 by 2 with a doorway in its front left
     * square: the registry's 63 and 55. */
    m->kinds[1].width = 3;
    m->kinds[1].depth = 2;
    m->kinds[1].present = 63;
    m->kinds[1].closed = 55;
    /* Kind 33 is a bridge of pieces. */
    m->kinds[33].width = 1;
    m->kinds[33].depth = 1;
    m->kinds[33].present = 1;
    m->kinds[33].closed = 1;
    m->kinds[33].pieces = true;
}

static MapStructure *put(MakeTerrain *m, int x, int y, uint32_t type)
{
    MapStructure *structure = &m->structures[m->map.structure_count++];
    structure->x = x;
    structure->y = y;
    structure->type = type;
    return structure;
}

/* Makes the table and gives what the square (x, y) is closed to; the
 * table stays in `m` for a look at more of it. */
static unsigned closed_at(MakeTerrain *m, int x, int y)
{
    TerrainCosts costs = some_costs();
    terrain_free(&m->terrain);
    if (!terrain_from_map(&m->map, &costs, m->kinds, &m->terrain)) {
        return 9999;
    }
    return m->terrain.closed[y * 24 + x];
}

static unsigned cost_at(const MakeTerrain *m, int x, int y)
{
    return m->terrain.cost[y * 24 + x];
}

UTEST(terrain, open_land_is_open_to_all_and_costs_as_land)
{
    static MakeTerrain m;
    make_terrain(&m);
    EXPECT_EQ(0u, closed_at(&m, 10, 10));
    EXPECT_EQ(8u, cost_at(&m, 10, 10));
    EXPECT_EQ(24u, m.terrain.width);
    EXPECT_EQ(22u, m.terrain.height);
    terrain_free(&m.terrain);
    EXPECT_TRUE(m.terrain.cost == NULL);
}

UTEST(terrain, a_square_keeps_the_height_the_map_gives_it)
{
    static MakeTerrain m;
    make_terrain(&m);
    m.heights[10 * 24 + 9] = 77;
    m.heights[0] = 3;
    EXPECT_EQ(0u, closed_at(&m, 9, 10));
    EXPECT_EQ(77u, m.terrain.raised[10 * 24 + 9]);
    EXPECT_EQ(3u, m.terrain.raised[0]);
    EXPECT_EQ(0u, m.terrain.raised[10 * 24 + 10]);
    terrain_free(&m.terrain);
}

UTEST(terrain, the_edge_of_a_map_is_closed_to_everything)
{
    static MakeTerrain m;
    make_terrain(&m);
    EXPECT_EQ((unsigned)TERRAIN_NO_ONE, closed_at(&m, 0, 10));
    EXPECT_EQ((unsigned)TERRAIN_NO_ONE, closed_at(&m, 7, 10));
    EXPECT_EQ(0u, closed_at(&m, 8, 10));
    EXPECT_EQ(0u, closed_at(&m, 15, 10));
    EXPECT_EQ((unsigned)TERRAIN_NO_ONE, closed_at(&m, 16, 10));
    EXPECT_EQ((unsigned)TERRAIN_NO_ONE, closed_at(&m, 23, 10));
    EXPECT_EQ((unsigned)TERRAIN_NO_ONE, closed_at(&m, 10, 7));
    EXPECT_EQ(0u, closed_at(&m, 10, 8));
    EXPECT_EQ(0u, closed_at(&m, 10, 13));
    EXPECT_EQ((unsigned)TERRAIN_NO_ONE, closed_at(&m, 10, 14));
    terrain_free(&m.terrain);
}

UTEST(terrain, water_stops_what_walks_and_a_tree_what_hovers_too)
{
    static MakeTerrain m;
    make_terrain(&m);
    m.ground[10 * 24 + 9] = WATER;
    m.objects[10 * 24 + 11] = 5;
    EXPECT_EQ((unsigned)TERRAIN_NO_WALK, closed_at(&m, 9, 10));
    EXPECT_EQ((unsigned)(TERRAIN_NO_WALK | TERRAIN_NO_HOVER),
        closed_at(&m, 11, 10));
    EXPECT_EQ(0u, closed_at(&m, 10, 10));
    terrain_free(&m.terrain);
}

UTEST(terrain, a_structure_closes_its_base_but_for_its_floor)
{
    static MakeTerrain m;
    make_terrain(&m);
    put(&m, 9, 9, 1);
    unsigned walls = TERRAIN_NO_WALK | TERRAIN_NO_HOVER;
    EXPECT_EQ(walls, closed_at(&m, 9, 9));
    EXPECT_EQ(walls, closed_at(&m, 10, 9));
    EXPECT_EQ(walls, closed_at(&m, 11, 9));
    /* The doorway: open, and it costs as cracked ground. */
    EXPECT_EQ(0u, closed_at(&m, 9, 10));
    EXPECT_EQ(6u, cost_at(&m, 9, 10));
    EXPECT_EQ(walls, closed_at(&m, 10, 10));
    EXPECT_EQ(walls, closed_at(&m, 11, 10));
    EXPECT_EQ(0u, closed_at(&m, 12, 10));
    EXPECT_EQ(0u, closed_at(&m, 9, 11));
    EXPECT_EQ(8u, cost_at(&m, 12, 10));
    terrain_free(&m.terrain);
}

UTEST(terrain, a_floor_opens_what_was_closed_under_it)
{
    static MakeTerrain m;
    make_terrain(&m);
    /* A doorway on water with a tree in it. */
    m.ground[10 * 24 + 9] = WATER;
    m.objects[10 * 24 + 9] = 5;
    put(&m, 9, 9, 1);
    EXPECT_EQ(0u, closed_at(&m, 9, 10));
    EXPECT_EQ(6u, cost_at(&m, 9, 10));
    terrain_free(&m.terrain);
}

UTEST(terrain, a_square_a_structure_is_not_on_is_left_as_it_was)
{
    static MakeTerrain m;
    make_terrain(&m);
    /* It is on its back row alone. */
    m.kinds[1].present = 7;
    m.kinds[1].closed = 63;
    m.ground[10 * 24 + 10] = WATER;
    put(&m, 9, 9, 1);
    EXPECT_EQ((unsigned)(TERRAIN_NO_WALK | TERRAIN_NO_HOVER),
        closed_at(&m, 10, 9));
    EXPECT_EQ(0u, closed_at(&m, 9, 10));
    EXPECT_EQ(8u, cost_at(&m, 9, 10));
    EXPECT_EQ((unsigned)TERRAIN_NO_WALK, closed_at(&m, 10, 10));
    terrain_free(&m.terrain);
}

UTEST(terrain, a_bridge_of_pieces_opens_every_square_it_covers)
{
    static MakeTerrain m;
    make_terrain(&m);
    for (int y = 9; y < 13; y++) {
        for (int x = 9; x < 12; x++) {
            m.ground[y * 24 + x] = WATER;
        }
    }
    MapStructure *bridge = put(&m, 10, 9, 33);
    bridge->bridge_width = 1;
    bridge->bridge_height = 4;
    EXPECT_EQ((unsigned)TERRAIN_NO_WALK, closed_at(&m, 9, 10));
    for (int y = 9; y < 13; y++) {
        EXPECT_EQ(0u, closed_at(&m, 10, y));
        EXPECT_EQ(6u, cost_at(&m, 10, y));
    }
    EXPECT_EQ((unsigned)TERRAIN_NO_WALK, closed_at(&m, 11, 12));
    /* One the map gives no size closes its square, as its kind says. */
    bridge->bridge_width = 0;
    EXPECT_EQ((unsigned)(TERRAIN_NO_WALK | TERRAIN_NO_HOVER),
        closed_at(&m, 10, 9));
    EXPECT_EQ((unsigned)TERRAIN_NO_WALK, closed_at(&m, 10, 10));
    terrain_free(&m.terrain);
}

UTEST(terrain, a_structure_of_no_known_kind_closes_its_one_square)
{
    static MakeTerrain m;
    make_terrain(&m);
    put(&m, 9, 9, 200);
    put(&m, 12, 12, 700);
    unsigned walls = TERRAIN_NO_WALK | TERRAIN_NO_HOVER;
    EXPECT_EQ(walls, closed_at(&m, 9, 9));
    EXPECT_EQ(0u, closed_at(&m, 10, 9));
    EXPECT_EQ(walls, closed_at(&m, 12, 12));
    terrain_free(&m.terrain);
}

UTEST(terrain, nothing_opens_the_edge_and_nothing_is_written_past_it)
{
    static MakeTerrain m;
    make_terrain(&m);
    /* A hut with all of its base a floor, half over the edge and
     * partly off the map. */
    m.kinds[1].closed = 0;
    put(&m, 7, 9, 1);
    put(&m, 22, 21, 1);
    put(&m, -2, -1, 1);
    EXPECT_EQ((unsigned)TERRAIN_NO_ONE, closed_at(&m, 7, 9));
    EXPECT_EQ(0u, closed_at(&m, 8, 9));
    EXPECT_EQ(6u, cost_at(&m, 8, 9));
    EXPECT_EQ((unsigned)TERRAIN_NO_ONE, closed_at(&m, 23, 21));
    EXPECT_EQ((unsigned)TERRAIN_NO_ONE, closed_at(&m, 0, 0));
    terrain_free(&m.terrain);
}

UTEST(terrain, a_base_of_more_than_32_squares_uses_its_bits_again)
{
    static MakeTerrain m;
    make_terrain(&m);
    /* 7 by 5: the square 32, (4, 4), goes by bit 0 again, which is
     * here the one floor. */
    m.kinds[2].width = 7;
    m.kinds[2].depth = 5;
    m.kinds[2].present = 0xffffffffu;
    m.kinds[2].closed = 0xfffffffeu;
    put(&m, 8, 8, 2);
    EXPECT_EQ(0u, closed_at(&m, 8, 8));
    EXPECT_EQ(0u, closed_at(&m, 12, 12));
    EXPECT_EQ((unsigned)(TERRAIN_NO_WALK | TERRAIN_NO_HOVER),
        closed_at(&m, 13, 12));
    terrain_free(&m.terrain);
}
