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

/* content/heroes: a folder for each look of the hero, with the look in
 * armour and without. */
int import_heroes(const ResArchive *graphics, const char *content_folder);

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

/* content/interface: the pictures and the sprites of the interface, the
 * menus, the shops and the cursors. `main_archive` and `movies` may be
 * NULL, and their pictures are then not written. */
int import_interface(const ResArchive *graphics,
    const ResArchive *main_archive, const ResArchive *movies,
    const char *content_folder);

/* content/fonts: a folder for each font. */
int import_fonts(const ResArchive *graphics, const char *content_folder);

/* content/campaign: a folder for each mission, with its map as it is and
 * its text as UTF-8. Either archive may be NULL, and the step then
 * fails; so it is with the texts and the videos. */
int import_campaign(const ResArchive *scenario,
    const ResArchive *main_archive, const char *content_folder);

/* content/maps: the maps that lie loose in the game folder, as they
 * are. */
int import_maps(const char *game_folder, const char *content_folder);

/* content/sounds: the sound effects, the music and the speech, as they
 * are. Any of the three archives may be NULL, and its sounds are then
 * not written. */
int import_sounds(const ResArchive *sfx, const ResArchive *music,
    const ResArchive *speech, const char *content_folder);

/* content/texts: the texts, as UTF-8, but for those of the missions. */
int import_texts(const ResArchive *main_archive, const char *content_folder);

/* content/videos: the videos, as they are, each with the registry that
 * says how it fades, as TOML. */
int import_videos(const ResArchive *video, const char *content_folder);

/* content/rules: the small registries of settings, as TOML. `scenario`
 * may be NULL, and its one is then left out. */
int import_rules(const ResArchive *world, const ResArchive *scenario,
    const char *content_folder);

#endif
