/* Writing PNG pictures.
 *
 * ROM2 keeps its sprites as 8-bit indexed PNGs: each pixel is a number
 * from 0 to 255, and the file carries a palette of 256 colours. The pixel
 * numbers are exactly the ones in the original game's files, so nothing is
 * lost and a sprite can still be recoloured by swapping its palette.
 *
 * The pixel data is stored without compression. Any PNG reader accepts
 * that; the files are simply larger than they could be.
 */
#ifndef ROM2_IMPORT_PNG_H
#define ROM2_IMPORT_PNG_H

#include <stdbool.h>
#include <stdint.h>

#include "base/buffer.h"
#include "import/rgb.h"

/* The largest width or height that png_write_indexed() accepts. */
#define PNG_MAX_SIDE 16384u

/* Adds a complete PNG file to the end of `out`.
 *
 * `pixels` holds `width * height` palette indices, row after row from the
 * top. `palette` gives the colour of each index. Index 0 is written as
 * fully transparent, whatever colour the palette gives it.
 *
 * Returns false, and leaves `out` as it was, if the width or height is 0
 * or more than PNG_MAX_SIDE, or memory runs out. */
bool png_write_indexed(ByteBuffer *out, uint32_t width, uint32_t height,
    const uint8_t *pixels, const Rgb palette[256]);

#endif
