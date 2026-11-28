#include "import/steps/shared.h"

#include <stdio.h>
#include <string.h>

#include "base/buffer.h"
#include "base/file.h"
#include "base/png.h"
#include "base/toml.h"
#include "import/cursor.h"
#include "import/plain_sheet.h"

bool step_reg_open(const char *step, const ResArchive *archive,
    const char *path, Reg *out)
{
    ResEntry entry;
    if (!res_find(archive, path, &entry) || entry.is_folder
        || !reg_open(archive->data + entry.offset, entry.size, out)) {
        fprintf(stderr, "%s: no %s in graphics.res\n", step, path);
        return false;
    }
    return true;
}

bool step_databin_open(const char *step, const ResArchive *world,
    DataBin *out)
{
    ResEntry entry;
    if (!res_find(world, "data/data.bin", &entry) || entry.is_folder
        || !databin_open(world->data + entry.offset, entry.size, out)) {
        fprintf(stderr, "%s: no usable data/data.bin in world.res\n", step);
        return false;
    }
    return true;
}

bool step_sprite_open(const ResArchive *archive, const char *path,
    Sprite256 *out)
{
    ResEntry entry;
    return res_find(archive, path, &entry) && !entry.is_folder
        && sprite256_open(archive->data + entry.offset, entry.size, out);
}

bool step_folder(const char *step, const char *content_folder,
    const char *name, char *out, size_t size)
{
    int written = snprintf(out, size, "%s/%s", content_folder, name);
    if (written < 0 || (size_t)written >= size
        || !file_make_folder(content_folder) || !file_make_folder(out)) {
        fprintf(stderr, "%s: cannot make the folder %s/%s\n", step,
            content_folder, name);
        return false;
    }
    return true;
}

bool step_subfolder(const char *parent, const char *name, char *out,
    size_t size)
{
    int written = snprintf(out, size, "%s/%s", parent, name);
    return written > 0 && (size_t)written < size && file_make_folder(out);
}

bool step_write(const char *folder, const char *name,
    const ByteBuffer *bytes)
{
    char path[1200];
    int written = snprintf(path, sizeof path, "%s/%s", folder, name);
    return written > 0 && (size_t)written < sizeof path
        && file_write_all(path, bytes->data, bytes->size);
}

bool step_sheet_write(const Sheet *sheet, const Rgb palette[256],
    const char *folder, const char *name)
{
    if (sheet->pixels == NULL) {
        return true;
    }
    ByteBuffer png = {0};
    bool ok = png_write_indexed(&png, sheet_width(sheet),
            sheet_height(sheet), sheet->pixels, palette)
        && step_write(folder, name, &png);
    buffer_free(&png);
    return ok;
}

int step_things(const char *step, const char *noun, const Things *things,
    const char *folder, int32_t only, StepThingWriter *writer,
    void *context)
{
    uint32_t count = 0;
    if (!things_count(things, &count)) {
        fprintf(stderr, "%s: the registry does not say how many %s it "
            "has\n", step, step);
        return -1;
    }
    int done = 0;
    int failed = 0;
    printf("%s\n", folder);
    for (uint32_t i = 0; i < count; i++) {
        int32_t id = 0;
        if (!things_id_at(things, i, &id) || (only >= 0 && id != only)) {
            continue;
        }
        const char *problem = writer(context, id, folder);
        if (problem == NULL) {
            done++;
            continue;
        }
        const char *name = "(no name)";
        things_text(things, id, "DescText", &name);
        fprintf(stderr, "  %s %d \"%s\" left out: %s\n", noun, (int)id,
            name, problem);
        failed++;
    }
    if (only >= 0 && done + failed == 0) {
        fprintf(stderr, "%s: no %s has the ID %d\n", step, noun, (int)only);
        return -1;
    }
    printf("%d %s written, %d left out\n", done, step, failed);
    return failed;
}

/* The deepest folders may be nested. The original goes 5 deep. */
#define MAX_DEPTH 16
#define PATH_SIZE 1200

typedef struct Walk {
    const ResArchive *archive;
    const char *ending;
    const char *new_ending;
    TreeWriter *writer;
    TreeSkip *skip;
    TreeCount *count;
} Walk;

