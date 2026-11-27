#include "import/projectile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/toml.h"
#include "import/frames.h"
#include "import/sprite16a.h"
#include "import/things.h"

/* A projectile that is mirrored has the directions from straight ahead
 * to straight back: half the circle and both its ends. */
#define MIRRORED_STEPS (PROJECTILE_STEPS / 2 + 1)
#define DEFAULT_BOX 64

Things projectiles_things(const Reg *reg)
{
    Things t = {reg, "Count", "Projectile"};
    return t;
}

bool projectiles_sprite_path(const Reg *reg, int32_t id, char *out,
    size_t size, bool *out_is_16a)
{
    Things t = projectiles_things(reg);
    char path[256];
    const char *text = NULL;
    int32_t a16 = 0;
    if (!things_text(&t, id, "File", &text)) {
        return false;
    }
    things_int(&t, id, "A16", &a16);
    int written = snprintf(path, sizeof path, "projectiles/%s.%s", text,
        a16 != 0 ? "16a" : "256");
    if (written < 0 || (size_t)written >= sizeof path
        || (size_t)written >= size) {
        return false;
    }
    /* The registry writes some folders with a capital. The archive has
     * every name in lower case. */
    for (char *p = path; *p != '\0'; p++) {
        if (*p == '\\') {
            *p = '/';
        } else if (*p >= 'A' && *p <= 'Z') {
            *p = (char)(*p - 'A' + 'a');
        }
    }
    memcpy(out, path, (size_t)written + 1);
    *out_is_16a = a16 != 0;
    return true;
}

bool projectiles_layout(const Reg *reg, int32_t id, uint32_t frame_count,
    ProjectileLayout *out)
{
    Things t = projectiles_things(reg);
    char section[THINGS_SECTION_SIZE];
    int32_t phases = 0;
    int32_t flip = 0;
    int32_t width = DEFAULT_BOX;
    int32_t height = DEFAULT_BOX;
    if (!things_section(&t, id, section)
        || !things_int(&t, id, "Phases", &phases)
        || phases <= 0 || (uint32_t)phases > PROJECTILE_MAX_PHASES) {
        return false;
    }
    things_int(&t, id, "Flip", &flip);
    things_int(&t, id, "Width", &width);
    things_int(&t, id, "Height", &height);

    int32_t stored = flip != 0 ? (int32_t)MIRRORED_STEPS
        : (int32_t)PROJECTILE_STEPS;
    things_int(&t, id, "RotationPhases", &stored);
    if (width <= 0 || height <= 0
        || (stored != 1 && stored != (int32_t)MIRRORED_STEPS
            && stored != (int32_t)PROJECTILE_STEPS)) {
        return false;
    }
    uint32_t needed = (uint32_t)phases * (uint32_t)stored;
    if (frame_count < needed) {
        return false;
    }

    ProjectileLayout layout;
    layout.phases = (uint32_t)phases;
    layout.stored_steps = (uint32_t)stored;
    layout.steps = stored == 1 ? 1 : PROJECTILE_STEPS;
    layout.width = (uint32_t)width;
    layout.height = (uint32_t)height;
    layout.spare_frames = frame_count - needed;
    *out = layout;
    return true;
}

bool projectiles_frame(const ProjectileLayout *layout, uint32_t step,
    uint32_t phase, uint32_t *out_frame, bool *out_mirror)
{
    if (step >= layout->steps || phase >= layout->phases) {
        return false;
    }
    /* A direction past the last stored one is the mirror image of the
     * one just as far from the start of the circle on the other side. */
    bool mirror = step >= layout->stored_steps;
    uint32_t stored = mirror ? PROJECTILE_STEPS - step : step;
    *out_frame = stored * layout->phases + phase;
    *out_mirror = mirror;
    return true;
}

uint32_t projectile_sheet_width(const ProjectileSheet *sheet)
{
    return sheet->cell_width * sheet->columns;
}

uint32_t projectile_sheet_height(const ProjectileSheet *sheet)
{
    return sheet->cell_height * sheet->rows;
}

void projectile_sheet_free(ProjectileSheet *sheet)
{
    free(sheet->pixels);
    free(sheet->solid);
    memset(sheet, 0, sizeof *sheet);
}

