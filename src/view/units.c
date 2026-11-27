#include "view/units.h"

#include "view/ground.h"
#include "view/light.h"

/* A unit's place is in these parts of a square. */
#define PLACE_PARTS 256

/* Which of its sheet's palettes a unit is drawn in. */
static uint16_t palette_of(const Map *map, const MapUnit *unit,
                           const UnitLook *look, const uint16_t *faces)
{
    if (!look->by_player) {
        /* Faces are counted from 1. */
        uint16_t face = unit->kind < 0 || unit->kind >= UNIT_KIND_IDS
            ? 0 : faces[unit->kind];
        return face == 0 ? 0 : (uint16_t)(face - 1);
    }
    if (unit->player < 1 || (uint32_t)unit->player > map->player_count) {
        return 0;
    }
    int32_t colour = map->players[unit->player - 1].colour;
    return colour < 0 || colour > UINT16_MAX ? 0 : (uint16_t)colour;
}

size_t units_most(const Map *map)
{
    return map->unit_count;
}

size_t units_sprites(const Map *map, const UnitLook *looks,
                     const uint16_t *faces, const Camera *camera,
                     Sprite *out)
{
    size_t count = 0;
    for (uint32_t i = 0; i < map->unit_count; i++) {
        const MapUnit *unit = &map->units[i];
        if (unit->type >= UNIT_TYPES || !looks[unit->type].known
            || unit->angle < 0 || unit->angle >= UNIT_FACINGS
            || unit->x < 0 || unit->y < 0) {
            continue;
        }
        const UnitLook *look = &looks[unit->type];
        int32_t place_x = unit->x * MAP_SQUARE / PLACE_PARTS;
        int32_t place_y = unit->y * MAP_SQUARE / PLACE_PARTS;
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
        sprite->source_x = (uint16_t)(unit->angle * look->cell_width);
        sprite->source_y = 0;
        sprite->width = look->cell_width;
        sprite->height = look->cell_height;
        sprite->x = (int16_t)left;
        sprite->y = (int16_t)top;
        sprite->light = LIGHT_STANDING;
        sprite->palette = palette_of(map, unit, look, faces);
        sprite->order = sprite_order(unit->x / PLACE_PARTS,
            unit->y / PLACE_PARTS);
    }
    return count;
}