bool tree_copy(const uint8_t *data, size_t size, const char *path)
{
    return file_write_all(path, data, size);
}

/* Says whether `name` ends in `ending` and has something before it. */
static bool ends_in(const char *name, const char *ending)
{
    size_t length = strlen(name);
    size_t end = strlen(ending);
    return length > end && strcmp(name + length - end, ending) == 0;
}

/* Makes the folder at `path` and every folder above it that is not
 * there yet. */
static bool make_folders(const char *path)
{
    char part[PATH_SIZE];
    size_t length = strlen(path);
    if (length >= sizeof part) {
        return false;
    }
    /* Each longer start of the path in turn. One that is there already,
     * or cannot be a folder at all, such as "C:", does no harm. */
    for (size_t i = 1; i < length; i++) {
        if (path[i] == '/') {
            memcpy(part, path, i);
            part[i] = '\0';
            file_make_folder(part);
        }
    }
    return file_make_folder(path);
}

/* Goes through the folder made of `count` entries starting at entry
 * `first`, and the folders in it. `folder` is the folder on disk that
 * is to hold its files. */
static void walk(const Walk *w, uint32_t first, uint32_t count,
    const char *folder, int depth)
{
    bool made = false;
    for (uint32_t i = 0; i < count; i++) {
        ResEntry entry;
        char path[PATH_SIZE];
        if (!res_entry_at(w->archive, first + i, &entry)) {
            continue;
        }
        if (!entry.is_folder && (!ends_in(entry.name, w->ending)
                || (w->skip != NULL && w->skip(entry.name)))) {
            continue;
        }
        int written = snprintf(path, sizeof path, "%s/%s", folder,
            entry.name);
        /* Room is kept for the new ending, which may be the longer. */
        if (written < 0
            || (size_t)written + strlen(w->new_ending) >= sizeof path
            || depth >= MAX_DEPTH) {
            w->count->failed++;
            continue;
        }
        if (entry.is_folder) {
            walk(w, entry.offset, entry.size, path, depth + 1);
            continue;
        }
        if (!made) {
            made = make_folders(folder);
        }
        strcpy(path + strlen(path) - strlen(w->ending), w->new_ending);
        if (w->writer(w->archive->data + entry.offset, entry.size, path)) {
            res_note(w->archive, first + i);
            w->count->written++;
        } else {
            fprintf(stderr, "  %s left out: it could not be written\n",
                path);
            w->count->failed++;
        }
    }
}

void tree_write(const ResArchive *archive, const char *inside,
    const char *folder, const char *ending, const char *new_ending,
    TreeWriter *writer, TreeCount *count)
{
    tree_write_but(archive, inside, folder, ending, new_ending, writer,
        NULL, count);
}

void tree_write_but(const ResArchive *archive, const char *inside,
    const char *folder, const char *ending, const char *new_ending,
    TreeWriter *writer, TreeSkip *skip, TreeCount *count)
{
    Walk w = {archive, ending, new_ending, writer, skip, count};
    uint32_t first = archive->header.root_first;
    uint32_t entries = archive->header.root_count;
    if (inside != NULL) {
        ResEntry entry;
        if (!res_find(archive, inside, &entry) || !entry.is_folder) {
            fprintf(stderr, "  no folder %s in the archive\n", inside);
            count->failed++;
            return;
        }
        first = entry.offset;
        entries = entry.size;
    }
    walk(&w, first, entries, folder, 0);
}

void tree_write_in(const ResArchive *archive, const char *inside,
    const char *folder, const char *to, const char *ending,
    const char *new_ending, TreeWriter *writer, TreeCount *count)
{
    char path[PATH_SIZE];
    int written = snprintf(path, sizeof path, "%s/%s", folder, to);
    if (written < 0 || (size_t)written >= sizeof path) {
        count->failed++;
        return;
    }
    tree_write(archive, inside, path, ending, new_ending, writer, count);
}

/* Writes the bytes of a buffer as the file whose path is `base` with
 * `ending` after it. */
