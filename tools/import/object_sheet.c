#include "import/object_sheet.h"

#include <string.h>

#include "import/frames.h"

/* Goes through the object's frames. With no sheet, the window is made
 * larger so that it holds each. With a sheet, each is put into its
 * cell. */
static bool place_all(const Frames *frames, uint32_t first, uint32_t count,
    uint32_t box_width, uint32_t box_height, FrameWindow *window,
    Sheet *sheet)
{
    for (uint32_t i = 0; i < count; i++) {
        bool ok = sheet == NULL
            ? frames_measure(frames, first + i, box_width, box_height,
                window)
            : frames_put(frames, first + i, false, box_width, box_height,
                window, sheet, i, 0);
        if (!ok) {
            return false;
        }
    }
    return true;
}

bool object_sheet_build(const Sprite256 *sprite, const Sprite256 *edges,
    uint32_t first, uint32_t count, uint32_t box_width,
    uint32_t box_height, int32_t center_x, int32_t center_y,
    ObjectSheet *out)
{
    if (box_width == 0 || box_width > OBJECT_SHEET_MAX_BOX
        || box_height == 0 || box_height > OBJECT_SHEET_MAX_BOX
        || count == 0 || first >= sprite->frame_count
        || count > sprite->frame_count - first
        || (edges != NULL && edges->frame_count != sprite->frame_count)) {
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

    /* First find the window that holds every frame, of the object and of
     * its edge, then fill sheets of that cell size. */
    FrameWindow window = frame_window_empty(box_width, box_height);
    bool ok = place_all(&frames, first, count, box_width, box_height,
            &window, NULL)
        && (edges == NULL || place_all(&edge_frames, first, count,
            box_width, box_height, &window, NULL));
    frame_window_close(&window, box_width, box_height);

    ObjectSheet result;
    memset(&result, 0, sizeof result);
    ok = ok && sheet_create(&result.sheet, window.right - window.left,
            window.bottom - window.top, count, 1)
        && place_all(&frames, first, count, box_width, box_height, &window,
            &result.sheet);
    if (ok && edges != NULL) {
        ok = sheet_create(&result.edges, window.right - window.left,
                window.bottom - window.top, count, 1)
            && place_all(&edge_frames, first, count, box_width, box_height,
                &window, &result.edges);
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
    *out = result;
    return true;
}

void object_sheet_free(ObjectSheet *sheet)
{
    sheet_free(&sheet->sheet);
    sheet_free(&sheet->edges);
    memset(sheet, 0, sizeof *sheet);
}