/* Makes an empty sheet for a layout: every pixel not there. */
static bool empty_sheet(const ProjectileLayout *layout,
    ProjectileSheet *out)
{
    if (layout->width == 0 || layout->width > PROJECTILE_SHEET_MAX_BOX
        || layout->height == 0 || layout->height > PROJECTILE_SHEET_MAX_BOX
        || layout->phases == 0 || layout->phases > PROJECTILE_MAX_PHASES
        || layout->steps == 0 || layout->steps > PROJECTILE_STEPS) {
        return false;
    }
    ProjectileSheet sheet;
    sheet.cell_width = layout->width;
    sheet.cell_height = layout->height;
    sheet.columns = layout->phases;
    sheet.rows = layout->steps;
    size_t total = (size_t)projectile_sheet_width(&sheet)
        * projectile_sheet_height(&sheet);
    sheet.pixels = calloc(total, 1);
    sheet.solid = calloc(total, 1);
    if (sheet.pixels == NULL || sheet.solid == NULL) {
        free(sheet.pixels);
        free(sheet.solid);
        return false;
    }
    *out = sheet;
    return true;
}

/* One frame to put on the sheet: its size, the colour of each pixel as
 * an index, and how solid each pixel is, 0 to 255. */
typedef struct Piece {
    uint32_t width;
    uint32_t height;
    const uint8_t *colour;
    const uint8_t *solid;       /* NULL: use `solid_all` where shown */
    uint8_t solid_all;
    bool zero_is_empty;         /* index 0 is no pixel at all */
} Piece;

/* Puts a piece into the middle of the cell at `column`, `row`. A pixel
 * that is already on the sheet is left as it is, so what is put first
 * lies on top. */
static bool put_piece(ProjectileSheet *sheet, uint32_t column, uint32_t row,
    const Piece *piece, bool mirror)
{
    if (piece->width > sheet->cell_width
        || piece->height > sheet->cell_height) {
        return false;
    }
    uint32_t left = (sheet->cell_width - piece->width) / 2;
    uint32_t top = (sheet->cell_height - piece->height) / 2;
    if (mirror) {
        left = sheet->cell_width - left - piece->width;
    }
    size_t stride = projectile_sheet_width(sheet);
    for (uint32_t y = 0; y < piece->height; y++) {
        size_t line = ((size_t)row * sheet->cell_height + top + y) * stride
            + (size_t)column * sheet->cell_width + left;
        for (uint32_t x = 0; x < piece->width; x++) {
            uint32_t from = mirror ? piece->width - 1 - x : x;
            size_t at = (size_t)y * piece->width + from;
            uint8_t index = piece->colour[at];
            uint8_t solid = piece->solid != NULL ? piece->solid[at]
                : piece->solid_all;
            if ((piece->zero_is_empty && index == 0) || solid == 0
                || sheet->solid[line + x] != 0) {
                continue;
            }
            sheet->pixels[line + x] = index;
            sheet->solid[line + x] = solid;
        }
    }
    return true;
}

bool projectile_sheet_build_16a(const ProjectileLayout *layout,
    const Sprite256 *sprite, ProjectileSheet *out)
{
    uint32_t needed = layout->phases * layout->stored_steps;
    ProjectileSheet sheet;
    if (sprite->frame_count < needed || !empty_sheet(layout, &sheet)) {
        return false;
    }

    /* The frames come in the order of the file, and each goes to every
     * cell that shows it: its own direction, and the mirrored one. */
    Reader r = sprite256_frames(sprite);
    bool ok = true;
    for (uint32_t i = 0; ok && i < needed; i++) {
        Frame16a frame;
        if (!sprite16a_frame_read(&r, &frame)) {
            ok = false;
            break;
        }
        /* The file's 15 levels of solidness, spread over 17 to 255. */
        size_t total = (size_t)frame.width * frame.height;
        for (size_t p = 0; p < total; p++) {
            frame.alpha[p] = (uint8_t)(frame.alpha[p] * 17);
        }
        Piece piece = {frame.width, frame.height, frame.pixels, frame.alpha,
            0, false};
        for (uint32_t step = 0; ok && step < layout->steps; step++) {
            uint32_t index = 0;
            bool mirror = false;
            ok = projectiles_frame(layout, step, i % layout->phases, &index,
                &mirror);
            if (ok && index == i) {
                ok = put_piece(&sheet, i % layout->phases, step, &piece,
                    mirror);
            }
        }
        frame16a_free(&frame);
    }
    if (!ok) {
        projectile_sheet_free(&sheet);
        return false;
    }
    *out = sheet;
    return true;
}

