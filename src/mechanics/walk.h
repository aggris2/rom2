/* Walking: a unit goes from its square to one of the eight beside it.
 *
 * A step is checked and taken in one tick. The unit turns to face the
 * way it goes; when it does, and the square is open to it and nobody
 * holds it, the unit holds it at once, so that no other can. What is
 * seen of it slides there over the ticks that follow, and nothing
 * stops or undoes a slide. Half-way there the unit is on the new
 * square and lets go of the old: until then it is still on the old
 * one, where it is seen to be. So no two units can ever be on one
 * square, there being no moment between the looking and the taking,
 * and a unit is hit where it looks to be.
 *
 * How fast it goes is the original's rule, see walk_stride().
 */
#ifndef ROM2_MECHANICS_WALK_H
#define ROM2_MECHANICS_WALK_H

#include <stdbool.h>
#include <stdint.h>

#include "mechanics/terrain.h"
#include "mechanics/units.h"

/* The ways a step can go: the first is up the map, and they go round
 * as a clock's hand does. A unit that goes a way faces
 * walk_facing() of it. */
#define WALK_WAYS 8

/* The fastest and the slowest a unit slides: parts of UNIT_STEP in a
 * tick. */
#define WALK_STRIDE_LEAST 1
#define WALK_STRIDE_MOST 63

/* How a way changes the square, and the facing of a unit that goes
 * it. `way` is less than WALK_WAYS. */
int32_t walk_across(uint8_t way);
int32_t walk_down(uint8_t way);
uint8_t walk_facing(uint8_t way);

/* The way from a square to another: the one of the eight that points
 * most nearly there. Returns false, and leaves `*way` alone, if the
 * two are one square. */
bool walk_way_to(int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y,
                 uint8_t *way);

/* How much of a step a unit goes in a tick, going `way` from the
 * square (x, y): from WALK_STRIDE_LEAST to WALK_STRIDE_MOST parts of
 * UNIT_STEP. A step lasts as many whole ticks as that takes, from 5
 * to 256, as in the original; the slide is spread evenly over them,
 * so that a unit that walks on does not falter between squares.
 *
 * What flies or hovers goes at its own speed. For what walks the
 * speed is taken eight times, then raised going downhill and lowered
 * going uphill by a 64th for each pixel the next square lies lower or
 * higher, up to 32 of them, and then divided by the mean of the costs
 * of the two squares. A step to a corner, which is longer, slides
 * 707 thousandths of that in a tick. */
uint8_t walk_stride(const Terrain *terrain, const Unit *unit, int32_t x,
                    int32_t y, uint8_t way);

/* Whether a unit may step `way` from the square (x, y) as far as the
 * ground goes: every square it would cover there is open to it. A
 * step to a corner asks no more than that, as in the original: a unit
 * goes past the corner of a wall. Who stands there is not asked. */
bool walk_open(const Terrain *terrain, const Unit *unit, int32_t x,
               int32_t y, uint8_t way);

/* For how many ticks a unit that is going somewhere waits with no
 * square nearer to step to, before it gives up: ten seconds at the
 * speed a game starts on. And how far around a goal that is closed to
 * it a unit looks for an open square to go to instead. */
#define WALK_PATIENCE 160
#define WALK_GOAL_NEAR 8

/* Sends a unit to the square (x, y). It goes by the field of that
 * goal, which it shares with every other unit sent there: each time
 * it stands, it steps to the square beside it that is nearest to the
 * goal of those that are open to it and that nobody stands on. With
 * none such it waits, and after WALK_PATIENCE ticks of that, or if
 * there is no way at all, it gives up and stands. A goal that is
 * closed to the unit becomes the nearest open square within
 * WALK_GOAL_NEAR. A unit of several squares goes to stand with its
 * middle on the goal, as near as whole squares allow.
 *
 * Whatever it was going to before is dropped. Returns false, and
 * leaves the unit as it was, if it is gone, there is no open square
 * near the goal, or no field is to be had: see fields_take(). */
bool walk_send(Units *units, Fields *fields, const Terrain *terrain,
               UnitId id, int32_t x, int32_t y);

/* Makes a unit forget where it was going: it ends the step it is on
 * and stands. To be called for a unit that is taken away, too, or its
 * field stays counted as in use. */
void walk_stop(Units *units, Fields *fields, UnitId id);

/* Tells a unit to take one step `way`. It begins when the unit has
 * finished the step it is on and has turned to face the way, and is
 * forgotten if it then cannot be taken. Returns false, and changes
 * nothing, if the unit is gone or there is no such way. */
bool walk_tell(Units *units, UnitId id, uint8_t way);

/* How many ticks a step lasts at a stride. */
uint16_t walk_ticks(uint8_t stride);

/* One tick of a unit's walking. Returns whether it had any to do: a
 * unit that slides, turns to a step, or waits for a square on its
 * way does nothing else in the tick. `fields` are those walk_send()
 * was given. */
bool walk_tick(Units *units, UnitId id, const Terrain *terrain,
               Fields *fields);

#endif
