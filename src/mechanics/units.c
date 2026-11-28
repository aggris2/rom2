#include "mechanics/units.h"

#include <stdlib.h>
#include <string.h>

#include "mechanics/idle.h"

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

/* Whether the computer plays the unit: no person plays its player. */
static bool computer_plays(const Units *units, const Unit *unit)
{
    return unit->player < 1 || unit->player > UNITS_PLAYERS
        || (units->people >> (unit->player - 1) & 1u) == 0;
}

void units_tick(Units *units, Random *random)
{
    for (uint32_t slot = 0; slot < units->count; slot++) {
        if (!units->slots[slot].taken) {
            continue;
        }
        Unit *unit = &units->slots[slot].unit;
        if (computer_plays(units, unit)) {
            idle_tick(unit, random);
        } else {
            unit_turn(unit);
        }
    }
}

/* How fast a unit of a kind turns. */
static uint8_t turn_speed_of(const UnitKind *kinds, int32_t kind)
{
    uint8_t speed = kind < 0 || kind >= UNIT_KIND_IDS
        ? 0 : kinds[kind].rotation_speed;
    return speed != 0 ? speed : UNITS_TURN_SPEED_UNKNOWN;
}

/* Gives a unit the health and the mana its kind starts with. */
static void fill_up(Unit *unit, const UnitKind *kinds)
{
    if (unit->kind < 0 || unit->kind >= UNIT_KIND_IDS) {
        return;
    }
    const UnitKind *kind = &kinds[unit->kind];
    unit->health = kind->health;
    unit->health_most = kind->health;
    unit->mana = kind->mana;
    unit->mana_most = kind->mana;
}

/* Whether a unit stands on a square. */
static bool stands_on(const Units *units, int32_t x, int32_t y)
{
    for (uint32_t slot = 0; slot < units->count; slot++) {
        const Unit *unit = units_at(units, slot, NULL);
        if (unit != NULL && unit->x / UNIT_PLACE_PARTS == x
            && unit->y / UNIT_PLACE_PARTS == y) {
            return true;
        }
    }
    return false;
}

bool units_enter(Units *units, const Map *map, const UnitKind *kinds,
                 const UnitsHero *who, Random *random, UnitId *out)
{
    if (map->drop_count == 0) {
        return false;
    }
    uint32_t picked = random_below(random, map->drop_count);
    const MapDrop *drop = &map->drops[picked];
    for (uint32_t i = 0; i < map->drop_count; i++) {
        const MapDrop *next = &map->drops[(picked + i) % map->drop_count];
        if (!stands_on(units, next->x, next->y)) {
            drop = next;
            break;
        }
    }
    Unit hero;
    memset(&hero, 0, sizeof hero);
    hero.type = who->type;
    hero.dress = who->dress;
    hero.kind = UNITS_HERO_KIND;
    hero.player = who->player;
    hero.x = drop->x * UNIT_PLACE_PARTS + UNIT_PLACE_PARTS / 2;
    hero.y = drop->y * UNIT_PLACE_PARTS + UNIT_PLACE_PARTS / 2;
    hero.turn_speed = turn_speed_of(kinds, hero.kind);
    fill_up(&hero, kinds);
    if (!units_add(units, &hero, out)) {
        return false;
    }
    if (who->player >= 1 && who->player <= UNITS_PLAYERS) {
        units->people |= 1u << (who->player - 1);
    }
    return true;
}

bool units_from_map(const Map *map, const UnitKind *kinds, Units *out)
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
        unit.turn_speed = turn_speed_of(kinds, from->kind);
        fill_up(&unit, kinds);
        /* A record may say its own health, and the most of it. */
        if (from->health >= 0 && from->health_max > 0) {
            unit.health = from->health;
            unit.health_most = from->health_max;
        }
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