/* Puts frame `index` of `frames` into its cell. */
static bool put_frame(ProjectileSheet *sheet, const Frames *frames,
    uint32_t index, uint32_t column, uint32_t row, bool mirror,
    uint8_t solid)
{
    if (index >= frames->count) {
        return false;
    }
    const Frame256 *frame = &frames->items[index];
    Piece piece = {frame->width, frame->height, frame->pixels, NULL, solid,
        true};
    return put_piece(sheet, column, row, &piece, mirror);
}

bool projectile_sheet_build_256(const ProjectileLayout *layout,
    const Sprite256 *sprite, const Sprite256 *edges, ProjectileSheet *out)
{
    uint32_t needed = layout->phases * layout->stored_steps;
    ProjectileSheet sheet;
    Frames frames;
    Frames edge_frames = {NULL, 0};
    if (sprite->frame_count < needed
        || (edges != NULL && edges->frame_count != sprite->frame_count)
        || !frames_read(sprite, &frames)) {
        return false;
    }
    if (edges != NULL && !frames_read(edges, &edge_frames)) {
        frames_free(&frames);
        return false;
    }
    bool ok = empty_sheet(layout, &sheet);
    if (!ok) {
        sheet.pixels = NULL;
        sheet.solid = NULL;
    }
    for (uint32_t step = 0; ok && step < layout->steps; step++) {
        for (uint32_t phase = 0; ok && phase < layout->phases; phase++) {
            uint32_t index = 0;
            bool mirror = false;
            /* The figure first, then its edge where the figure is not. */
            ok = projectiles_frame(layout, step, phase, &index, &mirror)
                && put_frame(&sheet, &frames, index, phase, step, mirror,
                    255)
                && (edges == NULL || put_frame(&sheet, &edge_frames, index,
                    phase, step, mirror, PROJECTILE_SHEET_EDGE));
        }
    }
    frames_free(&frames);
    frames_free(&edge_frames);
    if (!ok) {
        free(sheet.pixels);
        free(sheet.solid);
        return false;
    }
    *out = sheet;
    return true;
}

bool projectile_toml_folder_name(const Reg *reg, int32_t id,
    char out[NAME_FOLDER_SIZE])
{
    Things t = projectiles_things(reg);
    return thing_toml_folder_name(&t, id, "File", out);
}

bool projectile_toml_write(ByteBuffer *b, const Reg *reg, int32_t id,
    const ProjectileSheet *sheet)
{
    Things t = projectiles_things(reg);
    char section[THINGS_SECTION_SIZE];
    char name[128];
    const char *text = "";
    if (!things_section(&t, id, section)) {
        return false;
    }
    things_text(&t, id, "File", &text);
    snprintf(name, sizeof name, "%s", text);
    for (char *p = name; *p != '\0'; p++) {
        if (*p == '\\') {
            *p = '/';
        }
    }

    const int32_t cell[2] = {
        (int32_t)sheet->cell_width, (int32_t)sheet->cell_height
    };
    const int32_t anchor[2] = {cell[0] / 2, cell[1] / 2};

    return toml_comment(b, name)
        && toml_int(b, "id", id)
        && toml_text(b, "name", name)
        && thing_toml_int(b, &t, id, "Homing", "homing")
        && thing_toml_int(b, &t, id, "SFX", "sfx")
        && toml_table(b, "sprite")
        && toml_text(b, "sheet", "sprites.png")
        && toml_text(b, "alpha", "alpha.png")
        && toml_text(b, "palettes", "palettes.png")
        && toml_int(b, "palette_rows", 1)
        && toml_int_list(b, "cell", cell, 2)
        && toml_int_list(b, "anchor", anchor, 2)
        && toml_int(b, "columns", (int32_t)sheet->columns)
        && toml_int(b, "rows", (int32_t)sheet->rows);
}
