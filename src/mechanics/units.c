#include "mechanics/units.h"

#include <stdlib.h>
#include <string.h>

/* The steps of a turn a map counts a unit's facing in. */
#define MAP_FACINGS 16

/* Makes room for a slot more. */
static bool make_room(Units *units)
{
    if (units->count < units->room) {
        return true;
    }
    uint32_t room = units->room == 0 ? 64 : units->room * 2;
    room = room > UNITS_MOST ? UNITS_MOST : room;
    UnitSlot *slots = realloc(units->slots, room * sizeof *slots);
    if (slots == NULL) {
        return false;
    }
    memset(slots + units->room, 0, (room - units->room) * sizeof *slots);
    units->slots = slots;
    units->room = room;
    return true;
}

bool units_add(Units *units, const Unit *unit, UnitId *out)
{
    /* A slot that was left, if there is one: the first of them. */
    uint32_t slot = units->count;
    if (units->living < units->count) {
        for (slot = 0; units->slots[slot].taken; slot++) {
        }
    } else if (units->count >= UNITS_MOST || !make_room(units)) {
        return false;
    }
    UnitSlot *into = &units->slots[slot];
    into->taken = true;
    into->unit = *unit;
    if (slot == units->count) {
        units->count++;
    }
    units->living++;
    if (out != NULL) {
        out->slot = (uint16_t)slot;
        out->age = into->age;
    }
    return true;
}

Unit *units_at(const Units *units, uint32_t slot, UnitId *id)
{
    if (slot >= units->count || !units->slots[slot].taken) {
        return NULL;
    }
    if (id != NULL) {
        id->slot = (uint16_t)slot;
        id->age = units->slots[slot].age;
    }
    return &units->slots[slot].unit;
}

Unit *units_get(const Units *units, UnitId id)
{
    if (id.slot >= units->count || !units->slots[id.slot].taken
        || units->slots[id.slot].age != id.age) {
        return NULL;
    }
    return &units->slots[id.slot].unit;
}

void units_remove(Units *units, UnitId id)
{
    if (units_get(units, id) == NULL) {
        return;
    }
    UnitSlot *slot = &units->slots[id.slot];
    slot->taken = false;
    /* The next unit here is another: an id of this one no longer
     * fits. */
    slot->age++;
    units->living--;
}

bool units_from_map(const Map *map, Units *out)
{
    Units units;
    memset(&units, 0, sizeof units);
    for (uint32_t i = 0; i < map->unit_count; i++) {
        const MapUnit *from = &map->units[i];
        Unit unit;
        memset(&unit, 0, sizeof unit);
        unit.type = from->type > UINT16_MAX ? UINT16_MAX
            : (uint16_t)from->type;
        unit.kind = from->kind;
        unit.player = from->player;
        unit.x = from->x;
        unit.y = from->y;
        int32_t angle = from->angle < 0 || from->angle >= MAP_FACINGS
            ? 0 : from->angle;
        unit.facing = (uint8_t)(angle * (UNIT_TURN / MAP_FACINGS));
        unit.facing_wanted = unit.facing;
        if (!units_add(&units, &unit, NULL)) {
            units_free(&units);
            return false;
        }
    }
    *out = units;
    return true;
}

void units_free(Units *units)
{
    free(units->slots);
    memset(units, 0, sizeof *units);
}
