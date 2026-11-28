#include "mechanics/guard.h"

#include <stdlib.h>
#include <string.h>

#include "mechanics/sight.h"
#include "mechanics/walk.h"

/* A unit that joins a band, for putting them in the order of their
 * bands. */
typedef struct Joining {
    int32_t player;
    int32_t group;
    uint32_t slot;
} Joining;

static int by_band(const void *left, const void *right)
{
    const Joining *a = left;
    const Joining *b = right;
    if (a->player != b->player) {
        return a->player < b->player ? -1 : 1;
    }
    if (a->group != b->group) {
        return a->group < b->group ? -1 : 1;
    }
    return a->slot < b->slot ? -1 : a->slot > b->slot ? 1 : 0;
}

/* How far two squares are apart: across or down, whichever is
 * more. */
static int32_t far_apart(int32_t ax, int32_t ay, int32_t bx, int32_t by)
{
    int32_t across = ax < bx ? bx - ax : ax - bx;
    int32_t down = ay < by ? by - ay : ay - by;
    return across > down ? across : down;
}

static int32_t column_of(const Unit *unit)
{
    return unit->x / UNIT_PLACE_PARTS;
}

static int32_t row_of(const Unit *unit)
{
    return unit->y / UNIT_PLACE_PARTS;
}

static int32_t side_of(const Unit *unit)
{
    return unit->size == 0 ? 1 : unit->size;
}

void guard_free(Guard *guard)
{
    free(guard->bands);
    free(guard->seen);
    free(guard->present);
    free(guard->first);
    free(guard->members);
    free(guard->found);
    memset(guard, 0, sizeof *guard);
}

/* Counts the bands of units in the order of their bands. */
static uint32_t count_bands(const Joining *joining, uint32_t joined)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < joined; i++) {
        if (i == 0 || joining[i].player != joining[i - 1].player
            || joining[i].group != joining[i - 1].group) {
            count++;
        }
    }
    return count;
}

bool guard_make(Units *units, const Terrain *terrain, Random *random,
                Guard *out)
{
    Guard guard;
    memset(&guard, 0, sizeof guard);
    guard.width = terrain->width;
    guard.height = terrain->height;
    guard.parts_wide = (guard.width + GUARD_PART - 1) / GUARD_PART;
    guard.parts_high = (guard.height + GUARD_PART - 1) / GUARD_PART;
    size_t squares = (size_t)guard.width * guard.height;
    size_t parts = (size_t)guard.parts_wide * guard.parts_high;
    Joining *joining = malloc((units->count + 1) * sizeof *joining);
    uint32_t joined = 0;
    for (uint32_t slot = 0; joining != NULL && slot < units->count;
        slot++) {
        const Unit *unit = units_at(units, slot, NULL);
        if (unit != NULL && unit->group != 0 && unit->x >= 0 && unit->y >= 0
            && units_computer_plays(units, unit)) {
            joining[joined].player = unit->player;
            joining[joined].group = unit->group;
            joining[joined].slot = slot;
            joined++;
        }
    }
    if (joining != NULL) {
        qsort(joining, joined, sizeof *joining, by_band);
        guard.count = count_bands(joining, joined);
    }
    /* For each band the sums of its units' squares, and how many. */
    int64_t *sums = calloc((size_t)guard.count * 3 + 1, sizeof *sums);
    guard.bands = calloc((size_t)guard.count + 1, sizeof *guard.bands);
    guard.first = calloc((size_t)guard.count + 2, sizeof *guard.first);
    guard.seen = calloc(squares + 1, 1);
    guard.present = calloc(parts + 1, sizeof *guard.present);
    guard.members = malloc(((size_t)UNITS_MOST + 1) * sizeof *guard.members);
    guard.found = malloc(((size_t)UNITS_MOST + 1) * sizeof *guard.found);
    if (joining == NULL || sums == NULL || guard.bands == NULL
        || guard.first == NULL || guard.seen == NULL
        || guard.present == NULL || guard.members == NULL
        || guard.found == NULL) {
        free(joining);
        free(sums);
        guard_free(&guard);
        return false;
    }
    uint32_t band = 0;
    for (uint32_t i = 0; i < joined; i++) {
        if (i > 0 && (joining[i].player != joining[i - 1].player
                || joining[i].group != joining[i - 1].group)) {
            band++;
        }
        Unit *unit = units_at(units, joining[i].slot, NULL);
        guard.bands[band].player = joining[i].player;
        guard.bands[band].group = joining[i].group;
        unit->band = (uint16_t)(band + 1);
        unit->home_x = (int16_t)column_of(unit);
        unit->home_y = (int16_t)row_of(unit);
        sums[band * 3] += column_of(unit);
        sums[band * 3 + 1] += row_of(unit);
        sums[band * 3 + 2]++;
    }
    for (uint32_t i = 0; i < joined; i++) {
        const Unit *unit = units_at(units, joining[i].slot, NULL);
        const int64_t *sum = &sums[(size_t)(unit->band - 1) * 3];
        int32_t reach = far_apart(column_of(unit), row_of(unit),
            (int32_t)(sum[0] / sum[2]), (int32_t)(sum[1] / sum[2]))
            + unit->sight;
        GuardBand *of = &guard.bands[unit->band - 1];
        reach = reach > UINT8_MAX ? UINT8_MAX : reach;
        of->range = reach > of->range ? (uint8_t)reach : of->range;
    }
    for (uint32_t i = 0; i < guard.count; i++) {
        int32_t range = guard.bands[i].range;
        range = range < GUARD_RANGE_LEAST ? GUARD_RANGE_WIDE : range;
        range += GUARD_RANGE_MORE - 1 + (int32_t)random_below(random, 3);
        guard.bands[i].range = (uint8_t)(range > UINT8_MAX ? UINT8_MAX
            : range);
    }
    free(joining);
    free(sums);
    *out = guard;
    return true;
}

