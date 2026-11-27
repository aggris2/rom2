/* Which frame of a unit's sprite file shows what.
 *
 * A unit's sprite file is one long row of frames. The numbers in
 * units.reg say how to cut it up:
 *
 *     the rotation frames: the unit standing, turned in 16 steps
 *     moving:    MoveBeginPhases + MovePhases frames for each direction
 *     attacking: AttackPhases frames for each direction
 *     dying:     DyingPhases frames for each direction
 *     bones:     BonePhases frames for each direction
 *     idle:      IdlePhases frames for each direction
 *
 * Within a block, all frames of direction 0 come first, then all of
 * direction 1, and so on.
 *
 * A unit with "Flip" stores only half the circle: 9 of the 16 rotation
 * steps and 5 of the 8 directions. The other half is the mirror image of
 * the stored half.
 *
 * The dying and bones frames are in the file only if the unit's "Dying"
 * is its own ID. Otherwise they are in the file of the unit that "Dying"
 * names.
 */
#ifndef ROM2_IMPORT_UNIT_LAYOUT_H
#define ROM2_IMPORT_UNIT_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>

#include "import/reg.h"

#define UNIT_ROTATION_STEPS 16u
#define UNIT_DIRECTIONS 8u

/* The most frames one animation may have for one direction. The original
 * game's longest is 20. */
#define UNIT_MAX_PHASES 64u

typedef enum UnitAnim {
    UNIT_ANIM_MOVE,    /* the begin frames first, then the walk cycle */
    UNIT_ANIM_ATTACK,
    UNIT_ANIM_DYING,
    UNIT_ANIM_BONES,
    UNIT_ANIM_IDLE,
    UNIT_ANIM_COUNT
} UnitAnim;

typedef struct UnitLayout {
    bool flip;                          /* half the circle is mirrored */
    uint32_t move_begin;                /* how many move frames are "begin" */
    uint32_t phases[UNIT_ANIM_COUNT];   /* frames for one direction */
    uint32_t first[UNIT_ANIM_COUNT];    /* where each block starts */
    int32_t dying_id;                   /* the unit that has the dying frames */
    bool own_dying;                     /* they are in this unit's own file */
    uint32_t frame_count;               /* frames the file must have */
} UnitLayout;

/* Works out the layout of the unit with the given ID from units.reg.
 *
 * Returns false, and leaves `*out` alone, if no unit has that ID, or one
 * of its phase counts is negative or more than UNIT_MAX_PHASES. */
bool unit_layout_read(const Reg *r, int32_t id, UnitLayout *out);

/* Says which frame shows the unit standing, turned to `step` of the
 * UNIT_ROTATION_STEPS steps, and whether that frame has to be mirrored.
 *
 * Returns false, and leaves the out-parameters alone, if `step` is out of
 * range. */
bool unit_layout_rotation(const UnitLayout *layout, uint32_t step,
    uint32_t *out_frame, bool *out_mirror);

/* Says which frame shows `phase` of an animation for `direction`, one of
 * UNIT_DIRECTIONS, and whether that frame has to be mirrored.
 *
 * Returns false, and leaves the out-parameters alone, if the direction or
 * the phase is out of range, or the frames are not in this unit's file,
 * which is so for dying and bones when `own_dying` is false. */
bool unit_layout_frame(const UnitLayout *layout, UnitAnim anim,
    uint32_t direction, uint32_t phase, uint32_t *out_frame,
    bool *out_mirror);

/* Makes a layout fit a sprite file that has more frames than the
 * registry describes, where the frames over are dying frames: the unit
 * keeps its dying frames in its own file, and the file is longer by a
 * whole number of dying phases, one frame for each stored direction. The
 * dying then gets those phases more, and the bones and what follows
 * start that much later.
 *
 * The original has one such unit, the WarSheep: its registry gives its
 * dying 4 phases and its file has 5.
 *
 * Returns true, with the layout changed, if that makes it fit
 * `file_frames`. Returns false, and leaves the layout alone, if it fits
 * already, the file is shorter, the unit's dying frames are elsewhere,
 * or the frames over are not a whole number of phases. */
bool unit_layout_fit_dying(UnitLayout *layout, uint32_t file_frames);

#endif
