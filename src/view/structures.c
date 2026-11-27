#include "view/structures.h"

#include "view/ground.h"
#include "view/light.h"

/* The look of a structure of the map, or NULL if it is not drawn. */
static const StructureLook *look_of(const MapStructure *structure,
                                    const StructureLook *looks)
{
    if (structure->type >= STRUCTURE_KINDS || !looks[structure->type].known
        || structure->bridge_width != 0 || structure->bridge_height != 0) {
        return NULL;
    }
    return &looks[structure->type];
}

size_t structures_most(const Map *map, const StructureLook *looks)
{
    size_t most = 0;
    for (uint32_t i = 0; i < map->structure_count; i++) {
        const StructureLook *look = look_of(&map->structures[i], looks);
        most += look == NULL ? 0 : look->depth;
    }
    return most;
}

size_t structures_sprites(const Map *map, const StructureLook *looks,
                          const Camera *camera, Sprite *out)
{
    size_t count = 0;
    for (uint32_t i = 0; i < map->structure_count; i++) {
        const MapStructure *structure = &map->structures[i];
        const StructureLook *look = look_of(structure, looks);
        if (look == NULL) {
            continue;
        }
        int32_t width = (int32_t)look->width * MAP_SQUARE;
        /* How far the picture rises above the back of the base. */
        int32_t above = (int32_t)(look->height - look->depth) * MAP_SQUARE;
        int32_t left = structure->x * MAP_SQUARE;
        int32_t back = structure->y * MAP_SQUARE;
        int32_t raised = ground_height_at(map, left + width / 2,
            back + (int32_t)look->depth * MAP_SQUARE / 2);
        for (int32_t row = 0; row < (int32_t)look->depth; row++) {
            /* The strip of the back row starts at the picture's top. */
            int32_t over = row == 0 ? above : 0;
            int32_t height = MAP_SQUARE + over;
            int32_t x = left - camera->left;
            int32_t y = back + row * MAP_SQUARE - over - raised
                - camera->top;
            if (x >= camera->width || y >= camera->height
                || x + width <= 0 || y + height <= 0) {
                continue;
            }
            Sprite *sprite = &out[count++];
            sprite->sheet = look->sheet;
            sprite->source_x = 0;
            sprite->source_y = (uint16_t)(above + row * MAP_SQUARE - over);
            sprite->width = (uint16_t)width;
            sprite->height = (uint16_t)height;
            sprite->x = (int16_t)x;
            sprite->y = (int16_t)y;
            sprite->light = LIGHT_STANDING;
            /* After what stands to the right of it in the row, and
             * before what stands to the left. */
            sprite->order = sprite_order(
                structure->x + (int32_t)look->width - 1, structure->y + row);
        }
    }
    return count;
}