/* Sends a unit to stand with its first square on (x, y), unless it
 * is there or on its way there. */
static void go(Units *units, Fields *fields, const Terrain *terrain,
               UnitId id, const Unit *unit, int32_t x, int32_t y)
{
    int32_t middle = (side_of(unit) - 1) / 2;
    if (unit->going ? unit->goal_x == x && unit->goal_y == y
        : column_of(unit) == x && row_of(unit) == y) {
        return;
    }
    walk_send(units, fields, terrain, id, x + middle, y + middle);
}

/* Whether two units stand with no square between them. */
static bool beside(const Unit *unit, const Unit *other)
{
    int32_t across = column_of(other) - (column_of(unit) + side_of(unit) - 1);
    int32_t back = column_of(unit) - (column_of(other) + side_of(other) - 1);
    int32_t down = row_of(other) - (row_of(unit) + side_of(unit) - 1);
    int32_t up = row_of(unit) - (row_of(other) + side_of(other) - 1);
    return across <= 1 && back <= 1 && down <= 1 && up <= 1;
}

/* The facing of the way from a unit to another, or the unit's own if
 * the two are on one square. */
static uint8_t facing_to(const Unit *unit, const Unit *other)
{
    uint8_t way = 0;
    return walk_way_to(column_of(unit), row_of(unit), column_of(other),
        row_of(other), &way) ? walk_facing(way) : unit->facing;
}

/* How good an enemy is to go for: less is better. Nearer first, and
 * then the one that takes less of a turn, in eighths of a full one. */
static int32_t score_of(const Unit *unit, const Unit *enemy)
{
    uint8_t turn = (uint8_t)(facing_to(unit, enemy) - unit->facing);
    int32_t eighths = (turn > UNIT_TURN / 2 ? UNIT_TURN - turn : turn)
        / (UNIT_TURN / WALK_WAYS);
    return far_apart(column_of(unit), row_of(unit), column_of(enemy),
        row_of(enemy)) * 256 + eighths;
}

/* Finds the enemies of a band whose units are in the slots from
 * `begin` up to `end` of the members, and returns how many. */
static uint32_t find(Guard *guard, const Units *units,
                     const Terrain *terrain, const Relations *relations,
                     const GuardBand *band, uint32_t begin, uint32_t end)
{
    /* The band's middle, and the least box that holds it all. */
    int64_t sum_x = 0;
    int64_t sum_y = 0;
    int32_t left = INT32_MAX;
    int32_t top = INT32_MAX;
    int32_t right = 0;
    int32_t bottom = 0;
    for (uint32_t i = begin; i < end; i++) {
        const Unit *unit = units_at(units, guard->members[i], NULL);
        sum_x += column_of(unit);
        sum_y += row_of(unit);
        left = column_of(unit) < left ? column_of(unit) : left;
        top = row_of(unit) < top ? row_of(unit) : top;
        right = column_of(unit) > right ? column_of(unit) : right;
        bottom = row_of(unit) > bottom ? row_of(unit) : bottom;
    }
    int32_t middle_x = (int32_t)(sum_x / (end - begin));
    int32_t middle_y = (int32_t)(sum_y / (end - begin));
    /* Nothing is seen from further off than this. */
    left = left > SIGHT_FURTHEST ? left - SIGHT_FURTHEST : 0;
    top = top > SIGHT_FURTHEST ? top - SIGHT_FURTHEST : 0;
    right = right + SIGHT_FURTHEST < (int32_t)guard->width
        ? right + SIGHT_FURTHEST : (int32_t)guard->width - 1;
    bottom = bottom + SIGHT_FURTHEST < (int32_t)guard->height
        ? bottom + SIGHT_FURTHEST : (int32_t)guard->height - 1;
    uint32_t enemies = 0;
    for (int32_t player = 1; player <= (int32_t)RELATIONS_PLAYERS;
        player++) {
        if (relations_enemy(relations, band->player, player)) {
            enemies |= 1u << (player - 1);
        }
    }
    /* Is any of them about at all? */
    bool about = false;
    for (int32_t row = top / GUARD_PART; row <= bottom / GUARD_PART; row++) {
        for (int32_t column = left / GUARD_PART;
            column <= right / GUARD_PART; column++) {
            about = about || (guard->present[(size_t)row * guard->parts_wide
                + (size_t)column] & enemies) != 0;
        }
    }
    if (!about || right < left || bottom < top) {
        return 0;
    }
    for (int32_t row = top; row <= bottom; row++) {
        memset(guard->seen + (size_t)row * guard->width + (size_t)left, 0,
            (size_t)(right - left + 1));
    }
    for (uint32_t i = begin; i < end; i++) {
        const Unit *unit = units_at(units, guard->members[i], NULL);
        sight_mark(terrain, column_of(unit), row_of(unit), unit->sight,
            guard->seen);
    }
    uint32_t found = 0;
    for (int32_t row = top; row <= bottom; row++) {
        for (int32_t column = left; column <= right; column++) {
            if (guard->seen[(size_t)row * guard->width + (size_t)column] == 0
                || far_apart(middle_x, middle_y, column, row)
                    > band->range) {
                continue;
            }
            /* Who stands there, on the ground and in the air: a unit
             * of several squares counts on its first. */
            for (int air = 0; air < 2; air++) {
                UnitId id;
                const Unit *other = units_standing(units, air == 1, column,
                    row, &id) ? units_get(units, id) : NULL;
                if (other != NULL && column_of(other) == column
                    && row_of(other) == row && other->player >= 1
                    && other->player <= (int32_t)RELATIONS_PLAYERS
                    && (enemies >> (other->player - 1) & 1u) != 0) {
                    guard->found[found++] = id.slot;
                }
            }
        }
    }
    return found;
}

