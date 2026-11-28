#include "view/selection.h"

#include <stdlib.h>
#include <string.h>

#include "view/frame.h"

#define SLOTS ((size_t)UNITS_MOST + 1)

bool selection_make(Selection *selection)
{
    selection->chosen = calloc(SLOTS, sizeof *selection->chosen);
    return selection->chosen != NULL;
}

void selection_free(Selection *selection)
{
    free(selection->chosen);
    selection->chosen = NULL;
}

void selection_clear(Selection *selection)
{
    memset(selection->chosen, 0, SLOTS * sizeof *selection->chosen);
}

bool selection_has(const Selection *selection, UnitId id)
{
    return selection != NULL
        && selection->chosen[id.slot] == (uint32_t)id.age + 1;
}

void selection_put(Selection *selection, UnitId id, bool chosen)
{
    if (chosen) {
        selection->chosen[id.slot] = (uint32_t)id.age + 1;
    } else if (selection_has(selection, id)) {
        selection->chosen[id.slot] = 0;
    }
}

uint32_t selection_count(const Selection *selection, const Units *units)
{
    uint32_t count = 0;
    for (uint32_t slot = 0; slot < units->count; slot++) {
        UnitId id;
        if (units_at(units, slot, &id) != NULL
            && selection_has(selection, id)) {
            count++;
        }
    }
    return count;
}

/* The sides of a drag: the right and the low one are past its last
 * pixel. */
typedef struct Sides {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
} Sides;

static Sides sides_of(const SelectionDrag *drag)
{
    Sides sides;
    sides.left = drag->from_x < drag->to_x ? drag->from_x : drag->to_x;
    sides.right = drag->from_x < drag->to_x ? drag->to_x : drag->from_x;
    sides.top = drag->from_y < drag->to_y ? drag->from_y : drag->to_y;
    sides.bottom = drag->from_y < drag->to_y ? drag->to_y : drag->from_y;
    return sides;
}

bool selection_drag_is_box(const SelectionDrag *drag, const Camera *camera)
{
    Sides sides = sides_of(drag);
    int32_t least = camera->width / 64;
    return sides.right - sides.left > least
        || sides.bottom - sides.top > least;
}

/* Whether a box catches the unit of a slot, and which unit that is. */
static bool catches(const Sides *sides, const Map *map, const Units *units,
                    uint32_t slot, const UnitLook *looks, uint32_t between,
                    const Camera *camera, int32_t player, UnitId *id)
{
    int32_t x = 0;
    int32_t y = 0;
    const Unit *unit = units_at(units, slot, id);
    const UnitLook *look = unit == NULL
        || (player != 0 && unit->player != player) ? NULL
        : unit_on_screen(map, unit, looks, between, camera, &x, &y);
    if (look == NULL) {
        return false;
    }
    const SpriteBox *box = &look->selection;
    int32_t left = x + box->left > sides->left ? x + box->left : sides->left;
    int32_t right = x + box->right < sides->right ? x + box->right
        : sides->right;
    int32_t top = y + box->top > sides->top ? y + box->top : sides->top;
    int32_t bottom = y + box->bottom < sides->bottom ? y + box->bottom
        : sides->bottom;
    return right > left && bottom > top
        && (int64_t)(right - left) * (bottom - top)
            > SELECTION_LEAST_OVERLAP;
}

bool selection_box(Selection *selection, const Map *map, const Units *units,
                   const UnitLook *looks, uint32_t between,
                   const Camera *camera, const SelectionDrag *drag,
                   int32_t player, bool toggle)
{
    Sides sides = sides_of(drag);
    bool any = false;
    for (uint32_t slot = 0; slot < units->count; slot++) {
        UnitId id;
        if (!catches(&sides, map, units, slot, looks, between, camera,
                player, &id)) {
            continue;
        }
        if (toggle) {
            selection_put(selection, id, !selection_has(selection, id));
        } else {
            /* The first one caught lets go of all there were. */
            if (!any) {
                selection_clear(selection);
            }
            selection_put(selection, id, true);
        }
        any = true;
    }
    return any;
}

size_t selection_outline_most(const Camera *camera)
{
    return 2 * ((size_t)camera->width / SELECTION_SHEET_SIDE + 1)
        + 2 * ((size_t)camera->height / SELECTION_SHEET_SIDE + 1);
}

/* Writes a line from (x, y), `length` pixels long: across, or down. */
static size_t line(uint16_t sheet, int32_t x, int32_t y, int32_t length,
                   bool across, Sprite *out)
{
    size_t count = 0;
    for (int32_t done = 0; done < length; done += SELECTION_SHEET_SIDE) {
        int32_t part = length - done < SELECTION_SHEET_SIDE ? length - done
            : SELECTION_SHEET_SIDE;
        Sprite *sprite = &out[count++];
        sprite->sheet = sheet;
        sprite->source_x = 0;
        sprite->source_y = 0;
        sprite->width = (uint16_t)(across ? part : 1);
        sprite->height = (uint16_t)(across ? 1 : part);
        sprite->x = (int16_t)(across ? x + done : x);
        sprite->y = (int16_t)(across ? y : y + done);
        sprite->light = FRAME_LIGHT_WHOLE;
        sprite->palette = 0;
        sprite->order = SPRITE_ORDER_BOX;
    }
    return count;
}

size_t selection_outline(uint16_t sheet, const SelectionDrag *drag,
                         const Camera *camera, Sprite *out)
{
    Sides sides = sides_of(drag);
    /* Kept to the screen: the mouse does not leave it. */
    sides.left = sides.left < 0 ? 0 : sides.left;
    sides.top = sides.top < 0 ? 0 : sides.top;
    sides.right = sides.right >= camera->width ? camera->width - 1
        : sides.right;
    sides.bottom = sides.bottom >= camera->height ? camera->height - 1
        : sides.bottom;
    if (sides.right < sides.left || sides.bottom < sides.top) {
        return 0;
    }
    int32_t wide = sides.right - sides.left + 1;
    int32_t high = sides.bottom - sides.top + 1;
    size_t count = line(sheet, sides.left, sides.top, wide, true, out);
    count += line(sheet, sides.left, sides.bottom, wide, true, out + count);
    count += line(sheet, sides.left, sides.top, high, false, out + count);
    count += line(sheet, sides.right, sides.top, high, false, out + count);
    return count;
}
