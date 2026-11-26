/* Reading objects from the original game's objects.reg.
 *
 * An object is a piece of scenery: a tree, a bush, a rock. The registry
 * has them as things, in the sections "Object0", "Object1" and so on,
 * with "ID" and "Parent" as things.h describes.
 *
 * An object's frames are in a sprite file, which several objects may
 * share: "Index" is the first frame that is this object's, and "Phases"
 * is how many it has. See docs/formats/objects.md.
 */
#ifndef ROM2_IMPORT_OBJECTS_H
#define ROM2_IMPORT_OBJECTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "import/reg.h"
#include "import/things.h"

/* The objects of a registry, to be read with the functions of
 * things.h. */
Things objects_things(const Reg *reg);

/* Builds the archive path of an object's sprite file: the text "FileN"
 * of the section "Files", where N is the object's "File", with '\'
 * turned into '/', "objects/" in front and ".256" behind.
 *
 * Returns false, and leaves `out` alone, if the object has no "File",
 * the registry has no such text, or the path does not fit `size`. */
bool objects_sprite_path(const Reg *reg, int32_t id, char *out,
    size_t size);

/* Says which frames of its sprite file are an object's: `*out_first` is
 * the first and `*out_count` how many. `file_frames` is the number of
 * frames the file has.
 *
 * An object without "Index" starts at frame 0, and one without "Phases"
 * has one frame. An object that states more frames than the file has
 * left gets those that are there: the dead trees of the original state 7
 * and have 1.
 *
 * Returns false, and leaves the out-values alone, if no object has that
 * ID, a number is negative, or the first frame is not in the file. */
bool objects_frame_range(const Reg *reg, int32_t id, uint32_t file_frames,
    uint32_t *out_first, uint32_t *out_count);

#endif
