/* Reading sheets from the content and putting them on the card.
 */
#ifndef ROM2_GAME_SHEETS_H
#define ROM2_GAME_SHEETS_H

#include <stdbool.h>
#include <stdint.h>

#include "data/sprite_sheet.h"
#include "game/renderer.h"

/* Reads a picture with a palette that is solid all over, such as the
 * ground's tiles, and puts it on the card. `*sheet` gets its number,
 * and `*width` and `*height` its size. Returns false, with a message
 * printed, if it cannot. */
bool sheets_load_picture(Renderer *renderer, const char *path,
                         uint16_t *sheet, uint32_t *width,
                         uint32_t *height);

/* Reads the sprite sheet a definition names: the definition, the
 * picture of the figure beside it and, if it has one, the picture of
 * its soft edges. Joins the two pictures and puts the sheet on the
 * card. `*layout` gets how the sheet is cut and `*sheet` its number.
 * Returns false, with a message printed, if it cannot. */
bool sheets_load_sprites(Renderer *renderer, const char *definition,
                         SpriteSheet *layout, uint16_t *sheet);

#endif
