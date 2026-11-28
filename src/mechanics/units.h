/* The units of a game that is being played.
 *
 * Each has a place in a list, a slot. A unit that is gone leaves its
 * slot to the next one made, so the list does not grow for ever. What
 * names a unit, a UnitId, therefore says the slot and which of the
 * slot's units it means: an id kept from before leads to nothing once
 * its unit is gone, and never to the one that took the slot.
 */
#ifndef ROM2_MECHANICS_UNITS_H
#define ROM2_MECHANICS_UNITS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "data/map.h"
#include "data/unit_kind.h"
#include "mechanics/random.h"
#include "mechanics/unit.h"

/* The most units there can be at once. */
#define UNITS_MOST 65535u

typedef struct UnitId {
    uint16_t slot;
    uint16_t age;       /* how many units the slot had before this one */
} UnitId;

typedef struct UnitSlot {
    bool taken;
    uint16_t age;
    Unit unit;
} UnitSlot;

/* The list. `count` is how many slots have ever been used: those from
 * `count` on are free, and so is each one before it that is not taken.
 * `living` is how many are taken. The slots are owned. */
typedef struct Units {
    UnitSlot *slots;
    uint32_t count;
    uint32_t room;
    uint32_t living;
    /* The players a person plays: a bit for each of the first
     * UNITS_PLAYERS, the lowest for player 1. The computer plays the
     * others. A hero who enters makes his player a person's. */
    uint32_t people;
} Units;

/* The players the game tells a person's from the computer's: those of
 * a map, and as many more. */
#define UNITS_PLAYERS 32

/* The player a person plays who plays a map alone: its first, which
 * the maps call Self. */
#define UNITS_PLAYER_SELF 1

/* How far a unit turns in a tick whose kind is not known. */
#define UNITS_TURN_SPEED_UNKNOWN 16

/* Makes the units a map starts with, in the order the map has them,
 * so that the first is in slot 0. A unit faces as its record says: one
 * whose record says a way there is not faces the first.
 *
 * `kinds` has UNIT_KIND_IDS entries, the kind with each ID, all zeros
 * for one that is not known: a unit turns as fast as its kind, and has
 * the health and the mana of its kind, all of both, unless its record
 * says a health of its own and the most of it.
 *
 * Returns false, and leaves `*out` alone, if memory runs out. */
bool units_from_map(const Map *map, const UnitKind *kinds, Units *out);

/* The kind a hero has the numbers of, until he has his own: that of
 * an unarmed fighter. */
#define UNITS_HERO_KIND 1

/* A hero who enters a map: whose he is, and how he looks, which
 * follows what he holds and wears. */
typedef struct UnitsHero {
    int32_t player;         /* counted from 1 */
    uint16_t type;          /* the type of unit he is armed as */
    uint8_t dress;          /* UNIT_DRESS_HERO_LIGHT or _ARMOUR */
} UnitsHero;

/* Puts a hero on the map, in the middle of a square where a player
 * enters it. The dice pick one of them, as in the original; if a unit
 * stands on it he takes the next in the map's order that is free, and
 * the picked one if a unit stands on each. He faces the first way,
 * turns as fast as his kind and has its health and mana, and his
 * player is from then on one a person plays. `out` may be NULL.
 *
 * Returns false, and changes nothing, the dice too, if the map has no
 * such square. Returns false also if there are UNITS_MOST units
 * already or memory runs out. */
bool units_enter(Units *units, const Map *map, const UnitKind *kinds,
                 const UnitsHero *hero, Random *random, UnitId *out);

/* One tick of every unit: so far each stands, having nothing else to
 * do. A unit the computer plays may look around, as in the original;
 * one of a player a person plays, his hero and any other, stands as
 * it was left. The units take their turn in the order of their
 * slots. */
void units_tick(Units *units, Random *random);

/* Frees the list and sets it to none. */
void units_free(Units *units);

/* Adds a unit and gives its id. Returns false, and changes nothing, if
 * there are UNITS_MOST already or memory runs out. `out` may be
 * NULL. */
bool units_add(Units *units, const Unit *unit, UnitId *out);

/* The unit an id names, or NULL if it is gone or never was. What is
 * returned holds until the next unit is added. */
Unit *units_get(const Units *units, UnitId id);

/* Takes a unit away. Does nothing if it is gone already. */
void units_remove(Units *units, UnitId id);

/* The unit in a slot and its id, for going through all of them: NULL
 * for a slot that is free or past the last. `id` may be NULL. */
Unit *units_at(const Units *units, uint32_t slot, UnitId *id);

#endif
