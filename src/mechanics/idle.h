/* What a unit does that has nothing to do: it stands, and now and then
 * it looks another way.
 *
 * The rule is the original's. In each tick that a unit stands, the
 * dice say whether it looks around: seldom, about once in ten seconds
 * at the speed a game starts on. If it does, it picks a way well off
 * the one it faces and turns to it.
 */
#ifndef ROM2_MECHANICS_IDLE_H
#define ROM2_MECHANICS_IDLE_H

#include "mechanics/random.h"
#include "mechanics/unit.h"

/* A standing unit looks around in a tick when a number below
 * IDLE_CHANCES comes out below IDLE_LOOKS: 205 times in 32,768. */
#define IDLE_CHANCES 32768u
#define IDLE_LOOKS 205u

/* The new way to face is from this many steps further round to
 * IDLE_TURN_SPREAD - 1 more: from an eighth of a turn to most of
 * one. */
#define IDLE_TURN_LEAST 33u
#define IDLE_TURN_SPREAD 190u

/* One tick of a unit that has nothing to do. */
void idle_tick(Unit *unit, Random *random);

#endif
