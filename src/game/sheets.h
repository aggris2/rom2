/* Reading sheets from the content and putting them on the card.
 */
#ifndef ROM2_GAME_SHEETS_H
#define ROM2_GAME_SHEETS_H

#include <stdbool.h>
#include <stdint.h>

#include "base/toml.h"
#include "data/cursor.h"
#include "data/font.h"
#include "data/sprite_sheet.h"
#include "game/renderer.h"

/* Reads a picture with a palette that is solid all over, such as the
 * ground's tiles, and puts it on the card. `*sheet` gets its number,
 * and `*width` and `*height` its size. Returns false, with a message
 * printed, if it cannot. */
bool sheets_load_picture(Renderer *renderer, const char *path,
                         uint16_t *sheet, uint32_t *width,
                         uint32_t *height);

/* Reads a definition file. `*top` gets the table of its top, for the
 * caller to free with toml_free(). Returns false, with a message
 * printed, if it cannot. */
bool sheets_read_definition(const char *path, TomlTable **top);

/* Reads the sprite sheet of the definition at `definition`, given how
 * the definition says it is cut: the picture of the figure beside the
 * definition and, if it has them, the pictures of its soft edges and
 * of its palettes. Joins the first two and puts the sheet on the card.
 * `*sheet` gets its number. Returns false, with a message printed, if
 * it cannot. */
bool sheets_load_sprites(Renderer *renderer, const char *definition,
                         const SpriteSheet *layout, uint16_t *sheet);

/* Reads the pictures of the cursor whose definition is at
 * `definition`, given what the definition says: its sheet and, beside
 * it, how solid each pixel is. Puts the two on the card as one sheet.
 * `*sheet` gets its number. Returns false, with a message printed, if
 * it cannot. */
bool sheets_load_cursor(Renderer *renderer, const char *definition,
                        const Cursor *cursor, uint16_t *sheet);

/* Reads the picture of the font whose definition is at `definition`,
 * given what the definition says, and puts it on the card as a sheet
 * whose glyphs are of `colour`. A font of one colour only: one whose
 * picture has colours of its own is refused. `*sheet` gets its number.
 * Returns false, with a message printed, if it cannot. */
bool sheets_load_font(Renderer *renderer, const char *definition,
                      const Font *font, Rgb colour, uint16_t *sheet);

#endif
