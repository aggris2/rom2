/* A font: its definition, as the importer wrote it.
 *
 * A font's picture is a grid of cells of one size, a glyph in the top
 * left corner of each, in the order of the characters from the first
 * one the font has. A glyph is narrower than its cell, by its own
 * width. The picture of a font of one colour says only how solid each
 * pixel is: the game gives the text its colour.
 */
#ifndef ROM2_DATA_FONT_H
#define ROM2_DATA_FONT_H

#include <stdbool.h>
#include <stdint.h>

#include "base/toml.h"

/* The room the name of a font's picture needs, with its closing
 * zero. */
#define FONT_NAME_SIZE 64

/* The most glyphs a font may have: one for each character a byte can
 * be. */
#define FONT_MAX_GLYPHS 256

typedef struct Font {
    char glyphs[FONT_NAME_SIZE];        /* beside the definition */
    bool coloured;                      /* its picture has colours */
    uint16_t cell_width;
    uint16_t cell_height;
    uint16_t columns;                   /* cells in a row */
    uint16_t first_character;           /* the one its first glyph is */
    uint16_t count;                     /* how many glyphs */
    uint8_t widths[FONT_MAX_GLYPHS];    /* of each glyph, in pixels */
} Font;

/* Reads a font from the table of its definition's top.
 *
 * Returns false, and leaves `*out` alone, if the definition lacks a
 * part, the name of the picture is empty, too long or not a plain file
 * name, a cell has no size or one over 255, the font has no columns,
 * no glyphs or more than FONT_MAX_GLYPHS, its characters do not all
 * fit in a byte, there is not a width for each glyph, or a glyph is
 * wider than a cell. */
bool font_read(const TomlTable *top, Font *out);

#endif
