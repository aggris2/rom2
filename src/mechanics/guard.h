/* Guarding: what the units the computer plays do when nobody has told
 * them anything else, by the rule of the original game.
 *
 * The units a map puts in one group are a band. A band watches the
 * ground around it. When one of its units sees an enemy that is near
 * enough to the band, each unit of the band goes for the enemy
 * nearest to it; when there is none, each goes back to the square it
 * came from.
 */
#ifndef ROM2_MECHANICS_GUARD_H
#define ROM2_MECHANICS_GUARD_H

#include <stdbool.h>
#include <stdint.h>

#include "mechanics/field.h"
#include "mechanics/random.h"
#include "mechanics/relations.h"
#include "mechanics/terrain.h"
#include "mechanics/units.h"

/* The bands look around once in this many ticks, as in the
 * original. */
#define GUARD_TURN_TICKS 16

/* How far from its middle a band guards is worked out from where its
 * units stand. One that comes out at less than GUARD_RANGE_LEAST, the
 * original's MinimalGuardRange, guards GUARD_RANGE_WIDE instead. And
 * every band guards GUARD_RANGE_MORE further than that, give or take
 * a square. */
#define GUARD_RANGE_LEAST 8
#define GUARD_RANGE_WIDE 20
#define GUARD_RANGE_MORE 4

/* A band: the units of one player that a map put in one group. */
typedef struct GuardBand {
    int32_t player;
    int32_t group;
    uint8_t range;          /* how far from its middle it guards */
} GuardBand;

/* The bands of a game, and room to work in. All of it is owned. */
typedef struct Guard {
    GuardBand *bands;
    uint32_t count;
    uint32_t ticks;         /* since the bands last looked around */
    uint32_t width;         /* of the ground, in squares */
    uint32_t height;
    uint8_t *seen;          /* what a band sees, a square to an entry */
    /* Which players have a unit in each part of the ground, a bit to
     * a player: the parts are squares of GUARD_PART a side. */
    uint16_t *present;
    uint32_t parts_wide;
    uint32_t parts_high;
    /* The slots of the units that guard, band after band, and where
     * each band's begin; and those of the enemies a band has found. */
    uint32_t *first;
    uint16_t *members;
    uint16_t *found;
} Guard;

/* The side of a part of the ground, in squares. */
#define GUARD_PART 16

/* Makes the bands of the units there are: every unit the computer
 * plays that is in a group joins the band of its player and its
 * group, and the square it is on is the one it guards from. The dice
 * are thrown once for each band.
 *
 * A band's range is the most, over its units, of how far the unit is
 * from the band's middle plus how far it sees; then as the numbers
 * above say. The middle is the mean of the units' squares, and
 * far is counted in squares across or down, whichever is more.
 *
 * Returns false, and leaves `*out` and the units alone, if memory
 * runs out. */
bool guard_make(Units *units, const Terrain *terrain, Random *random,
                Guard *out);

/* Gives it back. One that is all zeros is allowed. */
void guard_free(Guard *guard);

/* One tick: every GUARD_TURN_TICKS ticks, the first of them at once,
 * the bands look around as guard_turn() says. */
void guard_tick(Guard *guard, Units *units, const Terrain *terrain,
                Fields *fields, const Relations *relations);

/* The bands look around. For each band:
 *
 * Its middle is the mean of the squares its units are on now. Its
 * enemies are the units of the players its player is at war with
 * that stand on a square one of its units sees, see
 * mechanics/sight.h, no further from the middle than its range.
 *
 * Each unit of the band takes the enemy nearest to it, and of those
 * equally near the one it has to turn least to face. If it stands
 * right beside that enemy it stays and turns to it; if not it walks
 * to it. A unit of a band that has no enemy walks back to the square
 * it guards from.
 *
 * A band with no unit of an enemy player anywhere near is not looked
 * at any closer, so bands that are left alone cost next to nothing. */
void guard_turn(Guard *guard, Units *units, const Terrain *terrain,
                Fields *fields, const Relations *relations);

#endif
