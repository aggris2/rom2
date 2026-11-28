#include "view/text.h"

#include "view/frame.h"

/* The place of a character's glyph among a font's, or -1 if the font
 * has none for it. */
static int32_t glyph_of(const FontLook *font, char character)
{
    int32_t glyph = (int32_t)(unsigned char)character
        - (int32_t)font->first_character;
    return glyph >= 0 && glyph < (int32_t)font->count ? glyph : -1;
}

/* How far a character moves the writing on. */
static int32_t advance_of(const FontLook *font, char character,
                          int32_t glyph)
{
    int32_t space = character == ' ' ? font->cell_height / 2 : 0;
    return space + font->widths[glyph] + TEXT_STEP;
}

int32_t text_width(const FontLook *font, const char *text)
{
    int32_t width = 0;
    for (const char *at = text; *at != '\0'; at++) {
        int32_t glyph = glyph_of(font, *at);
        if (glyph >= 0) {
            width += advance_of(font, *at, glyph);
        }
    }
    /* A glyph covers one pixel more than its width, and the rest of
     * its step is empty. */
    return width > 0 ? width - TEXT_STEP + 1 : 0;
}

size_t text_sprites(const FontLook *font, const char *text, int32_t x,
                    int32_t y, Sprite *out, size_t room)
{
    size_t count = 0;
    if (!font->known) {
        return 0;
    }
    for (const char *at = text; *at != '\0'; at++) {
        int32_t glyph = glyph_of(font, *at);
        if (glyph < 0) {
            continue;
        }
        if (*at != ' ' && count < room) {
            Sprite *sprite = &out[count++];
            sprite->sheet = font->sheet;
            sprite->source_x = (uint16_t)(glyph % font->columns
                * font->cell_width);
            sprite->source_y = (uint16_t)(glyph / font->columns
                * font->cell_height);
            sprite->width = font->cell_width;
            sprite->height = font->cell_height;
            sprite->x = (int16_t)x;
            sprite->y = (int16_t)y;
            sprite->light = FRAME_LIGHT_WHOLE;
            sprite->palette = 0;
            sprite->order = SPRITE_ORDER_TEXT;
        }
        x += advance_of(font, *at, glyph);
    }
    return count;
}
