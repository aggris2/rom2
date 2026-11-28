#include "mechanics/units.h"

#include <stdlib.h>
#include <string.h>

#include "mechanics/idle.h"
#include "mechanics/walk.h"

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

bool units_make(uint32_t width, uint32_t height, Units *out)
{
    Units units;
    size_t squares = (size_t)width * height;
    memset(&units, 0, sizeof units);
    units.width = width;
    units.height = height;
    units.on_ground = calloc(squares > 0 ? squares : 1,
        sizeof *units.on_ground);
    units.in_air = calloc(squares > 0 ? squares : 1, sizeof *units.in_air);
    if (units.on_ground == NULL || units.in_air == NULL) {
        free(units.on_ground);
        free(units.in_air);
        return false;
    }
    *out = units;
    return true;
}

/* The squares a unit covers along a side. */
static int32_t size_of(const Unit *unit)
{
    return unit->size == 0 ? 1 : unit->size;
}

/* The layer a unit stands in. */
static uint16_t *layer_of(const Units *units, const Unit *unit)
{
    return unit->mover == UNIT_FLIES ? units->in_air : units->on_ground;
}

/* Whether a unit of this layer and size could stand with its first
 * square on (x, y): each square it would cover is on the map and held
 * by nobody, or by the unit in `self`, a slot plus 1. */
static bool squares_free(const Units *units, const uint16_t *layer,
                         int32_t size, int32_t x, int32_t y, uint16_t self)
{
    if (x < 0 || y < 0 || (uint32_t)(x + size) > units->width
        || (uint32_t)(y + size) > units->height) {
        return false;
    }
    for (int32_t row = y; row < y + size; row++) {
        for (int32_t column = x; column < x + size; column++) {
            uint16_t who = layer[(size_t)row * units->width
                + (size_t)column];
            if (who != 0 && who != self) {
                return false;
            }
        }
    }
    return true;
}

/* Writes who holds the squares a unit covers with its first square
 * on (x, y): `who` is its slot plus 1, or 0 to free them. */
static void hold_at(Units *units, const Unit *unit, int32_t x, int32_t y,
                    uint16_t who)
{
    uint16_t *layer = layer_of(units, unit);
    int32_t size = size_of(unit);
    for (int32_t row = y; row < y + size; row++) {
        for (int32_t column = x; column < x + size; column++) {
            layer[(size_t)row * units->width + (size_t)column] = who;
        }
    }
}

