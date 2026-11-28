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
    int32_t health;
    int32_t health_most;    /* 0 or less for a unit that has none */
    int32_t mana;
    int32_t mana_most;      /* 0 or less for a unit that has none */
    /* How many squares it covers each way, from the square of its
     * place to the right and down: its kind's, and 0 counts as 1. And
     * how it moves, a UnitMover: 0 counts as walking. */
    uint8_t size;
    uint8_t mover;
    uint8_t speed;          /* its kind's: see mechanics/walk.h */
    /* Walking. A step goes from the square `from` to the square
     * `to`, and what is drawn slides from the one to the other, the
     * same part of the way in every tick. `ticks` is how many ticks
     * the step lasts and `left` how many of them are still to come:
     * at none left it has arrived. `ticks` stays for the tick in
     * which it arrives, so that the last part of the slide can be
     * drawn, and is 0 for a unit that stood before that.
     *
     * The unit holds the squares it steps to from the first tick, so
     * that no other can take them, and is `reaching` them. But its
     * place, where it is met and hit, stays where it came from until
     * it is half-way: then its place becomes the new square and the
     * old one is let go. So a unit is always on the square most of it
     * is over. */
    int16_t from_x;         /* in squares */
    int16_t from_y;
    int16_t to_x;
    int16_t to_y;
    bool reaching;
    uint16_t left;
    uint16_t ticks;
    /* How many ticks it has walked without a stop, for its frames. */
    uint16_t walked;
    /* The step it was told to take and has not yet begun: `way` is
     * one of the eight, see mechanics/walk.h. */
    bool told;
    uint8_t way;
    /* Where it is going, if `going`: the number of the field of its
     * goal, see mechanics/field.h. And for how many ticks it has had
     * no square nearer to step to. */
    bool going;
    uint16_t field;
    uint16_t waited;
} Unit;

/* The whole of a step, in the parts a stride is counted in: see
 * mechanics/walk.h. */
#define UNIT_STEP 256

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
