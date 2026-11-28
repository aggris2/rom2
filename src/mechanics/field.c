#include "mechanics/field.h"

#include <stdlib.h>
#include <string.h>

/* The lines of squares that wait, one for each distance, go round in
 * a ring: a step costs less than this, so a square that is put in
 * line is never as far ahead as the ring is long. */
#define RING 1024u
#define NONE UINT32_MAX

static const int8_t across_of[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int8_t down_of[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

/* Gives the fields room for a spread over `squares`. */
static bool spread_room(Fields *fields, uint32_t squares)
{
    if (fields->squares == squares && fields->next != NULL) {
        return true;
    }
    uint32_t *next = malloc((size_t)(squares > 0 ? squares : 1)
        * sizeof *next);
    uint32_t *before = malloc((size_t)(squares > 0 ? squares : 1)
        * sizeof *before);
    uint32_t *first = malloc(RING * sizeof *first);
    if (next == NULL || before == NULL || first == NULL) {
        free(next);
        free(before);
        free(first);
        return false;
    }
    free(fields->next);
    free(fields->before);
    free(fields->first);
    fields->next = next;
    fields->before = before;
    fields->first = first;
    fields->squares = squares;
    return true;
}

/* Takes a square out of the line of its distance. */
static void leave_line(Fields *fields, uint32_t square, uint32_t far)
{
    uint32_t next = fields->next[square];
    uint32_t before = fields->before[square];
    if (before == NONE) {
        fields->first[far % RING] = next;
    } else {
        fields->next[before] = next;
    }
    if (next != NONE) {
        fields->before[next] = before;
    }
}

/* Puts a square at the head of the line of a distance. */
static void join_line(Fields *fields, uint32_t square, uint32_t far)
{
    uint32_t head = fields->first[far % RING];
    fields->next[square] = head;
    fields->before[square] = NONE;
    if (head != NONE) {
        fields->before[head] = square;
    }
    fields->first[far % RING] = square;
}

/* Whether a unit of `size` could stand with its first square on
 * (x, y) as far as the ground goes. */
static bool open_at(const Terrain *terrain, uint8_t closed_to, int32_t size,
                    int32_t x, int32_t y)
{
    if (x < 0 || y < 0 || (uint32_t)(x + size) > terrain->width
        || (uint32_t)(y + size) > terrain->height) {
        return false;
    }
    for (int32_t row = y; row < y + size; row++) {
        for (int32_t column = x; column < x + size; column++) {
            if ((terrain->closed[(size_t)row * terrain->width
                    + (size_t)column] & closed_to) != 0) {
                return false;
            }
        }
    }
    return true;
}

bool field_spread(Fields *fields, Field *field, const Terrain *terrain,
                  uint8_t closed_to, uint8_t size, int32_t goal_x,
                  int32_t goal_y)
{
    uint32_t squares = terrain->width * terrain->height;
    int32_t side = size == 0 ? 1 : size;
    if (!spread_room(fields, squares)) {
        return false;
    }
    if (field->far == NULL || field->width != terrain->width
        || field->height != terrain->height) {
        uint32_t *far = malloc((size_t)(squares > 0 ? squares : 1)
            * sizeof *far);
        if (far == NULL) {
            return false;
        }
        free(field->far);
        field->far = far;
        field->width = terrain->width;
        field->height = terrain->height;
    }
    field->goal_x = goal_x;
    field->goal_y = goal_y;
    field->closed_to = closed_to;
    field->size = (uint8_t)side;
    for (uint32_t i = 0; i < squares; i++) {
        field->far[i] = FIELD_FAR;
    }
    for (uint32_t i = 0; i < RING; i++) {
        fields->first[i] = NONE;
    }
    if (!open_at(terrain, closed_to, side, goal_x, goal_y)) {
        return true;
    }
    bool walks = closed_to == TERRAIN_NO_WALK;
    uint32_t goal = (uint32_t)goal_y * terrain->width + (uint32_t)goal_x;
    uint32_t waiting = 1;
    field->far[goal] = 0;
    join_line(fields, goal, 0);
    /* The lines are gone through in the order of their distances: a
     * square that comes to the head of its line is as near as it will
     * ever be. */
    for (uint32_t far = 0; waiting > 0; far++) {
        while (fields->first[far % RING] != NONE) {
            uint32_t at = fields->first[far % RING];
            int32_t x = (int32_t)(at % terrain->width);
            int32_t y = (int32_t)(at / terrain->width);
            leave_line(fields, at, far);
            waiting--;
            /* A step from a square beside this one onto it costs by
             * this one. */
            uint32_t cost = walks ? terrain->cost[at] : FIELD_COST_FLAT;
            for (int way = 0; way < 8; way++) {
                int32_t from_x = x + across_of[way];
                int32_t from_y = y + down_of[way];
                bool corner = across_of[way] != 0 && down_of[way] != 0;
                if (!open_at(terrain, closed_to, side, from_x, from_y)) {
                    continue;
                }
                uint32_t from = (uint32_t)from_y * terrain->width
                    + (uint32_t)from_x;
                uint32_t there = far
                    + cost * (corner ? FIELD_CORNER : FIELD_STRAIGHT);
                if (there >= field->far[from]) {
                    continue;
                }
                if (field->far[from] != FIELD_FAR) {
                    leave_line(fields, from, field->far[from]);
                    waiting--;
                }
                field->far[from] = there;
                join_line(fields, from, there);
                waiting++;
            }
        }
    }
    return true;
}

uint32_t field_far(const Field *field, int32_t x, int32_t y)
{
    if (field->far == NULL || x < 0 || y < 0 || (uint32_t)x >= field->width
        || (uint32_t)y >= field->height) {
        return FIELD_FAR;
    }
    return field->far[(size_t)y * field->width + (size_t)x];
}

bool fields_take(Fields *fields, const Terrain *terrain, uint8_t closed_to,
                 uint8_t size, int32_t goal_x, int32_t goal_y,
                 uint16_t *out)
{
    int32_t spare = -1;
    uint8_t side = size == 0 ? 1 : size;
    for (int32_t i = 0; i < FIELDS_MOST; i++) {
        const Field *field = &fields->field[i];
        if (fields->users[i] > 0 && field->goal_x == goal_x
            && field->goal_y == goal_y && field->closed_to == closed_to
            && field->size == side
            && field->width == terrain->width
            && field->height == terrain->height) {
            fields->users[i]++;
            *out = (uint16_t)i;
            return true;
        }
        if (fields->users[i] == 0 && spare < 0) {
            spare = i;
        }
    }
    if (spare < 0 || !field_spread(fields, &fields->field[spare], terrain,
            closed_to, side, goal_x, goal_y)) {
        return false;
    }
    fields->users[spare] = 1;
    *out = (uint16_t)spare;
    return true;
}

void fields_drop(Fields *fields, uint16_t number)
{
    if (number < FIELDS_MOST && fields->users[number] > 0) {
        fields->users[number]--;
    }
}

void fields_no_users(Fields *fields)
{
    memset(fields->users, 0, sizeof fields->users);
}

void fields_user(Fields *fields, uint16_t number)
{
    if (number < FIELDS_MOST) {
        fields->users[number]++;
    }
}

bool fields_respread(Fields *fields, const Terrain *terrain)
{
    bool all = true;
    for (int32_t i = 0; i < FIELDS_MOST; i++) {
        Field *field = &fields->field[i];
        if (fields->users[i] > 0 && !field_spread(fields, field, terrain,
                field->closed_to, field->size, field->goal_x,
                field->goal_y)) {
            all = false;
        }
    }
    return all;
}

const Field *fields_get(const Fields *fields, uint16_t number)
{
    return number < FIELDS_MOST && fields->users[number] > 0
        ? &fields->field[number] : NULL;
}

void fields_free(Fields *fields)
{
    for (int32_t i = 0; i < FIELDS_MOST; i++) {
        free(fields->field[i].far);
    }
    free(fields->next);
    free(fields->before);
    free(fields->first);
    memset(fields, 0, sizeof *fields);
}
