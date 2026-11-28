/* A unit as it is in a game that is being played: where it stands and
 * which way it looks now. A map's record of a unit, MapUnit, is only
 * how the unit starts.
 *
 * It holds what the game has a use for so far, and grows with it.
 */
#ifndef ROM2_MECHANICS_UNIT_H
#define ROM2_MECHANICS_UNIT_H

#include <stdbool.h>
#include <stdint.h>

/* A unit's place is in these parts of a square, as a map has it. */
#define UNIT_PLACE_PARTS 256

/* A unit can face this many ways: a full turn in so many steps, far
 * finer than its pictures show. Facings are counted round with a byte,
 * so that the step after the last is the first. */
#define UNIT_TURN 256

/* Which pictures show a unit. A hero, a player's own character, is
 * laid out as the type of unit he is armed as, but has pictures of his
 * own for it: one set in armour and one without. */
typedef enum UnitDress {
    UNIT_DRESS_PLAIN,       /* the pictures of its type: all but heroes */
    UNIT_DRESS_HERO_LIGHT,
    UNIT_DRESS_HERO_ARMOUR,
    UNIT_DRESSES
} UnitDress;

typedef struct Unit {
    uint16_t type;          /* its ID in units.reg: how it looks */
    uint8_t dress;          /* a UnitDress: which pictures of the type */
    int32_t kind;           /* the id of its kind: its numbers */
    int32_t player;         /* counted from 1, as a map counts */
    int32_t x;              /* in UNIT_PLACE_PARTS of a square */
    int32_t y;
    uint8_t facing;         /* of UNIT_TURN */
    uint8_t facing_wanted;  /* the way it is turning to */
    uint8_t turn_speed;     /* steps of facing in a tick: its kind's */
    bool turning;           /* on its way to the facing it wants */
} Unit;

/* A turn of less than this many steps is taken at once, by a unit that
 * was not turning already. */
#define UNIT_TURN_AT_ONCE 33

/* One tick of turning to the facing a unit wants, by the shorter way
 * round and at its speed, as the original turns one. A unit that is
 * not turning yet and wants a turn of less than UNIT_TURN_AT_ONCE
 * steps takes it at once. A unit that faces as it wants does not
 * move. */
void unit_turn(Unit *unit);

#endif
