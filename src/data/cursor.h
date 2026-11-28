/* A cursor, the picture the mouse moves over the screen: its
 * definition, as the importer wrote it.
 *
 * A cursor's picture is a sheet of its frames with, beside it, how
 * solid each pixel is. Its tip is the pixel of a frame that points:
 * the one that goes where the mouse is.
 */
#ifndef ROM2_DATA_CURSOR_H
#define ROM2_DATA_CURSOR_H

#include <stdbool.h>
#include <stdint.h>

#include "base/toml.h"

/* The room the name of one of a cursor's files needs, with its closing
 * zero. */
#define CURSOR_NAME_SIZE 64

typedef struct Cursor {
    char sheet[CURSOR_NAME_SIZE];   /* beside the definition */
    char alpha[CURSOR_NAME_SIZE];   /* how solid each pixel is */
    uint16_t cell_width;            /* of a frame */
    uint16_t cell_height;
    uint16_t tip_x;                 /* from a frame's top left corner */
    uint16_t tip_y;
} Cursor;

/* Reads a cursor from the table of its definition's top.
 *
 * Returns false, and leaves `*out` alone, if the definition lacks its
 * sheet, its alpha, its cell or its tip, a name is empty, too long or
 * not a plain file name, a cell has no size or one over 4096, or the
 * tip is not a pixel of the cell. */
bool cursor_read(const TomlTable *top, Cursor *out);

#endif
