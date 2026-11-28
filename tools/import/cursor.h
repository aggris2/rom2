/* The tip of each cursor: the pixel of its picture that points, which
 * goes where the mouse is.
 *
 * No file of the original says it. The numbers are in the game's own
 * program, one pair for each cursor it knows, and are written into a
 * cursor's definition as `tip`, so that the game need not know them.
 */
#ifndef ROM2_IMPORT_CURSOR_H
#define ROM2_IMPORT_CURSOR_H

#include <stdbool.h>
#include <stdint.h>

/* Says the tip of the cursor whose files are written at `base`, a path
 * without its ending such as "content/interface/cursors/default/sprites":
 * the pixel from the left and from the top of a frame.
 *
 * Returns false, and leaves `out` alone, for a path that is not that of
 * a cursor the original has a tip for. */
bool cursor_tip(const char *base, int32_t out[2]);

#endif
