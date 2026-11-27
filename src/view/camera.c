#include "view/camera.h"

void camera_look_at(Camera *camera, int32_t x, int32_t y)
{
    camera->left = x - camera->width / 2;
    camera->top = y - camera->height / 2;
}

/* One side: where the screen starts, given its size and the squares of
 * the map along it. */
static int32_t keep_inside(int32_t at, int32_t size, uint32_t squares)
{
    int32_t low = MAP_EDGE * MAP_SQUARE;
    int32_t high = ((int32_t)squares - MAP_EDGE) * MAP_SQUARE - size;
    if (high < low) {
        /* The same space on both sides: the middle of what is shown
         * in the middle of the screen. */
        return (low + high) / 2;
    }
    return at < low ? low : at > high ? high : at;
}

void camera_keep_inside(Camera *camera, uint32_t map_width,
                        uint32_t map_height)
{
    camera->left = keep_inside(camera->left, camera->width, map_width);
    camera->top = keep_inside(camera->top, camera->height, map_height);
}
