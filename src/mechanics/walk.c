#include "mechanics/walk.h"

#include <stddef.h>

/* What the speed of what walks is taken times, as the original's
 * rules have it, and the most pixels of height that count. */
#define SPEED_TIMES 8
#define HEIGHT_MOST 32
#define HEIGHT_PARTS 64

/* The cost that changes nothing, taken where two squares have none. */
#define COST_PLAIN 8

static const int8_t across_of[WALK_WAYS] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int8_t down_of[WALK_WAYS] = {-1, -1, 0, 1, 1, 1, 0, -1};

int32_t walk_across(uint8_t way)
{
    return across_of[way % WALK_WAYS];
}

int32_t walk_down(uint8_t way)
{
    return down_of[way % WALK_WAYS];
}

uint8_t walk_facing(uint8_t way)
{
    return (uint8_t)(way % WALK_WAYS * (UNIT_TURN / WALK_WAYS));
}

bool walk_way_to(int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y,
                 uint8_t *way)
{
    int32_t across = to_x - from_x;
    int32_t down = to_y - from_y;
    int32_t wide = across < 0 ? -across : across;
    int32_t tall = down < 0 ? -down : down;
    if (across == 0 && down == 0) {
        return false;
    }
    /* Straight if it is more than twice as far one way as the other,
     * and to a corner if not. */
    int32_t step_across = wide * 2 < tall ? 0 : across > 0 ? 1 : -1;
    int32_t step_down = tall * 2 < wide ? 0 : down > 0 ? 1 : -1;
    for (uint8_t i = 0; i < WALK_WAYS; i++) {
        if (across_of[i] == step_across && down_of[i] == step_down) {
            *way = i;
            return true;
        }
    }
    return false;
}

/* What a square is closed to that stops this unit. */
static uint8_t closed_to(const Unit *unit)
{
    return unit->mover == UNIT_FLIES ? TERRAIN_NO_FLY
        : unit->mover == UNIT_HOVERS ? TERRAIN_NO_HOVER : TERRAIN_NO_WALK;
}

static bool on_terrain(const Terrain *terrain, int32_t x, int32_t y)
{
    return x >= 0 && y >= 0 && (uint32_t)x < terrain->width
        && (uint32_t)y < terrain->height;
}

/* Whether a unit could stand with its first square on (x, y) as far
 * as the ground goes. */
static bool ground_open(const Terrain *terrain, const Unit *unit, int32_t x,
                        int32_t y)
{
    int32_t size = unit->size == 0 ? 1 : unit->size;
    for (int32_t row = y; row < y + size; row++) {
        for (int32_t column = x; column < x + size; column++) {
            if (!on_terrain(terrain, column, row)
                || (terrain->closed[(size_t)row * terrain->width
                    + (size_t)column] & closed_to(unit)) != 0) {
                return false;
            }
        }
    }
    return true;
}

bool walk_open(const Terrain *terrain, const Unit *unit, int32_t x,
               int32_t y, uint8_t way)
{
    return ground_open(terrain, unit, x + walk_across(way),
        y + walk_down(way));
}

uint8_t walk_stride(const Terrain *terrain, const Unit *unit, int32_t x,
                    int32_t y, uint8_t way)
{
    int32_t across = walk_across(way);
    int32_t down = walk_down(way);
    int32_t speed = unit->speed == 0 ? UNITS_SPEED_UNKNOWN : unit->speed;
    if (unit->mover != UNIT_FLIES && unit->mover != UNIT_HOVERS
        && on_terrain(terrain, x, y)
        && on_terrain(terrain, x + across, y + down)) {
        size_t here = (size_t)y * terrain->width + (size_t)x;
        size_t next = (size_t)(y + down) * terrain->width
            + (size_t)(x + across);
        /* Above 0 going downhill. */
        int32_t lower = (int32_t)terrain->raised[here]
            - (int32_t)terrain->raised[next];
        lower = lower > HEIGHT_MOST ? HEIGHT_MOST
            : lower < -HEIGHT_MOST ? -HEIGHT_MOST : lower;
        speed *= SPEED_TIMES;
        /* Each cut down to a whole number, as the original cuts. */
        speed = lower < 0 ? speed - speed * -lower / HEIGHT_PARTS
            : speed + speed * lower / HEIGHT_PARTS;
        int32_t cost = ((int32_t)terrain->cost[here]
            + (int32_t)terrain->cost[next]) / 2;
        speed /= cost == 0 ? COST_PLAIN : cost;
    }
    speed = speed < WALK_STRIDE_LEAST ? WALK_STRIDE_LEAST
        : speed > WALK_STRIDE_MOST ? WALK_STRIDE_MOST : speed;
    if (across != 0 && down != 0) {
        speed = speed * 707 / 1000;
        speed = speed < WALK_STRIDE_LEAST ? WALK_STRIDE_LEAST : speed;
    }
    return (uint8_t)speed;
}

