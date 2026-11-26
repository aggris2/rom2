/* Reading the original game's .pal files: palettes kept on their own.
 *
 * A sprite file has one palette inside it. A unit's other colourings are
 * in .pal files next to it. There are two kinds:
 *
 * - A BMP picture with 256 colours. The palette is the picture's colour
 *   table, and the pixels are not used. Monsters have one for each
 *   colouring: palette.pal, palette2.pal and so on.
 * - A table: palettes one after the other, 1024 bytes each, with no
 *   header. Humans share one, human.pal, with a palette for each of the
 *   16 player colours.
 *
 * In both a colour is 4 bytes: blue, green, red, and one that is not
 * used. See docs/formats/units.md.
 */
#ifndef ROM2_IMPORT_PAL_H
#define ROM2_IMPORT_PAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "import/rgb.h"

/* Reads the palette of a .pal file that is a BMP picture.
 *
 * Returns false, and leaves `out` alone, if the bytes are not a BMP with
 * 8 bits for each pixel, or its 256 colours are not all there. */
bool pal_bmp_read(const uint8_t *data, size_t size, Rgb out[256]);

/* The number of palettes in a .pal file that is a table: 16 for
 * human.pal. It is 0 if the size is not a whole number of palettes. */
uint32_t pal_table_count(size_t size);

/* Reads palette number `index`, counted from 0, of a table.
 *
 * Returns false, and leaves `out` alone, if the table has no palette with
 * that number. */
bool pal_table_read(const uint8_t *data, size_t size, uint32_t index,
    Rgb out[256]);

#endif
