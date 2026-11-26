/* The steps of rom2_import. Each writes one kind of thing of the
 * original game into the content folder, from archives that are open.
 *
 * A step prints what it wrote, and on stderr what it had to leave out.
 * It returns how many things it left out, or -1 if it could do nothing
 * at all. `only` is the ID of the one thing to write, or -1 for all.
 * Where a step says an archive may be NULL, that is one the game folder
 * does not have; the others must be open.
 */
#ifndef ROM2_ROM2_IMPORT_STEPS_H
#define ROM2_ROM2_IMPORT_STEPS_H

#include <stdint.h>

#include "import/res.h"

/* content/units: a folder for each unit, with its kinds, sheets and
 * palettes. */
int import_units(const ResArchive *graphics, const ResArchive *world,
    const char *content_folder, int32_t only);

/* content/objects: a folder for each object. */
int import_objects(const ResArchive *graphics, const char *content_folder,
    int32_t only);

/* content/structures: a folder for each structure. */
int import_structures(const ResArchive *graphics, const ResArchive *world,
    const char *content_folder, int32_t only);

/* content/projectiles: a folder for each projectile. */
int import_projectiles(const ResArchive *graphics,
    const char *content_folder, int32_t only);

/* content/spells: a folder for each spell. */
int import_spells(const ResArchive *world, const char *content_folder,
    int32_t only);

/* content/items and content/characters: the items with their pictures,
 * and the characters they are drawn on. */
int import_items(const ResArchive *graphics, const ResArchive *world,
    const char *content_folder);

/* content/terrain: the ground tiles. */
int import_terrain(const ResArchive *graphics, const char *content_folder);

/* content/interface: the plain pictures of the interface, the menus and
 * the shops. `main_archive` and `movies` may be NULL, and their pictures
 * are then not written. */
int import_interface(const ResArchive *graphics,
    const ResArchive *main_archive, const ResArchive *movies,
    const char *content_folder);

/* content/maps: the maps, as they are. `scenario` may be NULL, and the
 * step then fails; so it is with the texts and the videos. */
int import_maps(const ResArchive *scenario, const char *content_folder);

/* content/sounds: the sound effects, the music and the speech, as they
 * are. Any of the three archives may be NULL, and its sounds are then
 * not written. */
int import_sounds(const ResArchive *sfx, const ResArchive *music,
    const ResArchive *speech, const char *content_folder);

/* content/texts: the texts, as UTF-8. */
int import_texts(const ResArchive *main_archive, const char *content_folder);

/* content/videos: the videos, as they are. */
int import_videos(const ResArchive *video, const char *content_folder);

#endif
