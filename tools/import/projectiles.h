/* Reading projectiles from the original game's projectiles.reg.
 *
 * A projectile is what flies from an attacker to its target, and what
 * shows where a spell lands: an arrow, a fire bolt, a healing sparkle.
 * The registry has them as things, in the sections "Projectile0",
 * "Projectile1" and so on.
 *
 * A projectile has "Phases" pictures of an animation and may turn to
 * face where it flies. Its sprite file holds the pictures of the first
 * direction, then those of the next, and so on. An arrow has one picture
 * in each of 16 directions. A fire bolt has 4 in each of 9, and is
 * mirrored for the other 7, as a unit is. A sparkle has no direction.
 *
 * See docs/formats/projectiles.md.
 */
#ifndef ROM2_IMPORT_PROJECTILES_H
#define ROM2_IMPORT_PROJECTILES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "import/reg.h"
#include "import/things.h"

/* The directions a projectile that turns is shown in. */
#define PROJECTILE_STEPS 16u
/* The most pictures an animation may have. */
#define PROJECTILE_MAX_PHASES 256u

/* How the frames of a projectile's sprite file are used. */
typedef struct ProjectileLayout {
    /* The pictures of the animation, in each direction. */
    uint32_t phases;
    /* The directions the file has: 1 for a projectile that does not
     * turn, 16, or 9 for one that is mirrored for the rest. */
    uint32_t stored_steps;
    /* The directions the projectile is shown in: 1 or 16. */
    uint32_t steps;
    /* The box it is drawn in. A frame is the middle of it. */
    uint32_t width;
    uint32_t height;
    /* The frames of the file that the numbers do not account for. */
    uint32_t spare_frames;
} ProjectileLayout;

/* The projectiles of a registry, to be read with the functions of
 * things.h. */
Things projectiles_things(const Reg *reg);

/* Builds the archive path of a projectile's sprite file: its "File" text
 * in lower case, with '\' turned into '/', "projectiles/" in front, and
 * ".16a" behind if the projectile says "A16", ".256" otherwise.
 * `*out_is_16a` says which.
 *
 * Returns false, and leaves the out-values alone, if the projectile has
 * no "File" or the path does not fit `size`. */
bool projectiles_sprite_path(const Reg *reg, int32_t id, char *out,
    size_t size, bool *out_is_16a);

/* Works out the layout of a projectile whose sprite file has
 * `frame_count` frames.
 *
 * The directions are "RotationPhases" if the projectile states it. If
 * not, they are 9 for one that says "Flip" and 16 for one that does not.
 * The box is "Width" by "Height", and 64 by 64 for a projectile that
 * does not state them.
 *
 * Returns false, and leaves `*out` alone, if no projectile has that ID,
 * its numbers are missing or not usable, or the file has fewer frames
 * than they need. */
bool projectiles_layout(const Reg *reg, int32_t id, uint32_t frame_count,
    ProjectileLayout *out);

/* Says which frame of the file shows the projectile in direction `step`
 * at picture `phase`, and whether it has to be mirrored left to right.
 *
 * Returns false if the layout has no such direction or picture. */
bool projectiles_frame(const ProjectileLayout *layout, uint32_t step,
    uint32_t phase, uint32_t *out_frame, bool *out_mirror);

#endif
