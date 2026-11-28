/* A flow field: how far every square of a map is from a goal, by the
 * way a unit would walk it.
 *
 * It is made by one spread from the goal outward over the ground, and
 * costs the same whatever number of units then use it. A unit that
 * wants to reach the goal needs no path of its own: it steps to the
 * square beside it that is nearer, and any square it can get to the
 * goal from has such a neighbour.
 *
 * The price of a step is the original's: the cost of the square
 * stepped onto, and one and a half times that to a corner. What does
 * not walk pays the same for every square. A unit of more than one
 * square needs all the squares it covers open, and its field is by
 * the first of them, the top left: see walk_open().
 *
 * The table knows nothing of units. Fields are kept by goal and
 * shared: see Fields below.
 */
#ifndef ROM2_MECHANICS_FIELD_H
#define ROM2_MECHANICS_FIELD_H

#include <stdbool.h>
#include <stdint.h>

#include "mechanics/terrain.h"

/* How far a square is that the goal cannot be reached from. */
#define FIELD_FAR UINT32_MAX

/* What a straight step and a step to a corner cost, times the cost of
 * the square stepped onto: the second is one and a half of the
 * first. And the cost every square has for what does not walk. */
#define FIELD_STRAIGHT 2u
#define FIELD_CORNER 3u
#define FIELD_COST_FLAT 1u

typedef struct Field {
    uint32_t width;         /* in squares, as its terrain */
    uint32_t height;
    int32_t goal_x;
    int32_t goal_y;
    uint8_t closed_to;      /* a TERRAIN_NO_ bit: whom it is for */
    uint8_t size;           /* the squares along a side of who uses it */
    uint32_t *far;          /* a value for each square. Owned. */
} Field;

/* The fields of a game, kept by their goal and by whom they are for,
 * so that all the units with one goal share one. Each is counted by
 * those that use it and can be made anew for another goal when nobody
 * does. The lists are owned. */
#define FIELDS_MOST 64

typedef struct Fields {
    Field field[FIELDS_MOST];
    uint32_t users[FIELDS_MOST];
    /* Room for a spread, for a map of `squares`: where each square is
     * in the line of those that wait, and where each distance's line
     * starts. */
    uint32_t squares;
    uint32_t *next;
    uint32_t *before;
    uint32_t *first;
} Fields;

/* Makes the distances of a field anew for a goal on a terrain, for
 * units of `size` squares a side that `closed_to` stops: 0 counts as
 * 1. The field gets room if it has none, or not that of this terrain.
 * `fields` lends the room for the spread.
 *
 * A goal that is closed or off the map is far from everything: the
 * caller picks an open square. Returns false, and leaves the field
 * alone, if memory runs out. */
bool field_spread(Fields *fields, Field *field, const Terrain *terrain,
                  uint8_t closed_to, uint8_t size, int32_t goal_x,
                  int32_t goal_y);

/* How far the square (x, y) is from the field's goal: 0 on it,
 * FIELD_FAR where it cannot be reached from or off the map. */
uint32_t field_far(const Field *field, int32_t x, int32_t y);

/* Takes the field for a goal and for whom, makes it if there is none
 * yet, and counts one more user of it. `*out` gets its number, to give back
 * with fields_drop(). Returns false, and leaves `*out` alone, if all
 * FIELDS_MOST are in use for other goals or memory runs out. */
bool fields_take(Fields *fields, const Terrain *terrain, uint8_t closed_to,
                 uint8_t size, int32_t goal_x, int32_t goal_y,
                 uint16_t *out);

/* Counts one user less of a field. Does nothing for a number that is
 * not a field in use. */
void fields_drop(Fields *fields, uint16_t number);

/* Counts the users of every field anew: fields_no_users() says none
 * has any, and fields_user() then counts one for a field. The game
 * does this from its units each tick, so that a field can never stay
 * in use for a unit that is gone. A number that is no field is not
 * counted. */
void fields_no_users(Fields *fields);
void fields_user(Fields *fields, uint16_t number);

/* Makes every field that is in use anew for the terrain as it is
 * now: for when a structure has risen or fallen. Returns false if
 * memory ran out for one, which then keeps its old distances. */
bool fields_respread(Fields *fields, const Terrain *terrain);

/* The field with a number that fields_take() gave, or NULL. */
const Field *fields_get(const Fields *fields, uint16_t number);

/* Frees every field and the room for the spread, and sets all to
 * none. */
void fields_free(Fields *fields);

#endif
