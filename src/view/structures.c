#include "view/structures.h"

#include "view/ground.h"
#include "view/light.h"

/* How many pieces a row of a bridge's sheet has for the first square,
 * those between and the last: and it has as many rows of them. */
#define PIECE_PARTS 3

/* The look of a structure of the map, or NULL if it is not drawn. */
static const StructureLook *look_of(const MapStructure *structure,
                                    const StructureLook *looks)
{
    if (structure->type >= STRUCTURE_KINDS
        || !looks[structure->type].known) {
        return NULL;
    }
    const StructureLook *look = &looks[structure->type];
    if (look->pieces && (structure->bridge_width < 1
            || structure->bridge_height < 1)) {
        return NULL;
    }
    return look;
}

size_t structures_most(const Map *map, const StructureLook *looks)
{
    size_t most = 0;
    for (uint32_t i = 0; i < map->structure_count; i++) {
        const MapStructure *structure = &map->structures[i];
        const StructureLook *look = look_of(structure, looks);
        if (look == NULL) {
            continue;
        }
        most += look->pieces ? (size_t)structure->bridge_width
            * (size_t)structure->bridge_height : look->depth;
    }
    return most;
}

/* Which of the three parts the square `at` of `count` in a line is:
 * the first, one between, or the last. */
static int32_t part_of(int32_t at, int32_t count)
{
    return at == 0 ? 0 : at == count - 1 ? 2 : 1;
}

/* Writes the sprites of a bridge, a piece for each of its squares the
 * camera can see, and returns how many. */
static size_t bridge_sprites(const Map *map, const MapStructure *bridge,
                             const StructureLook *look,
                             const Camera *camera, Sprite *out)
{
    size_t count = 0;
    int32_t left = bridge->x * MAP_SQUARE;
    int32_t top = bridge->y * MAP_SQUARE;
    int32_t raised = ground_height_at(map,
        left + bridge->bridge_width * MAP_SQUARE / 2,
        top + bridge->bridge_height * MAP_SQUARE / 2);
    for (int32_t row = 0; row < bridge->bridge_height; row++) {
        for (int32_t column = 0; column < bridge->bridge_width; column++) {
            int32_t x = left + column * MAP_SQUARE - camera->left;
            int32_t y = top + row * MAP_SQUARE - raised - camera->top;
            if (x >= camera->width || y >= camera->height
                || x + MAP_SQUARE <= 0 || y + MAP_SQUARE <= 0) {
                continue;
            }
            int32_t piece = part_of(row, bridge->bridge_height)
                * PIECE_PARTS + part_of(column, bridge->bridge_width);
            Sprite *sprite = &out[count++];
            sprite->sheet = look->sheet;
            sprite->source_x = (uint16_t)(piece * MAP_SQUARE);
            sprite->source_y = 0;
            sprite->width = MAP_SQUARE;
            sprite->height = MAP_SQUARE;
            sprite->x = (int16_t)x;
            sprite->y = (int16_t)y;
            sprite->light = LIGHT_STANDING;
            sprite->palette = 0;
            sprite->order = SPRITE_ORDER_FLAT;
        }
    }
    return count;
}

size_t structures_sprites(const Map *map, const StructureLook *looks,
                          uint64_t tick, const Camera *camera,
                          Sprite *out)
{
    size_t count = 0;
    for (uint32_t i = 0; i < map->structure_count; i++) {
        const MapStructure *structure = &map->structures[i];
        const StructureLook *look = look_of(structure, looks);
        if (look == NULL) {
            continue;
        }
        if (look->pieces) {
            count += bridge_sprites(map, structure, look, camera,
                out + count);
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
            sprite->source_x = (uint16_t)(
                sprite_round_frame(&look->round, tick) * (uint32_t)width);
            sprite->source_y = (uint16_t)(above + row * MAP_SQUARE - over);
            sprite->width = (uint16_t)width;
            sprite->height = (uint16_t)height;
            sprite->x = (int16_t)x;
            sprite->y = (int16_t)y;
            sprite->light = LIGHT_STANDING;
            sprite->palette = 0;
            /* After what stands to the right of it in the row, and
             * before what stands to the left. */
            sprite->order = sprite_order(
                structure->x + (int32_t)look->width - 1, structure->y + row);
        }
    }
    return count;
}