static bool write_as(const char *base, const char *ending,
    const ByteBuffer *bytes)
{
    char path[1300];
    int written = snprintf(path, sizeof path, "%s%s", base, ending);
    return written > 0 && (size_t)written < sizeof path
        && file_write_all(path, bytes->data, bytes->size);
}

/* Returns the name of the file at `path`, without its folders. */
static const char *file_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

/* Writes a sprite as its sheet, its solidness if it is a .16a, and its
 * definition. `base` is the path of the files without their endings.
 * The colours are those of `palette_from`, which is the sprite itself
 * but for a sprite of soft edges, which has the colours of its figure. */
bool sprite_files_write(const Sprite256 *sprite,
    const Sprite256 *palette_from, bool is_16a, const char *base)
{
    Rgb palette[256];
    PlainSheet sheet;
    if (!sprite256_palette(palette_from, palette)
        || !(is_16a ? plain_sheet_build(sprite, PLAIN_SHEET_16A, 0, &sheet)
            : plain_sheet_build(sprite, PLAIN_SHEET_256, 0, &sheet))) {
        return false;
    }
    uint32_t width = plain_sheet_width(&sheet);
    uint32_t height = plain_sheet_height(&sheet);
    char sheet_name[80];
    char alpha_name[80];
    snprintf(sheet_name, sizeof sheet_name, "%.60s_sheet.png",
        file_name(base));
    snprintf(alpha_name, sizeof alpha_name, "%.60s_alpha.png",
        file_name(base));
    int32_t cell[2] = {(int32_t)sheet.cell_width, (int32_t)sheet.cell_height};

    /* In a .256 the index 0 is a pixel that is not there. In a .16a it
     * is a colour like the others, and the solidness says what is
     * there. */
    ByteBuffer out = {0};
    bool ok = (is_16a
            ? png_write_indexed_solid(&out, width, height, sheet.pixels,
                palette)
            : png_write_indexed(&out, width, height, sheet.pixels, palette))
        && write_as(base, "_sheet.png", &out);
    out.size = 0;
    ok = ok && (!is_16a
        || (png_write_grey(&out, width, height, sheet.solid)
            && write_as(base, "_alpha.png", &out)));
    out.size = 0;
    /* A cursor says which pixel of it points: see import/cursor.h. */
    int32_t tip[2];
    bool has_tip = cursor_tip(base, tip);
    ok = ok && toml_text(&out, "sheet", sheet_name)
        && (!is_16a || toml_text(&out, "alpha", alpha_name))
        && toml_int(&out, "frames", (int32_t)sheet.frames)
        && toml_int(&out, "columns", (int32_t)sheet.columns)
        && toml_int_list(&out, "cell", cell, 2)
        && (!has_tip || toml_int_list(&out, "tip", tip, 2))
        && (sheet.one_size
            || (toml_comment(&out, "The frames are not all as large as "
                    "a cell. Each lies in the top left corner of its "
                    "own.")
                && toml_int_list(&out, "widths", sheet.widths, sheet.frames)
                && toml_int_list(&out, "heights", sheet.heights,
                    sheet.frames)))
        && write_as(base, ".toml", &out);
    buffer_free(&out);
    plain_sheet_free(&sheet);
    return ok;
}

/* Writes the sprite in `data`. `path` is where the walk would put it,
 * with ".png" at its end: the files are named after it without that. */
static bool file_write(const uint8_t *data, size_t size,
    const char *path, bool is_16a)
{
    char base[1250];
    size_t length = strlen(path);
    Sprite256 sprite;
    /* The original has a few empty files among its cursors. There is
     * nothing to write for one, and nothing has gone wrong. */
    if (size == 0) {
        return true;
    }
    if (length < 4 || length - 4 >= sizeof base
        || !sprite256_open(data, size, &sprite)) {
        return false;
    }
    memcpy(base, path, length - 4);
    base[length - 4] = '\0';
    return sprite_files_write(&sprite, &sprite, is_16a, base);
}

bool sprite_files_write_256(const uint8_t *data, size_t size,
    const char *path)
{
    return file_write(data, size, path, false);
}

bool sprite_files_write_16a(const uint8_t *data, size_t size,
    const char *path)
{
    return file_write(data, size, path, true);
}
