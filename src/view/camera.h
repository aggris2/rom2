/* The camera: which part of a map is on the screen.
 *
 * A map is measured in pixels here, MAP_SQUARE to a square, from its
 * top left corner.
 */
#ifndef ROM2_VIEW_CAMERA_H
#define ROM2_VIEW_CAMERA_H

#include <stdint.h>

#include "data/map.h"

typedef struct Camera {
    int32_t left;       /* the pixel of the map at the screen's corner */
    int32_t top;
    int32_t width;      /* the size of the screen */
    int32_t height;
} Camera;

/* Puts the point (`x`, `y`) of the map in the middle of the screen. */
void camera_look_at(Camera *camera, int32_t x, int32_t y);

/* Moves the camera back inside a map of `map_width` by `map_height`
 * squares, so that the edge that is never shown stays out of sight.
 * Along a side where the screen is larger than what there is to show,
 * the map is put in the middle. */
void camera_keep_inside(Camera *camera, uint32_t map_width,
                        uint32_t map_height);

#endif
