/* How slow each kind of ground is to walk on: its cost. A unit crosses
 * ground of cost 8 at its own speed, ground of a higher cost slower
 * and of a lower cost faster.
 *
 * The costs are the [Terrain] part of `rules/map.toml`, which has them
 * under the names the original gives them: CostLand, CostRoad and so
 * on.
 */
#ifndef ROM2_DATA_TERRAIN_COSTS_H
#define ROM2_DATA_TERRAIN_COSTS_H

#include <stdbool.h>
#include <stdint.h>

#include "base/toml.h"

/* The kinds of ground, by the numbers the original knows them by. */
typedef enum TerrainKind {
    TERRAIN_NONE,       /* no ground at all: nothing crosses it */
    TERRAIN_LAND,
    TERRAIN_GRASS,
    TERRAIN_FLOWERS,
    TERRAIN_SAND,
    TERRAIN_CRACKED,
    TERRAIN_STONES,
    TERRAIN_SAVANNA,
    TERRAIN_MOUNTAIN,
    TERRAIN_WATER,
    TERRAIN_ROAD,
    TERRAIN_KINDS
} TerrainKind;

/* The cost of what cannot be crossed, and the most a kind may cost. */
#define TERRAIN_COST_NONE 255
#define TERRAIN_COST_MOST 254

typedef struct TerrainCosts {
    uint8_t of[TERRAIN_KINDS];   /* TERRAIN_COST_NONE for TERRAIN_NONE */
} TerrainCosts;

/* Reads the costs from the table of the top of `rules/map.toml`.
 *
 * Returns false, and leaves `*out` alone, if there is no [Terrain]
 * table, it lacks the cost of a kind, or a cost is not from 1 to
 * TERRAIN_COST_MOST. */
bool terrain_costs_read(const TomlTable *top, TerrainCosts *out);

#endif
