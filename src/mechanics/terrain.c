#include "mechanics/terrain.h"

#include <stdlib.h>
#include <string.h>

/* The parts of a square's number: which pair of kinds meets on it,
 * which of four sets of shapes its picture is from, and which shape of
 * the set. Water has numbers of its own. */
#define NUMBER_CLOSED 0x2000u
#define NUMBER_WATER_MASK 0x300u
#define NUMBER_WATER 0x200u
#define PAIRS 16
#define SETS 4
#define SHAPES 14

/* The two kinds of each pair: the one that comes in, and the one it
 * comes into. A pair of two nones is not used by any ground. */
static const uint8_t pair_kinds[PAIRS][2] = {
    {TERRAIN_GRASS, TERRAIN_LAND}, {TERRAIN_CRACKED, TERRAIN_LAND},
    {TERRAIN_SAND, TERRAIN_LAND}, {TERRAIN_SAVANNA, TERRAIN_LAND},
    {TERRAIN_STONES, TERRAIN_LAND}, {TERRAIN_CRACKED, TERRAIN_STONES},
    {TERRAIN_FLOWERS, TERRAIN_SAVANNA}, {TERRAIN_MOUNTAIN, TERRAIN_STONES},
    {TERRAIN_NONE, TERRAIN_NONE}, {TERRAIN_NONE, TERRAIN_NONE},
    {TERRAIN_NONE, TERRAIN_NONE}, {TERRAIN_NONE, TERRAIN_NONE},
    {TERRAIN_ROAD, TERRAIN_LAND}, {TERRAIN_NONE, TERRAIN_NONE},
    {TERRAIN_NONE, TERRAIN_NONE}, {TERRAIN_NONE, TERRAIN_NONE}
};

/* How much of the first kind a shape has, from 1 for none of it to 5
 * for all of it. */
static const uint8_t shape_share[SETS][SHAPES] = {
    {2, 3, 2, 4, 3, 4, 2, 2, 2, 2, 4, 4, 4, 4},
    {3, 5, 3, 3, 1, 3, 2, 4, 2, 2, 4, 2, 4, 4},
    {2, 3, 2, 4, 3, 4, 2, 4, 2, 2, 4, 2, 4, 4},
    {5, 5, 5, 5, 5, 5, 2, 2, 2, 2, 4, 4, 4, 4}
};

bool terrain_of_number(uint16_t number, const TerrainCosts *costs,
                      uint8_t *cost)
{
    uint32_t shape = number & 0xfu;
    uint32_t set = (number >> 4) & 0x3u;
    bool closed = (number & NUMBER_CLOSED) != 0;
    if ((number & NUMBER_WATER_MASK) == NUMBER_WATER) {
        /* Half of water's numbers are no ground at all. */
        *cost = shape < 8 ? TERRAIN_COST_WATER : TERRAIN_COST_NONE;
        return true;
    }
    if (shape >= SHAPES) {
        *cost = TERRAIN_COST_NONE;
        return true;
    }
    const uint8_t *pair = pair_kinds[(number >> 6) & 0xfu];
    uint32_t first = costs->of[pair[0]];
    uint32_t second = costs->of[pair[1]];
    uint32_t share = shape_share[set][shape];
    /* The square is of the kind it has more of, and three or more
     * parts of five are more. */
    uint32_t kind = share > 2 ? pair[0] : pair[1];
    uint32_t mixed = share == 1 ? second
        : share == 2 ? (first + second * 3) >> 2
        : share == 3 ? (first + second) >> 1
        : share == 4 ? (first * 3 + second) >> 2 : first;
    *cost = (uint8_t)(mixed > TERRAIN_COST_NONE ? TERRAIN_COST_NONE : mixed);
    return closed || kind == TERRAIN_MOUNTAIN || kind == TERRAIN_NONE
        || *cost == TERRAIN_COST_NONE;
}

/* Puts a structure on the table. */
static void stand_structure(Terrain *terrain, const MapStructure *structure,
                            const TerrainCosts *costs,
                            const StructureKind *kinds)
{
    int32_t width = 1;
    int32_t depth = 1;
    uint32_t present = 1;
    uint32_t closed = 1;
    if (structure->type < STRUCTURE_KINDS
        && kinds[structure->type].width != 0) {
        const StructureKind *kind = &kinds[structure->type];
        width = kind->width;
        depth = kind->depth;
        present = kind->present;
        closed = kind->closed;
        if (kind->pieces && structure->bridge_width > 0
            && structure->bridge_height > 0) {
            width = structure->bridge_width;
            depth = structure->bridge_height;
            present = UINT32_MAX;
            closed = 0;
        }
    }
    for (int32_t row = 0; row < depth; row++) {
        for (int32_t column = 0; column < width; column++) {
            uint32_t bit = (uint32_t)(row * width + column) & 31u;
            int32_t x = structure->x + column;
            int32_t y = structure->y + row;
            if ((present >> bit & 1u) == 0 || x < 0 || y < 0
                || (uint32_t)x >= terrain->width
                || (uint32_t)y >= terrain->height) {
                continue;
            }
            size_t at = (size_t)y * terrain->width + (size_t)x;
            if ((closed >> bit & 1u) != 0) {
                terrain->closed[at] |= TERRAIN_NO_WALK | TERRAIN_NO_HOVER;
            } else {
                terrain->closed[at] &= (uint8_t)~(TERRAIN_NO_WALK
                    | TERRAIN_NO_HOVER);
                terrain->cost[at] = costs->of[TERRAIN_CRACKED];
            }
        }
    }
}

bool terrain_from_map(const Map *map, const TerrainCosts *costs,
                     const StructureKind *kinds, Terrain *out)
{
    Terrain terrain;
    size_t squares = (size_t)map->info.width * map->info.height;
    terrain.width = map->info.width;
    terrain.height = map->info.height;
    terrain.cost = malloc(squares > 0 ? squares : 1);
    terrain.closed = calloc(squares > 0 ? squares : 1, 1);
    if (terrain.cost == NULL || terrain.closed == NULL) {
        free(terrain.cost);
        free(terrain.closed);
        return false;
    }
    for (size_t i = 0; i < squares; i++) {
        if (terrain_of_number(map->ground[i], costs, &terrain.cost[i])) {
            terrain.closed[i] = TERRAIN_NO_WALK;
        }
        if (map->objects[i] != 0) {
            terrain.closed[i] |= TERRAIN_NO_WALK | TERRAIN_NO_HOVER;
        }
    }
    for (uint32_t i = 0; i < map->structure_count; i++) {
        stand_structure(&terrain, &map->structures[i], costs, kinds);
    }
    /* The edge last: nothing opens it. */
    for (uint32_t y = 0; y < terrain.height; y++) {
        for (uint32_t x = 0; x < terrain.width; x++) {
            if (x < MAP_EDGE || y < MAP_EDGE || x + MAP_EDGE >= terrain.width
                || y + MAP_EDGE >= terrain.height) {
                terrain.closed[(size_t)y * terrain.width + x] = TERRAIN_NO_ONE;
            }
        }
    }
    *out = terrain;
    return true;
}

void terrain_free(Terrain *terrain)
{
    free(terrain->cost);
    free(terrain->closed);
    memset(terrain, 0, sizeof *terrain);
}
