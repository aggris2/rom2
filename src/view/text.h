/* Writing on the screen: a text as sprites, a glyph of a font for
 * each of its characters.
 */
#ifndef ROM2_VIEW_TEXT_H
#define ROM2_VIEW_TEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "data/font.h"
#include "view/sprite.h"

/* How far a glyph moves the writing on beyond its width, as the
 * original writes. A glyph's width in its font is one less than the
 * pixels it covers, so this leaves one empty pixel between two. */
#define TEXT_STEP 2

/* How a font looks: its sheet on the card, and where each glyph is in
 * it and how wide, as Font has it. One whose picture could not be
 * loaded is not `known`, and nothing is written in it. */
typedef struct FontLook {
    bool known;
    uint16_t sheet;
    uint16_t cell_width;
    uint16_t cell_height;
    uint16_t columns;
    uint16_t first_character;
    uint16_t count;
    uint8_t widths[FONT_MAX_GLYPHS];
} FontLook;

/* How wide a text is in a font, in pixels, from the first pixel of its
 * first glyph to the last of its last. A character the font has no
 * glyph for takes no room. A space moves the writing on by half the
 * height of a cell more than a glyph of no width would, as in the
 * original. */
int32_t text_width(const FontLook *font, const char *text);

/* Writes the sprites of a text with its top left corner at (`x`, `y`)
 * of the screen, and returns how many it wrote: at most `room`, and
 * one for each character that has a glyph and is not a space. A sprite
 * is the whole cell of its glyph. The text is as its sheet's colours,
 * and lies over the map and under the pointer. */
size_t text_sprites(const FontLook *font, const char *text, int32_t x,
                    int32_t y, Sprite *out, size_t room);

#endif
