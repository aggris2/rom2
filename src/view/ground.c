#include "view/ground.h"

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
    return across * down * FRAME_SQUARE_CORNERS;
}

int32_t ground_height_at(const Map *map, int32_t x, int32_t y)
{
    int32_t square_x = square_of(x);
    int32_t square_y = square_of(y);
    int32_t across = x - square_x * MAP_SQUARE;
    int32_t down = y - square_y * MAP_SQUARE;
    int32_t top_left = map->heights[corner_index(map, square_x, square_y)];
    int32_t top_right
        = map->heights[corner_index(map, square_x + 1, square_y)];
    int32_t low_left
        = map->heights[corner_index(map, square_x, square_y + 1)];
    int32_t low_right
        = map->heights[corner_index(map, square_x + 1, square_y + 1)];
    /* A square is drawn as two flat triangles, cut from its top right
     * corner to its low left: see ground_corners(). The height is that
     * of the triangle the point is in, so that what stands there
     * stands on the ground as it is drawn. */
    if (across + down <= MAP_SQUARE) {
        return (top_left * MAP_SQUARE + (top_right - top_left) * across
            + (low_left - top_left) * down) / MAP_SQUARE;
    }
    return (low_right * MAP_SQUARE
        + (low_left - low_right) * (MAP_SQUARE - across)
        + (top_right - low_right) * (MAP_SQUARE - down)) / MAP_SQUARE;
}

/* The corner (`x`, `y`) of the map, for a square whose tile is at
 * (`tile_x`, `tile_y`) of the sheet: where it is on the screen and how
 * bright. Which point of the tile is there is for the caller to say. */
static GroundCorner corner_of(const Map *map, const uint8_t *light,
                              const Camera *camera, int32_t x, int32_t y,
                              uint32_t tile_x, uint32_t tile_y)
{
    size_t at = corner_index(map, x, y);
    GroundCorner corner;
    corner.x = (int16_t)(x * MAP_SQUARE - camera->left);
    corner.y = (int16_t)(y * MAP_SQUARE - (int32_t)map->heights[at]
        - camera->top);
    corner.point_x = 0;
    corner.point_y = 0;
    corner.tile_x = (uint16_t)tile_x;
    corner.tile_y = (uint16_t)tile_y;
    corner.light = light[at];
    return corner;
}

size_t ground_corners(const Map *map, const uint8_t *light,
                      const Camera *camera, uint32_t sheet_width,
                      uint32_t sheet_height, GroundCorner *out)
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
            uint32_t tile_x = ((tile >> 4) & 0xffu) * MAP_SQUARE;
            uint32_t tile_y = (tile & 0xfu) * MAP_SQUARE;
            if (tile_x + MAP_SQUARE > sheet_width
                || tile_y + MAP_SQUARE > sheet_height) {
                continue;
            }
            GroundCorner square[4];
            for (int32_t i = 0; i < 4; i++) {
                int32_t right = i & 1;
                int32_t low = i >> 1;
                square[i] = corner_of(map, light, camera, x + right,
                    y + low, tile_x, tile_y);
                square[i].point_x
                    = (uint16_t)(tile_x + (uint32_t)right * MAP_SQUARE);
                square[i].point_y
                    = (uint16_t)(tile_y + (uint32_t)low * MAP_SQUARE);
            }
            /* Top left, top right, low left; then top right, low
             * right, low left. */
            GroundCorner *corners = &out[count];
            corners[0] = square[0];
            corners[1] = square[1];
            corners[2] = square[2];
            corners[3] = square[1];
            corners[4] = square[3];
            corners[5] = square[2];
            count += FRAME_SQUARE_CORNERS;
        }
    }
    return count;
}
