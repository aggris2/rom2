#include "import/object.h"

#include <stdio.h>
#include <string.h>

#include "base/toml.h"
#include "import/frames.h"
#include "import/things.h"

Things objects_things(const Reg *reg)
{
    Things t = {reg, "ObjectCount", "Object"};
    return t;
}

bool objects_sprite_path(const Reg *reg, int32_t id, char *out,
    size_t size)
{
    Things t = objects_things(reg);
    int32_t file = 0;
    char key[32];
    char path[256];
    const char *text = NULL;
    if (!things_int(&t, id, "File", &file)) {
        return false;
    }
    snprintf(key, sizeof key, "File%d", (int)file);
    if (!reg_text(reg, "Files", key, &text)) {
        return false;
    }
    int written = snprintf(path, sizeof path, "objects/%s.256", text);
    if (written < 0 || (size_t)written >= sizeof path
        || (size_t)written >= size) {
        return false;
    }
    for (char *p = path; *p != '\0'; p++) {
        if (*p == '\\') {
            *p = '/';
        }
    }
    memcpy(out, path, (size_t)written + 1);
    return true;
}

bool objects_frame_range(const Reg *reg, int32_t id, uint32_t file_frames,
    uint32_t *out_first, uint32_t *out_count)
{
    Things t = objects_things(reg);
    char section[THINGS_SECTION_SIZE];
    int32_t first = 0;
    int32_t count = 1;
    if (!things_section(&t, id, section)) {
        return false;
    }
    things_int(&t, id, "Index", &first);
    things_int(&t, id, "Phases", &count);
    if (first < 0 || count < 0 || (uint32_t)first >= file_frames) {
        return false;
    }
    uint32_t left = file_frames - (uint32_t)first;
    *out_first = (uint32_t)first;
    *out_count = (uint32_t)count < left ? (uint32_t)count : left;
    return true;
}

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

bool object_toml_folder_name(const Reg *reg, int32_t id,
    char out[NAME_FOLDER_SIZE])
{
    Things t = objects_things(reg);
    return thing_toml_folder_name(&t, id, "DescText", out);
}

bool object_toml_write(ByteBuffer *b, const Reg *reg, int32_t id,
    const ObjectSheet *sheet)
{
    Things t = objects_things(reg);
    char section[THINGS_SECTION_SIZE];
    int32_t frames[OBJECT_TOML_MAX_STEPS];
    int32_t ticks[OBJECT_TOML_MAX_STEPS];
    size_t frame_count = 0;
    size_t tick_count = 0;
    const char *name = "";
    if (!things_section(&t, id, section)
        || !thing_toml_list(&t, id, "AnimationFrame", frames,
            OBJECT_TOML_MAX_STEPS, &frame_count)
        || !thing_toml_list(&t, id, "AnimationTime", ticks,
            OBJECT_TOML_MAX_STEPS, &tick_count)
        || frame_count != tick_count) {
        return false;
    }
    for (size_t i = 0; i < frame_count; i++) {
        if (frames[i] < 0 || (uint32_t)frames[i] >= sheet->sheet.columns) {
            return false;
        }
    }
    things_text(&t, id, "DescText", &name);

    /* An object that is never destroyed says -1, which is left out. */
    int32_t dead = -1;
    things_int(&t, id, "DeadObject", &dead);

    const int32_t cell[2] = {
        (int32_t)sheet->sheet.cell_width, (int32_t)sheet->sheet.cell_height
    };
    const int32_t anchor[2] = {sheet->anchor_x, sheet->anchor_y};

    return toml_comment(b, name)
        && toml_int(b, "id", id)
        && toml_text(b, "name", name)
        && thing_toml_int(b, &t, id, "InMapEditor", "in_map_editor")
        && thing_toml_int(b, &t, id, "IconID", "icon_id")
        && (dead < 0 || toml_int(b, "dead_object", dead))
        && thing_toml_int(b, &t, id, "FireObject", "fire_object")
        && toml_table(b, "sprite")
        && toml_text(b, "sheet", "sprites.png")
        && (sheet->edges.pixels == NULL
            || toml_text(b, "edges", "edges.png"))
        && toml_int_list(b, "cell", cell, 2)
        && toml_int_list(b, "anchor", anchor, 2)
        && toml_int(b, "columns", (int32_t)sheet->sheet.columns)
        && (frame_count == 0
            || (toml_int_list(b, "frames", frames, frame_count)
                && toml_int_list(b, "ticks", ticks, tick_count)));
}
