#include "view/ground.h"

/* The square a pixel of the map is in, also left of and above the
 * map, where plain division would round the wrong way. */
static int32_t square_of(int32_t pixel)
{
    return pixel >= 0 ? pixel / CAMERA_SQUARE
        : -((-pixel + CAMERA_SQUARE - 1) / CAMERA_SQUARE);
}

size_t ground_most(const Camera *camera)
{
    /* A screen that does not start on the edge of a square shows a
     * part of one more at each end. */
    size_t across = camera->width <= 0 ? 0
        : (size_t)(camera->width / CAMERA_SQUARE) + 2;
    size_t down = camera->height <= 0 ? 0
        : (size_t)(camera->height / CAMERA_SQUARE) + 2;
    return across * down;
}

size_t ground_sprites(const Map *map, const Camera *camera, uint16_t sheet,
                      uint32_t sheet_width, uint32_t sheet_height,
                      Sprite *out)
{
    if (camera->width <= 0 || camera->height <= 0) {
        return 0;
    }
    int32_t first_x = square_of(camera->left);
    int32_t first_y = square_of(camera->top);
    int32_t last_x = square_of(camera->left + camera->width - 1);
    int32_t last_y = square_of(camera->top + camera->height - 1);
    first_x = first_x < 0 ? 0 : first_x;
    first_y = first_y < 0 ? 0 : first_y;
    last_x = last_x >= (int32_t)map->info.width
        ? (int32_t)map->info.width - 1 : last_x;
    last_y = last_y >= (int32_t)map->info.height
        ? (int32_t)map->info.height - 1 : last_y;

    size_t count = 0;
    for (int32_t y = first_y; y <= last_y; y++) {
        for (int32_t x = first_x; x <= last_x; x++) {
            /* The picture, then the tile in it: see
             * docs/formats/terrain.md. */
            uint16_t tile = map->ground[(size_t)y * map->info.width
                + (size_t)x];
            uint32_t source_x = ((tile >> 4) & 0xffu) * CAMERA_SQUARE;
            uint32_t source_y = (tile & 0xfu) * CAMERA_SQUARE;
            if (source_x + CAMERA_SQUARE > sheet_width
                || source_y + CAMERA_SQUARE > sheet_height) {
                continue;
            }
            Sprite *sprite = &out[count++];
            sprite->sheet = sheet;
            sprite->source_x = (uint16_t)source_x;
            sprite->source_y = (uint16_t)source_y;
            sprite->width = CAMERA_SQUARE;
            sprite->height = CAMERA_SQUARE;
            sprite->x = (int16_t)(x * CAMERA_SQUARE - camera->left);
            sprite->y = (int16_t)(y * CAMERA_SQUARE - camera->top);
        }
    }
    return count;
}
