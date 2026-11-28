/* The ground of a map as units meet it: for each square, how slow it
 * is to cross and what it is closed to.
 *
 * The rules are the original's. A square's number in the map says
 * which kinds of ground meet on it and how much of each there is; its
 * cost is that of the one kind or a mean of the two, and it is closed
 * to what walks if it is water, mountain or marked closed. A tree or
 * a rock closes its square. A structure closes the squares of its base
 * that its kind says so of and opens its others, whatever was under
 * them: a doorway, the deck of a bridge over water. Along each edge of
 * a map MAP_EDGE squares are closed to everything.
 *
 * The table is made when a map opens and changes only when a structure
 * rises or falls. It knows nothing of units.
 */
#ifndef ROM2_MECHANICS_TERRAIN_H
#define ROM2_MECHANICS_TERRAIN_H

#include <stdbool.h>
#include <stdint.h>

#include "data/terrain_costs.h"
#include "data/map.h"
#include "data/structure_kind.h"

/* What a square is closed to: what walks, what hovers over the ground
 * and so crosses water, and what flies. */
#define TERRAIN_NO_WALK 1u
#define TERRAIN_NO_HOVER 2u
#define TERRAIN_NO_FLY 4u
#define TERRAIN_NO_ONE (TERRAIN_NO_WALK | TERRAIN_NO_HOVER | TERRAIN_NO_FLY)

/* The cost of water, which the original gives no kind's cost but this
 * number. Only what does not walk crosses it. */
#define TERRAIN_COST_WATER 8

/* The table. A value for each square, row after row from the top, as a
 * map has them. Owned. */
typedef struct Terrain {
    uint32_t width;         /* in squares */
    uint32_t height;
    uint8_t *cost;          /* TERRAIN_COST_NONE where nothing crosses */
    uint8_t *closed;        /* TERRAIN_NO_ bits */
} Terrain;

/* What a square's number in a map says of it: `*cost` gets its cost,
 * and it returns whether the square is closed to what walks. A number
 * that names no ground the game knows has TERRAIN_COST_NONE and is
 * closed. */
bool terrain_of_number(uint16_t number, const TerrainCosts *costs,
                      uint8_t *cost);

/* Makes the table of a map. `kinds` has STRUCTURE_KINDS entries, the
 * kind with each ID, all zeros for one that is not known: a structure
 * of such a kind closes the one square it is placed on.
 *
 * A bridge of pieces covers the squares its record in the map says and
 * opens them all. The floor of a structure costs as cracked ground
 * does. Of a base of more than 32 squares the bits are used again from
 * the first, as the original uses them.
 *
 * Returns false, and leaves `*out` alone, if memory runs out. */
bool terrain_from_map(const Map *map, const TerrainCosts *costs,
                     const StructureKind *kinds, Terrain *out);

/* Frees the table and sets it to none. */
void terrain_free(Terrain *terrain);

#endif
