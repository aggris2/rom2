#include "view/marks.h"

#include <stddef.h>

#include "view/frame.h"

void marks_closed(const Terrain *terrain, uint8_t whom, uint8_t *marks)
{
    size_t squares = (size_t)terrain->width * terrain->height;
    for (size_t i = 0; i < squares; i++) {
        marks[i] = (terrain->closed[i] & whom) != 0 ? FRAME_MARK_CLOSED
            : FRAME_MARK_NONE;
    }
}

/* Marks, or gives back, the squares a unit covers with its first
 * square on (x, y). */
static void mark_at(const Units *units, const Unit *unit, int32_t x,
                    int32_t y, bool held, const uint8_t *under,
                    uint8_t *marks)
{
    int32_t size = unit->size == 0 ? 1 : unit->size;
    for (int32_t row = y; row < y + size; row++) {
        for (int32_t column = x; column < x + size; column++) {
            if (column < 0 || row < 0 || (uint32_t)column >= units->width
                || (uint32_t)row >= units->height) {
                continue;
            }
            size_t at = (size_t)row * units->width + (size_t)column;
            marks[at] = held ? FRAME_MARK_HELD : under[at];
        }
    }
}

void marks_held(const Units *units, UnitId id, bool held,
                const uint8_t *under, uint8_t *marks)
{
    const Unit *unit = units_get(units, id);
    if (unit == NULL || unit->x < 0 || unit->y < 0) {
        return;
    }
    mark_at(units, unit, unit->x / UNIT_PLACE_PARTS,
        unit->y / UNIT_PLACE_PARTS, held, under, marks);
    if (unit->reaching) {
        mark_at(units, unit, unit->to_x, unit->to_y, held, under, marks);
    }
}

void marks_seen(const uint8_t *seen, size_t squares, uint8_t *marks)
{
    for (size_t i = 0; i < squares; i++) {
        if (seen[i] != 0 && marks[i] == FRAME_MARK_NONE) {
            marks[i] = FRAME_MARK_SEEN;
        }
    }
}