bool walk_tell(Units *units, UnitId id, uint8_t way)
{
    Unit *unit = units_get(units, id);
    if (unit == NULL || way >= WALK_WAYS) {
        return false;
    }
    unit->told = true;
    unit->way = way;
    return true;
}

void walk_stop(Units *units, Fields *fields, UnitId id)
{
    Unit *unit = units_get(units, id);
    if (unit == NULL || !unit->going) {
        return;
    }
    fields_drop(fields, unit->field);
    unit->going = false;
    unit->told = false;
    unit->waited = 0;
}

/* Sends a unit to stand with its first square on (x, y), or on the
 * nearest square around it that is open to it and, with `unheld`,
 * that no other unit holds: ring after ring out to WALK_GOAL_NEAR, in
 * a ring row after row. With `nearer_than` at 0 or more, only a
 * square in a ring nearer to (x, y) than that will do. */
static bool send(Units *units, Fields *fields, const Terrain *terrain,
                 UnitId id, int32_t x, int32_t y, bool unheld,
                 int32_t nearer_than)
{
    Unit *unit = units_get(units, id);
    uint16_t field = 0;
    bool found = false;
    int32_t last = nearer_than >= 0 && nearer_than <= WALK_GOAL_NEAR
        ? nearer_than - 1 : WALK_GOAL_NEAR;
    for (int32_t ring = 0; ring <= last && !found; ring++) {
        for (int32_t down = -ring; down <= ring && !found; down++) {
            for (int32_t across = -ring; across <= ring && !found;
                across++) {
                bool on_ring = down == -ring || down == ring
                    || across == -ring || across == ring;
                if (on_ring && ground_open(terrain, unit, x + across,
                        y + down)
                    && (!unheld || units_can_step(units, id, x + across,
                            y + down))) {
                    x += across;
                    y += down;
                    found = true;
                }
            }
        }
    }
    if (!found || !fields_take(fields, terrain, closed_to(unit),
            unit->size, x, y, &field)) {
        return false;
    }
    walk_stop(units, fields, id);
    unit->going = true;
    unit->field = field;
    unit->waited = 0;
    return true;
}

bool walk_send(Units *units, Fields *fields, const Terrain *terrain,
               UnitId id, int32_t x, int32_t y)
{
    Unit *unit = units_get(units, id);
    if (unit == NULL) {
        return false;
    }
    /* Its first square is up and left of its middle. */
    x -= ((unit->size == 0 ? 1 : unit->size) - 1) / 2;
    y -= ((unit->size == 0 ? 1 : unit->size) - 1) / 2;
    if (!send(units, fields, terrain, id, x, y, false, -1)) {
        return false;
    }
    unit->goal_x = (int16_t)x;
    unit->goal_y = (int16_t)y;
    unit->beside = 0;
    return true;
}

/* Whether any unit that holds a square the unit would cover with its
 * first square on (x, y) is on its way somewhere: walking, about to,
 * or going to a goal of its own. One that is not will stay. */
static bool held_by_one_on_its_way(const Units *units, UnitId id,
                                   const Unit *unit, int32_t x, int32_t y)
{
    int32_t size = unit->size == 0 ? 1 : unit->size;
    for (int32_t row = y; row < y + size; row++) {
        for (int32_t column = x; column < x + size; column++) {
            UnitId holder;
            if (!units_standing(units, unit->mover == UNIT_FLIES, column,
                    row, &holder) || holder.slot == id.slot) {
                continue;
            }
            const Unit *other = units_get(units, holder);
            if (other != NULL && (other->going || other->told
                    || other->left > 0)) {
                return true;
            }
        }
    }
    return false;
}

/* Picks the next step of a unit that is going somewhere and stands:
 * tells it the way, or counts a tick of waiting, or ends its going.
 * Returns whether it is still going. */
