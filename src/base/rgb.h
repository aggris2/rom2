/* A colour, as the importer passes it between a palette that was read and
 * a picture that is written. */
#ifndef ROM2_BASE_RGB_H
#define ROM2_BASE_RGB_H

#include <stdint.h>

typedef struct Rgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Rgb;

#endif