/* A band looks around: its units are in the slots from `begin` up to
 * `end` of the members. */
static void look(Guard *guard, Units *units, const Terrain *terrain,
                 Fields *fields, const Relations *relations,
                 const GuardBand *band, uint32_t begin, uint32_t end)
{
    uint32_t found = find(guard, units, terrain, relations, band, begin,
        end);
    for (uint32_t i = begin; i < end; i++) {
        UnitId id;
        Unit *unit = units_at(units, guard->members[i], &id);
        const Unit *enemy = NULL;
        int32_t best = INT32_MAX;
        for (uint32_t j = 0; j < found; j++) {
            const Unit *other = units_at(units, guard->found[j], NULL);
            int32_t score = score_of(unit, other);
            if (score < best) {
                best = score;
                enemy = other;
            }
        }
        if (enemy == NULL) {
            go(units, fields, terrain, id, unit, unit->home_x, unit->home_y);
        } else if (beside(unit, enemy)) {
            walk_stop(units, fields, id);
            unit->facing_wanted = facing_to(unit, enemy);
        } else {
            /* With its middle on the enemy's square. */
            int32_t middle = (side_of(unit) - 1) / 2;
            go(units, fields, terrain, id, unit, column_of(enemy) - middle,
                row_of(enemy) - middle);
        }
    }
}

void guard_turn(Guard *guard, Units *units, const Terrain *terrain,
                Fields *fields, const Relations *relations)
{
    if (guard->count == 0) {
        return;
    }
    memset(guard->first, 0, ((size_t)guard->count + 2) * sizeof *guard->first);
    memset(guard->present, 0, (size_t)guard->parts_wide * guard->parts_high
        * sizeof *guard->present);
    /* Who is where, and how many units each band has. */
    for (uint32_t slot = 0; slot < units->count; slot++) {
        const Unit *unit = units_at(units, slot, NULL);
        if (unit == NULL || unit->x < 0 || unit->y < 0
            || (uint32_t)column_of(unit) >= guard->width
            || (uint32_t)row_of(unit) >= guard->height) {
            continue;
        }
        if (unit->player >= 1
            && unit->player <= (int32_t)RELATIONS_PLAYERS) {
            guard->present[(size_t)(row_of(unit) / GUARD_PART)
                * guard->parts_wide + (size_t)(column_of(unit) / GUARD_PART)]
                |= (uint16_t)(1u << (unit->player - 1));
        }
        if (unit->band >= 1 && unit->band <= guard->count) {
            guard->first[unit->band + 1]++;
        }
    }
    /* The units of each band, one band after another. */
    for (uint32_t i = 2; i < guard->count + 2; i++) {
        guard->first[i] += guard->first[i - 1];
    }
    for (uint32_t slot = 0; slot < units->count; slot++) {
        const Unit *unit = units_at(units, slot, NULL);
        if (unit != NULL && unit->x >= 0 && unit->y >= 0
            && (uint32_t)column_of(unit) < guard->width
            && (uint32_t)row_of(unit) < guard->height && unit->band >= 1
            && unit->band <= guard->count) {
            guard->members[guard->first[unit->band]++] = (uint16_t)slot;
        }
    }
    for (uint32_t band = 0; band < guard->count; band++) {
        if (guard->first[band] < guard->first[band + 1]) {
            look(guard, units, terrain, fields, relations,
                &guard->bands[band], guard->first[band],
                guard->first[band + 1]);
        }
    }
}

void guard_tick(Guard *guard, Units *units, const Terrain *terrain,
                Fields *fields, const Relations *relations)
{
    if (guard->ticks == 0) {
        guard_turn(guard, units, terrain, fields, relations);
    }
    guard->ticks = (guard->ticks + 1) % GUARD_TURN_TICKS;
}