bool units_add(Units *units, const Unit *unit, UnitId *out)
{
    /* On a map it must have its squares free. */
    if (units->width != 0 && (unit->x < 0 || unit->y < 0
            || !squares_free(units, layer_of(units, unit), size_of(unit),
                unit->x / UNIT_PLACE_PARTS, unit->y / UNIT_PLACE_PARTS,
                0))) {
        return false;
    }
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
    into->unit.reaching = false;
    if (units->width != 0) {
        hold_at(units, unit, unit->x / UNIT_PLACE_PARTS,
            unit->y / UNIT_PLACE_PARTS, (uint16_t)(slot + 1));
    }
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
    if (units->width != 0) {
        const Unit *unit = &slot->unit;
        hold_at(units, unit, unit->x / UNIT_PLACE_PARTS,
            unit->y / UNIT_PLACE_PARTS, 0);
        if (unit->reaching) {
            hold_at(units, unit, unit->to_x, unit->to_y, 0);
        }
    }
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

bool units_can_step(const Units *units, UnitId id, int32_t x, int32_t y)
{
    const Unit *unit = units_get(units, id);
    return unit != NULL && units->width != 0
        && squares_free(units, layer_of(units, unit), size_of(unit), x, y,
            (uint16_t)(id.slot + 1));
}

bool units_step(Units *units, UnitId id, int32_t x, int32_t y)
{
    Unit *unit = units_get(units, id);
    if (unit == NULL || units->width == 0 || unit->reaching
        || !squares_free(units, layer_of(units, unit), size_of(unit), x,
            y, (uint16_t)(id.slot + 1))) {
        return false;
    }
    unit->from_x = (int16_t)(unit->x / UNIT_PLACE_PARTS);
    unit->from_y = (int16_t)(unit->y / UNIT_PLACE_PARTS);
    unit->to_x = (int16_t)x;
    unit->to_y = (int16_t)y;
    unit->reaching = true;
    hold_at(units, unit, x, y, (uint16_t)(id.slot + 1));
    return true;
}

void units_arrive(Units *units, UnitId id)
{
    Unit *unit = units_get(units, id);
    if (unit == NULL || !unit->reaching) {
        return;
    }
    /* The old squares are let go, and the new held again: a large
     * unit covers some of both. */
    hold_at(units, unit, unit->x / UNIT_PLACE_PARTS,
        unit->y / UNIT_PLACE_PARTS, 0);
    hold_at(units, unit, unit->to_x, unit->to_y, (uint16_t)(id.slot + 1));
    unit->x = unit->to_x * UNIT_PLACE_PARTS + UNIT_PLACE_PARTS / 2;
    unit->y = unit->to_y * UNIT_PLACE_PARTS + UNIT_PLACE_PARTS / 2;
    unit->reaching = false;
}

void units_tick(Units *units, const Terrain *terrain, Fields *fields,
                Random *random)
{
    /* The fields are in use by the units that are going by them now,
     * and by no unit that has gone since the last tick. */
    if (fields != NULL) {
        fields_no_users(fields);
        for (uint32_t slot = 0; slot < units->count; slot++) {
            const Unit *unit = units_at(units, slot, NULL);
            if (unit != NULL && unit->going) {
                fields_user(fields, unit->field);
            }
        }
    }
    for (uint32_t slot = 0; slot < units->count; slot++) {
        UnitId id;
        Unit *unit = units_at(units, slot, &id);
        if (unit == NULL
            || (terrain != NULL
                && walk_tick(units, id, terrain, fields))) {
            continue;
        }
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

/* Gives a unit what its kind says of it: the health and the mana it
 * starts with, its size and how it moves. */
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
    unit->size = kind->size;
    unit->mover = kind->mover;
    unit->speed = kind->speed;
}

/* Whether a unit could stand with its first square on (x, y): the
 * ground is open to it there and nobody of its layer holds a square
 * it would cover. */
static bool can_stand(const Units *units, const Terrain *terrain,
                      const Unit *unit, int32_t x, int32_t y)
{
    int32_t size = size_of(unit);
    uint8_t closed = unit->mover == UNIT_FLIES ? TERRAIN_NO_FLY
        : unit->mover == UNIT_HOVERS ? TERRAIN_NO_HOVER : TERRAIN_NO_WALK;
    if (!squares_free(units, layer_of(units, unit), size, x, y, 0)) {
        return false;
    }
    for (int32_t row = y; row < y + size; row++) {
        for (int32_t column = x; column < x + size; column++) {
            if ((terrain->closed[(size_t)row * terrain->width
                    + (size_t)column] & closed) != 0) {
                return false;
            }
        }
    }
    return true;
}

/* Finds the nearest square to (x, y) a unit can stand on, in rings
 * around it out to UNITS_NEAR: in a ring row after row from the top,
 * and in a row from the left. Returns false if there is none. */
static bool stand_near(const Units *units, const Terrain *terrain,
                       const Unit *unit, int32_t x, int32_t y,
                       int32_t *out_x, int32_t *out_y)
{
    for (int32_t ring = 0; ring <= UNITS_NEAR; ring++) {
        for (int32_t down = -ring; down <= ring; down++) {
            for (int32_t across = -ring; across <= ring; across++) {
                bool on_ring = down == -ring || down == ring
                    || across == -ring || across == ring;
                if (on_ring && can_stand(units, terrain, unit, x + across,
                        y + down)) {
                    *out_x = x + across;
                    *out_y = y + down;
                    return true;
                }
            }
        }
    }
    return false;
}

bool units_enter(Units *units, const Map *map, const UnitKind *kinds,
                 const Terrain *terrain, const UnitsHero *who,
                 Random *random, UnitId *out)
{
    if (map->drop_count == 0) {
        return false;
    }
    Random dice = *random;
    const MapDrop *drop = &map->drops[random_below(&dice,
        map->drop_count)];
    Unit hero;
    int32_t x = 0;
    int32_t y = 0;
    memset(&hero, 0, sizeof hero);
    hero.type = who->type;
    hero.dress = who->dress;
    hero.kind = UNITS_HERO_KIND;
    hero.player = who->player;
    hero.turn_speed = turn_speed_of(kinds, hero.kind);
    fill_up(&hero, kinds);
    if (!stand_near(units, terrain, &hero, drop->x, drop->y, &x, &y)) {
        return false;
    }
    hero.x = x * UNIT_PLACE_PARTS + UNIT_PLACE_PARTS / 2;
    hero.y = y * UNIT_PLACE_PARTS + UNIT_PLACE_PARTS / 2;
    if (!units_add(units, &hero, out)) {
        return false;
    }
    *random = dice;
    if (who->player >= 1 && who->player <= UNITS_PLAYERS) {
        units->people |= 1u << (who->player - 1);
    }
    return true;
}

bool units_from_map(const Map *map, const UnitKind *kinds,
                    const Terrain *terrain, Units *out)
{
    Units units;
    if (!units_make(map->info.width, map->info.height, &units)) {
        return false;
    }
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
        /* Its own square if nobody is on it, whatever the ground;
         * else one near that it can stand on. */
        int32_t x = unit.x / UNIT_PLACE_PARTS;
        int32_t y = unit.y / UNIT_PLACE_PARTS;
        if (unit.x < 0 || unit.y < 0
            || !squares_free(&units, layer_of(&units, &unit),
                size_of(&unit), x, y, 0)) {
            if (unit.x < 0 || unit.y < 0
                || !stand_near(&units, terrain, &unit, x, y, &x, &y)) {
                continue;
            }
            unit.x = x * UNIT_PLACE_PARTS + UNIT_PLACE_PARTS / 2;
            unit.y = y * UNIT_PLACE_PARTS + UNIT_PLACE_PARTS / 2;
        }
        /* The list is full or memory ran out. */
        if (!units_add(&units, &unit, NULL)) {
            units_free(&units);
            return false;
        }
    }
    *out = units;
    return true;
}

bool units_standing(const Units *units, bool in_air, int32_t x, int32_t y,
                    UnitId *out)
{
    if (x < 0 || y < 0 || (uint32_t)x >= units->width
        || (uint32_t)y >= units->height) {
        return false;
    }
    const uint16_t *layer = in_air ? units->in_air : units->on_ground;
    uint16_t who = layer[(size_t)y * units->width + (size_t)x];
    if (who == 0) {
        return false;
    }
    out->slot = (uint16_t)(who - 1);
    out->age = units->slots[who - 1].age;
    return true;
}

/* Whether a unit holds each square it would cover with its first
 * square on (x, y). */
static bool holds_at(const Units *units, const Unit *unit, uint32_t slot,
                     int32_t x, int32_t y)
{
    const uint16_t *layer = layer_of(units, unit);
    int32_t size = size_of(unit);
    if (x < 0 || y < 0 || (uint32_t)(x + size) > units->width
        || (uint32_t)(y + size) > units->height) {
        return false;
    }
    for (int32_t row = y; row < y + size; row++) {
        for (int32_t column = x; column < x + size; column++) {
            if (layer[(size_t)row * units->width + (size_t)column]
                != slot + 1) {
                return false;
            }
        }
    }
    return true;
}

bool units_check(const Units *units)
{
    size_t squares = (size_t)units->width * units->height;
    size_t held = 0;
    size_t covered = 0;
    if (units->width == 0) {
        return false;
    }
    /* Every unit holds each square it is on, and each it reaches
     * for. */
    for (uint32_t slot = 0; slot < units->count; slot++) {
        const Unit *unit = units_at(units, slot, NULL);
        if (unit == NULL) {
            continue;
        }
        int32_t size = size_of(unit);
        int32_t x = unit->x / UNIT_PLACE_PARTS;
        int32_t y = unit->y / UNIT_PLACE_PARTS;
        if (unit->x < 0 || unit->y < 0
            || !holds_at(units, unit, slot, x, y)) {
            return false;
        }
        covered += (size_t)(size * size);
        if (!unit->reaching) {
            continue;
        }
        if (!holds_at(units, unit, slot, unit->to_x, unit->to_y)) {
            return false;
        }
        /* Those of the new squares that are not also old ones. */
        for (int32_t row = unit->to_y; row < unit->to_y + size; row++) {
            for (int32_t column = unit->to_x; column < unit->to_x + size;
                column++) {
                covered += column < x || column >= x + size || row < y
                    || row >= y + size ? 1u : 0u;
            }
        }
    }
    /* And no square is held beyond those. */
    for (size_t i = 0; i < squares; i++) {
        held += units->on_ground[i] != 0 ? 1u : 0u;
        held += units->in_air[i] != 0 ? 1u : 0u;
    }
    return held == covered;
}

void units_free(Units *units)
{
    free(units->on_ground);
    free(units->in_air);
    free(units->slots);
    memset(units, 0, sizeof *units);
}
