#include "view/units.h"

#include "view/ground.h"
#include "view/light.h"

/* Which of its sheet's palettes a unit is drawn in. */
static uint16_t palette_of(const Map *map, const Unit *unit,
                           const UnitLook *look, const UnitKind *kinds)
{
    if (!look->by_player) {
        /* Faces are counted from 1. */
        uint16_t face = unit->kind < 0 || unit->kind >= UNIT_KIND_IDS
            ? 0 : kinds[unit->kind].face;
        return face == 0 ? 0 : (uint16_t)(face - 1);
    }
    if (unit->player < 1 || (uint32_t)unit->player > map->player_count) {
        return 0;
    }
    int32_t colour = map->players[unit->player - 1].colour;
    return colour < 0 || colour > UINT16_MAX ? 0 : (uint16_t)colour;
}

size_t units_most(const Units *units)
{
    return units->living;
}

size_t units_sprites(const Map *map, const Units *units,
                     const UnitLook *looks, const UnitKind *kinds,
                     uint64_t tick, const Camera *camera, Sprite *out)
{
    size_t count = 0;
    for (uint32_t slot = 0; slot < units->count; slot++) {
        const Unit *unit = units_at(units, slot, NULL);
        if (unit == NULL || unit->type >= UNIT_TYPES
            || !looks[unit->type].known || unit->x < 0 || unit->y < 0) {
            continue;
        }
        const UnitLook *look = &looks[unit->type];
        int32_t place_x = unit->x * MAP_SQUARE / UNIT_PLACE_PARTS;
        int32_t place_y = unit->y * MAP_SQUARE / UNIT_PLACE_PARTS;
        int32_t left = place_x - look->anchor_x - camera->left;
        int32_t top = place_y - look->anchor_y - camera->top
            - ground_height_at(map, place_x, place_y);
        if (left >= camera->width || top >= camera->height
            || left + (int32_t)look->cell_width <= 0
            || top + (int32_t)look->cell_height <= 0) {
            continue;
        }
        Sprite *sprite = &out[count++];
        sprite->sheet = look->sheet;
        int32_t shown = unit->facing / (UNIT_TURN / UNIT_FACINGS);
        int32_t frame = (shown + UNIT_FACINGS / 2) % UNIT_FACINGS;
        int32_t row = 0;
        if (look->idle.round.length > 0) {
            /* Two standing frames to each row of an animation. */
            row = 1 + frame / (UNIT_FACINGS / UNIT_ANIMATION_FACINGS);
            frame = look->idle.column
                + (int32_t)sprite_round_frame(&look->idle.round, tick);
        }
        sprite->source_x = (uint16_t)(frame * look->cell_width);
        sprite->source_y = (uint16_t)(row * look->cell_height);
        sprite->width = look->cell_width;
        sprite->height = look->cell_height;
        sprite->x = (int16_t)left;
        sprite->y = (int16_t)top;
        sprite->light = LIGHT_STANDING;
        sprite->palette = palette_of(map, unit, look, kinds);
        sprite->order = sprite_order(unit->x / UNIT_PLACE_PARTS,
            unit->y / UNIT_PLACE_PARTS);
    }
    return count;
}
