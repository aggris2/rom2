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
} Units;

/* How far a unit turns in a tick whose kind is not known. */
#define UNITS_TURN_SPEED_UNKNOWN 16

/* Makes the units a map starts with, in the order the map has them,
 * so that the first is in slot 0. A unit faces as its record says: one
 * whose record says a way there is not faces the first.
 *
 * `kinds` has UNIT_KIND_IDS entries, the kind with each ID, all zeros
 * for one that is not known: a unit turns as fast as its kind.
 *
 * Returns false, and leaves `*out` alone, if memory runs out. */
bool units_from_map(const Map *map, const UnitKind *kinds, Units *out);

/* One tick of every unit: so far each stands and may look around,
 * having nothing else to do. The units take their turn in the order of their
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