static bool next_step(Units *units, Fields *fields, const Terrain *terrain,
                      UnitId id, Unit *unit)
{
    const Field *field = fields_get(fields, unit->field);
    int32_t x = unit->x / UNIT_PLACE_PARTS;
    int32_t y = unit->y / UNIT_PLACE_PARTS;
    uint32_t here = field == NULL ? FIELD_FAR : field_far(field, x, y);
    uint32_t nearest = here;
    bool worth_waiting = false;
    /* There, or no way there at all. */
    if (here == 0 || here == FIELD_FAR) {
        walk_stop(units, fields, id);
        return false;
    }
    for (uint8_t way = 0; way < WALK_WAYS; way++) {
        int32_t to_x = x + walk_across(way);
        int32_t to_y = y + walk_down(way);
        uint32_t far = field_far(field, to_x, to_y);
        if (far >= here || !walk_open(terrain, unit, x, y, way)) {
            continue;
        }
        if (!units_can_step(units, id, to_x, to_y)) {
            /* A nearer square that a unit holds: it may leave. */
            worth_waiting = worth_waiting
                || held_by_one_on_its_way(units, id, unit, to_x, to_y);
        } else if (far < nearest) {
            nearest = far;
            unit->way = way;
        }
    }
    if (nearest < here) {
        unit->told = true;
        unit->waited = 0;
        return true;
    }
    if (worth_waiting && ++unit->waited <= WALK_PATIENCE) {
        return true;
    }
    /* It has waited long enough, or every nearer square is held by a
     * unit that stays. Then it goes for a free square nearer to its
     * goal than it is now, if there is one, a few times at most; else
     * this is as near as it gets. */
    int32_t across = x - unit->goal_x;
    int32_t down = y - unit->goal_y;
    int32_t ring = (across < 0 ? -across : across)
        > (down < 0 ? -down : down) ? (across < 0 ? -across : across)
        : (down < 0 ? -down : down);
    uint8_t tries = unit->beside;
    if (!worth_waiting && tries < WALK_TRIES_BESIDE
        && send(units, fields, terrain, id, unit->goal_x, unit->goal_y,
            true, ring)) {
        unit->beside = (uint8_t)(tries + 1);
        return true;
    }
    walk_stop(units, fields, id);
    return false;
}

uint16_t walk_ticks(uint8_t stride)
{
    uint32_t each = stride == 0 ? WALK_STRIDE_LEAST : stride;
    return (uint16_t)((UNIT_STEP + each - 1) / each);
}

/* A unit that does not slide in this tick stands. */
static void stand(Unit *unit)
{
    unit->ticks = 0;
    unit->walked = 0;
}

bool walk_tick(Units *units, UnitId id, const Terrain *terrain,
               Fields *fields)
{
    Unit *unit = units_get(units, id);
    if (unit == NULL) {
        return false;
    }
    if (unit->left > 0) {
        /* A slide goes on to its end, whatever else is. Half-way the
         * unit is on its new square, and the old one is free. */
        unit->left--;
        unit->walked++;
        if (unit->left <= unit->ticks / 2) {
            units_arrive(units, id);
        }
        return true;
    }
    if (unit->going && !unit->told
        && !next_step(units, fields, terrain, id, unit)) {
        stand(unit);
        return false;
    }
    if (!unit->told) {
        /* With nothing to do, or waiting for a square. */
        stand(unit);
        return unit->going;
    }
    /* First it faces the way. */
    uint8_t facing = walk_facing(unit->way);
    if (unit->facing != facing) {
        unit->facing_wanted = facing;
        unit_turn(unit);
        stand(unit);
        return true;
    }
    /* Then the step is looked at and taken at once, or dropped. */
    int32_t x = unit->x / UNIT_PLACE_PARTS;
    int32_t y = unit->y / UNIT_PLACE_PARTS;
    unit->told = false;
    if (!walk_open(terrain, unit, x, y, unit->way)
        || !units_step(units, id, x + walk_across(unit->way),
            y + walk_down(unit->way))) {
        stand(unit);
        return unit->going;
    }
    /* It slides from this tick on: the first part of the way is gone
     * already. */
    unit->ticks = walk_ticks(walk_stride(terrain, unit, x, y, unit->way));
    unit->left = (uint16_t)(unit->ticks - 1);
    unit->walked++;
    if (unit->left <= unit->ticks / 2) {
        units_arrive(units, id);
    }
    return true;
}
