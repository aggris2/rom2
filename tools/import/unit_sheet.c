#include "import/unit_sheet.h"

#include <string.h>

#include "import/frames.h"

/* What a sheet is being built from. */
typedef struct Plan {
    const UnitLayout *layout;
    const Frames *frames;
    uint32_t box_width;
    uint32_t box_height;
    bool standing_row;                /* row 0 holds the 16 steps */
    bool wanted[UNIT_ANIM_COUNT];     /* which animations go on it */
    uint32_t column[UNIT_ANIM_COUNT];
    uint32_t columns[UNIT_ANIM_COUNT];
} Plan;

/* Deals with one frame of the plan. With no sheet, the window is made
 * larger so that it holds the frame. With a sheet, the frame is put into
 * its cell, at the place it has in the box. */
static bool place(const Plan *plan, uint32_t index, bool mirror,
    uint32_t column, uint32_t row, FrameWindow *window, Sheet *sheet)
{
    if (sheet == NULL) {
        return frames_measure(plan->frames, index, plan->box_width,
            plan->box_height, window);
    }
    return frames_put(plan->frames, index, mirror, plan->box_width,
        plan->box_height, window, sheet, column, row);
}

/* Goes through every frame of the plan, in the order of the sheet. */
static bool place_all(const Plan *plan, FrameWindow *window, Sheet *sheet)
{
    uint32_t first_row = plan->standing_row ? 1 : 0;
    bool ok = true;

    for (uint32_t step = 0;
        ok && plan->standing_row && step < UNIT_ROTATION_STEPS; step++) {
        uint32_t frame = 0;
        bool mirror = false;
        ok = unit_layout_rotation(plan->layout, step, &frame, &mirror)
            && place(plan, frame, mirror, step, 0, window, sheet);
    }
    for (int anim = 0; ok && anim < UNIT_ANIM_COUNT; anim++) {
        for (uint32_t d = 0; ok && d < UNIT_DIRECTIONS; d++) {
            for (uint32_t p = 0; ok && p < plan->columns[anim]; p++) {
                uint32_t frame = 0;
                bool mirror = false;
                ok = unit_layout_frame(plan->layout, (UnitAnim)anim, d, p,
                        &frame, &mirror)
                    && place(plan, frame, mirror, plan->column[anim] + p,
                        first_row + d, window, sheet);
            }
        }
    }
    return ok;
}

/* Builds a sheet with the wanted animations, and the standing row if
 * asked for. */
static bool build(const UnitLayout *layout, const Sprite256 *sprite,
    const Sprite256 *edges, uint32_t box_width, uint32_t box_height,
    int32_t center_x, int32_t center_y, const bool wanted[UNIT_ANIM_COUNT],
    bool standing_row, UnitSheet *out)
{
    if (box_width == 0 || box_width > UNIT_SHEET_MAX_BOX
        || box_height == 0 || box_height > UNIT_SHEET_MAX_BOX
        || sprite->frame_count != layout->frame_count
        || (edges != NULL && edges->frame_count != layout->frame_count)) {
        return false;
    }

    Frames frames;
    Frames edge_frames = {NULL, 0};
    if (!frames_read(sprite, &frames)) {
        return false;
    }
    if (edges != NULL && !frames_read(edges, &edge_frames)) {
        frames_free(&frames);
        return false;
    }

    /* The wanted animations side by side in the direction rows. One that
     * the unit does not have, or whose frames are in another unit's
     * file, takes no columns. */
    Plan plan;
    memset(&plan, 0, sizeof plan);
    plan.layout = layout;
    plan.frames = &frames;
    plan.box_width = box_width;
    plan.box_height = box_height;
    plan.standing_row = standing_row;
    uint32_t columns = 0;
    for (int anim = 0; anim < UNIT_ANIM_COUNT; anim++) {
        uint32_t frame = 0;
        bool mirror = false;
        bool on_sheet = wanted[anim] && unit_layout_frame(layout,
            (UnitAnim)anim, 0, 0, &frame, &mirror);
        plan.column[anim] = columns;
        plan.columns[anim] = on_sheet ? layout->phases[anim] : 0;
        columns += plan.columns[anim];
    }
    if (standing_row && columns < UNIT_ROTATION_STEPS) {
        columns = UNIT_ROTATION_STEPS;
    }
    uint32_t rows = UNIT_DIRECTIONS + (standing_row ? 1 : 0);

    /* The same plan for the edge frames, which go on a sheet of their
     * own with the same cells. */
    Plan edge_plan = plan;
    edge_plan.frames = &edge_frames;

    /* First find the window that holds every frame, of the figure and of
     * its edge, then fill sheets of that cell size. */
    FrameWindow window = frame_window_empty(box_width, box_height);
    bool ok = columns > 0 && place_all(&plan, &window, NULL)
        && (edges == NULL || place_all(&edge_plan, &window, NULL));
    frame_window_close(&window, box_width, box_height);

    UnitSheet result;
    memset(&result, 0, sizeof result);
    ok = ok && sheet_create(&result.sheet, window.right - window.left,
        window.bottom - window.top, columns, rows);
    ok = ok && place_all(&plan, &window, &result.sheet);
    if (ok && edges != NULL) {
        ok = sheet_create(&result.edges, window.right - window.left,
                window.bottom - window.top, columns, rows)
            && place_all(&edge_plan, &window, &result.edges);
    }
    frames_free(&frames);
    frames_free(&edge_frames);
    if (!ok) {
        sheet_free(&result.sheet);
        sheet_free(&result.edges);
        return false;
    }

    result.anchor_x = center_x - (int32_t)window.left;
    result.anchor_y = center_y - (int32_t)window.top;
    result.first_direction_row = standing_row ? 1 : 0;
    memcpy(result.column, plan.column, sizeof result.column);
    memcpy(result.columns, plan.columns, sizeof result.columns);
    *out = result;
    return true;
}

bool unit_sheet_build(const UnitLayout *layout, const Sprite256 *sprite,
    const Sprite256 *edges, uint32_t box_width, uint32_t box_height,
    int32_t center_x, int32_t center_y, UnitSheet *out)
{
    bool wanted[UNIT_ANIM_COUNT] = {false};
    wanted[UNIT_ANIM_MOVE] = true;
    wanted[UNIT_ANIM_ATTACK] = true;
    wanted[UNIT_ANIM_IDLE] = true;
    return build(layout, sprite, edges, box_width, box_height, center_x,
        center_y, wanted, true, out);
}

bool unit_sheet_build_dying(const UnitLayout *layout,
    const Sprite256 *sprite, const Sprite256 *edges, uint32_t box_width,
    uint32_t box_height, int32_t center_x, int32_t center_y, UnitSheet *out)
{
    bool wanted[UNIT_ANIM_COUNT] = {false};
    wanted[UNIT_ANIM_DYING] = true;
    wanted[UNIT_ANIM_BONES] = true;
    /* A layout whose dying frames are in another file has no columns to
     * give, and build() refuses a sheet with none. */
    return build(layout, sprite, edges, box_width, box_height, center_x,
        center_y, wanted, false, out);
}

void unit_sheet_free(UnitSheet *sheet)
{
    sheet_free(&sheet->sheet);
    sheet_free(&sheet->edges);
    memset(sheet, 0, sizeof *sheet);
}
