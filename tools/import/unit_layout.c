#include "import/unit_layout.h"

#include "import/units.h"

/* A unit that is not flipped stores every step and direction. A flipped
 * one stores the first half of the circle and both of its ends. */
#define STORED_STEPS_FLIPPED 9u
#define STORED_DIRECTIONS_FLIPPED 5u

static uint32_t stored_steps(const UnitLayout *layout)
{
    return layout->flip ? STORED_STEPS_FLIPPED : UNIT_ROTATION_STEPS;
}

static uint32_t stored_directions(const UnitLayout *layout)
{
    return layout->flip ? STORED_DIRECTIONS_FLIPPED : UNIT_DIRECTIONS;
}

/* Reads a phase count. A count the unit and its parents do not state is
 * 0. Returns false if the count is negative or too large. */
static bool read_phases(const Reg *r, int32_t id, const char *key,
    uint32_t *out)
{
    int32_t value = 0;
    if (!units_int(r, id, key, &value)) {
        *out = 0;
        return true;
    }
    if (value < 0 || (uint32_t)value > UNIT_MAX_PHASES) {
        return false;
    }
    *out = (uint32_t)value;
    return true;
}

bool unit_layout_read(const Reg *r, int32_t id, UnitLayout *out)
{
    char section[UNITS_SECTION_SIZE];
    if (!units_section(r, id, section)) {
        return false;
    }

    UnitLayout layout = {0};
    int32_t flip = 0;
    if (units_int(r, id, "Flip", &flip)) {
        layout.flip = flip != 0;
    }

    uint32_t move = 0;
    if (!read_phases(r, id, "MoveBeginPhases", &layout.move_begin)
        || !read_phases(r, id, "MovePhases", &move)
        || !read_phases(r, id, "AttackPhases",
            &layout.phases[UNIT_ANIM_ATTACK])
        || !read_phases(r, id, "DyingPhases",
            &layout.phases[UNIT_ANIM_DYING])
        || !read_phases(r, id, "BonePhases",
            &layout.phases[UNIT_ANIM_BONES])
        || !read_phases(r, id, "IdlePhases",
            &layout.phases[UNIT_ANIM_IDLE])) {
        return false;
    }
    layout.phases[UNIT_ANIM_MOVE] = layout.move_begin + move;

    /* A unit that does not say whose dying frames it uses has its own. */
    layout.dying_id = id;
    units_int(r, id, "Dying", &layout.dying_id);
    layout.own_dying = layout.dying_id == id;

    /* The blocks follow each other in the order of the enum, after the
     * rotation frames. Dying and bones take no room when the frames are
     * in another unit's file. */
    uint32_t next = stored_steps(&layout);
    for (int anim = 0; anim < UNIT_ANIM_COUNT; anim++) {
        bool in_file = layout.own_dying
            || (anim != UNIT_ANIM_DYING && anim != UNIT_ANIM_BONES);
        layout.first[anim] = next;
        if (in_file) {
            next += layout.phases[anim] * stored_directions(&layout);
        }
    }
    layout.frame_count = next;

    *out = layout;
    return true;
}

bool unit_layout_rotation(const UnitLayout *layout, uint32_t step,
    uint32_t *out_frame, bool *out_mirror)
{
    if (step >= UNIT_ROTATION_STEPS) {
        return false;
    }
    /* A step past the last stored one is the mirror image of the step
     * just as far from the start of the circle on the other side. */
    bool mirror = step >= stored_steps(layout);
    *out_frame = mirror ? UNIT_ROTATION_STEPS - step : step;
    *out_mirror = mirror;
    return true;
}

bool unit_layout_frame(const UnitLayout *layout, UnitAnim anim,
    uint32_t direction, uint32_t phase, uint32_t *out_frame,
    bool *out_mirror)
{
    if ((int)anim < 0 || anim >= UNIT_ANIM_COUNT
        || direction >= UNIT_DIRECTIONS
        || phase >= layout->phases[anim]) {
        return false;
    }
    if (!layout->own_dying
        && (anim == UNIT_ANIM_DYING || anim == UNIT_ANIM_BONES)) {
        return false;
    }
    bool mirror = direction >= stored_directions(layout);
    uint32_t stored = mirror ? UNIT_DIRECTIONS - direction : direction;
    *out_frame = layout->first[anim] + stored * layout->phases[anim] + phase;
    *out_mirror = mirror;
    return true;
}
