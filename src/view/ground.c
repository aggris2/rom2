#include "view/ground.h"

/* The pictures of the sheet come in families of four, and the families
 * of water are these four, one after the other: see
 * docs/formats/terrain.md. */
#define FAMILY_PICTURES 4u
#define WATER_FIRST_FAMILY 8u
#define WATER_FAMILIES 4u

/* The rows below the screen that the highest ground can reach up
 * from. */
#define ROWS_BELOW ((MAP_HIGHEST + MAP_SQUARE - 1) / MAP_SQUARE)

/* The square a pixel of the map is in, also left of and above the
 * map, where plain division would round the wrong way. */
static int32_t square_of(int32_t pixel)
{
    return pixel >= 0 ? pixel / MAP_SQUARE
        : -((-pixel + MAP_SQUARE - 1) / MAP_SQUARE);
}

/* The place in the map's lists of the corner (`x`, `y`). One past the
 * right or the bottom edge counts as the last inside. */
static size_t corner_index(const Map *map, int32_t x, int32_t y)
{
    int32_t width = (int32_t)map->info.width;
    int32_t height = (int32_t)map->info.height;
    x = x < 0 ? 0 : x >= width ? width - 1 : x;
    y = y < 0 ? 0 : y >= height ? height - 1 : y;
    return (size_t)y * (size_t)width + (size_t)x;
}

size_t ground_most(const Camera *camera)
{
    /* A screen that does not start on the edge of a square shows a
     * part of one more at each end. */
    size_t across = camera->width <= 0 ? 0
        : (size_t)(camera->width / MAP_SQUARE) + 2;
    size_t down = camera->height <= 0 ? 0
        : (size_t)(camera->height / MAP_SQUARE) + 2 + ROWS_BELOW;
    return across * down;
}

/* The height at the corner (`x`, `y`) of the map. */
static int32_t corner_height(const Map *map, int32_t x, int32_t y)
{
    return map->heights[corner_index(map, x, y)];
}

int32_t ground_height_at(const Map *map, int32_t x, int32_t y)
{
    int32_t square_x = square_of(x);
    int32_t square_y = square_of(y);
    int32_t across = x - square_x * MAP_SQUARE;
    int32_t down = y - square_y * MAP_SQUARE;
    int32_t top_left = corner_height(map, square_x, square_y);
    int32_t top_right = corner_height(map, square_x + 1, square_y);
    int32_t low_left = corner_height(map, square_x, square_y + 1);
    int32_t low_right = corner_height(map, square_x + 1, square_y + 1);
    /* Division of whole numbers in C cuts towards zero, which is what
     * the original does here. */
    int32_t top = top_left + (top_right - top_left) * across / MAP_SQUARE;
    int32_t low = low_left + (low_right - low_left) * across / MAP_SQUARE;
    return top + (low - top) * down / MAP_SQUARE;
}

int32_t ground_square_height(const Map *map, int32_t column, int32_t row)
{
    int32_t sum = 0;
    for (int32_t i = 0; i < FRAME_SQUARE_CORNERS; i++) {
        sum += corner_height(map, column + (i & 1), row + (i >> 1));
    }
    return sum / FRAME_SQUARE_CORNERS;
}

/* The picture a square shows at a tick: the one the map names, or for
 * water the same picture of the family of water whose turn it is. */
static uint32_t picture_at(uint32_t picture, int32_t x, int32_t y,
                           uint64_t tick)
{
    uint32_t family = picture / FAMILY_PICTURES;
    if (family < WATER_FIRST_FAMILY
        || family >= WATER_FIRST_FAMILY + WATER_FAMILIES) {
        return picture;
    }
    uint64_t turn = family + (uint64_t)(x + 1) * (uint64_t)y
        + tick / GROUND_WATER_TICKS;
    family = WATER_FIRST_FAMILY + (uint32_t)(turn % WATER_FAMILIES);
    return family * FAMILY_PICTURES + picture % FAMILY_PICTURES;
}

size_t ground_squares(const Map *map, const uint8_t *light,
                      const uint8_t *marks, uint64_t tick,
                      const Camera *camera, uint32_t sheet_width,
                      uint32_t sheet_height, GroundSquare *out)
{
    if (camera->width <= 0 || camera->height <= 0) {
        return 0;
    }
    int32_t first_x = square_of(camera->left);
    int32_t first_y = square_of(camera->top);
    int32_t last_x = square_of(camera->left + camera->width - 1);
    int32_t last_y = square_of(camera->top + camera->height - 1)
        + ROWS_BELOW;
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
            uint32_t tile_x = picture_at((tile >> 4) & 0xffu, x, y, tick)
                * MAP_SQUARE;
            uint32_t tile_y = (tile & 0xfu) * MAP_SQUARE;
            if (tile_x + MAP_SQUARE > sheet_width
                || tile_y + MAP_SQUARE > sheet_height) {
                continue;
            }
            GroundSquare *square = &out[count++];
            square->x = (int16_t)(x * MAP_SQUARE - camera->left);
            square->mark = marks == NULL ? 0
                : marks[(size_t)y * map->info.width + (size_t)x];
            square->tile_x = (uint16_t)tile_x;
            square->tile_y = (uint16_t)tile_y;
            /* Top left, top right, low left, low right. */
            for (int32_t i = 0; i < FRAME_SQUARE_CORNERS; i++) {
                int32_t low = i >> 1;
                size_t at = corner_index(map, x + (i & 1), y + low);
                square->y[i] = (int16_t)((y + low) * MAP_SQUARE
                    - (int32_t)map->heights[at] - camera->top);
                square->light[i] = light[at];
            }
        }
    }
    return count;
}
