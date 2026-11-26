/* import_objects: writes the objects of the original game as ROM2
 * content.
 *
 *     import_objects <graphics.res> <content folder> [object id]
 *
 * Every object gets a folder of its own in <content folder>/objects,
 * named after its ID and its name, with these files in it:
 *
 *     object.toml   the object's definition
 *     sprites.png   its frames, in one row
 *     edges.png     their soft edges, to draw under them
 *
 * With an object ID only that object is written.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/buffer.h"
#include "base/file.h"
#include "import/object_sheet.h"
#include "import/object_toml.h"
#include "import/objects.h"
#include "import/png.h"
#include "import/reg.h"
#include "import/res.h"
#include "import/sprite256.h"

/* Writes a sheet as a PNG file in a folder. A sheet without pixels, which
 * is the edges of an object that has none, writes nothing. */
static bool sheet_write(const Sheet *sheet, const Rgb palette[256],
    const char *folder, const char *name)
{
    if (sheet->pixels == NULL) {
        return true;
    }
    char path[1024];
    int written = snprintf(path, sizeof path, "%s/%s", folder, name);
    if (written < 0 || (size_t)written >= sizeof path) {
        return false;
    }
    ByteBuffer png = {0};
    bool ok = png_write_indexed(&png, sheet_width(sheet),
            sheet_height(sheet), sheet->pixels, palette)
        && file_write_all(path, png.data, png.size);
    buffer_free(&png);
    return ok;
}

/* Opens the sprite file at `path` in the archive. */
static bool sprite_open(const ResArchive *archive, const char *path,
    Sprite256 *out)
{
    ResEntry entry;
    return res_find(archive, path, &entry) && !entry.is_folder
        && sprite256_open(archive->data + entry.offset, entry.size, out);
}

/* Writes the folder of one object. Returns a message saying what went
 * wrong, or NULL. */
static const char *object_write(const ResArchive *archive, const Reg *reg,
    int32_t id, const char *objects_folder)
{
    Things t = objects_things(reg);
    char path[256];
    char edges_path[260];
    Sprite256 sprite;
    Sprite256 edges;
    Rgb palette[256];
    int32_t width = 0;
    int32_t height = 0;
    int32_t center_x = 0;
    int32_t center_y = 0;
    uint32_t first = 0;
    uint32_t count = 0;

    if (!objects_sprite_path(reg, id, path, sizeof path)) {
        return "the object names no sprite file";
    }
    if (!sprite_open(archive, path, &sprite)) {
        return "the object's sprite file is not in the archive";
    }
    if (!sprite256_palette(&sprite, palette)) {
        return "the object's sprite file has no palette";
    }
    if (!things_int(&t, id, "Width", &width) || width <= 0
        || !things_int(&t, id, "Height", &height) || height <= 0
        || !things_int(&t, id, "CenterX", &center_x)
        || !things_int(&t, id, "CenterY", &center_y)) {
        return "the object has no usable Width, Height, CenterX or CenterY";
    }
    if (!objects_frame_range(reg, id, sprite.frame_count, &first, &count)
        || count == 0) {
        return "the object's frames are not in its sprite file";
    }

    /* The soft edges are in a file of the same name with "b" before the
     * ending. An object without one is drawn without them. */
    snprintf(edges_path, sizeof edges_path, "%.*sb.256",
        (int)(strlen(path) - 4), path);
    bool has_edges = sprite_open(archive, edges_path, &edges)
        && edges.frame_count == sprite.frame_count;

    char name[NAME_FOLDER_SIZE];
    char folder[1024];
    if (!object_toml_folder_name(reg, id, name)) {
        return "the object's ID does not make a folder name";
    }
    int written = snprintf(folder, sizeof folder, "%s/%s", objects_folder,
        name);
    if (written < 0 || (size_t)written >= sizeof folder - 16) {
        return "the name of the content folder is too long";
    }

    ObjectSheet sheet;
    if (!object_sheet_build(&sprite, has_edges ? &edges : NULL, first, count,
            (uint32_t)width, (uint32_t)height, center_x, center_y, &sheet)) {
        return "the sheet could not be built";
    }

    const char *problem = NULL;
    ByteBuffer toml = {0};
    char toml_path[1040];
    snprintf(toml_path, sizeof toml_path, "%s/object.toml", folder);
    if (!object_toml_write(&toml, reg, id, &sheet)) {
        problem = "the object's values could not be written out";
    } else if (!file_make_folder(folder)
        || !file_write_all(toml_path, toml.data, toml.size)
        || !sheet_write(&sheet.sheet, palette, folder, "sprites.png")
        || !sheet_write(&sheet.edges, palette, folder, "edges.png")) {
        problem = "the object's folder could not be written";
    }
    buffer_free(&toml);
    object_sheet_free(&sheet);
    if (problem == NULL) {
        printf("  %s, %u frames\n", name, count);
    }
    return problem;
}

/* Does the work once the archive is in memory. `only` is the one object
 * to write, or -1 for all. Returns how many objects could not be
 * written, or -1 if nothing could be done at all. */
static int import_objects(const uint8_t *data, size_t size,
    const char *content_folder, int32_t only)
{
    ResArchive archive;
    ResEntry entry;
    Reg reg;
    uint32_t count = 0;
    if (!res_open(data, size, &archive)) {
        fprintf(stderr, "import_objects: not a .res archive\n");
        return -1;
    }
    if (!res_find(&archive, "objects/objects.reg", &entry)
        || entry.is_folder
        || !reg_open(archive.data + entry.offset, entry.size, &reg)) {
        fprintf(stderr, "import_objects: no objects/objects.reg in the "
            "archive\n");
        return -1;
    }
    Things t = objects_things(&reg);
    if (!things_count(&t, &count)) {
        fprintf(stderr, "import_objects: the registry does not say how "
            "many objects it has\n");
        return -1;
    }

    char objects_folder[1000];
    int written = snprintf(objects_folder, sizeof objects_folder,
        "%s/objects", content_folder);
    if (written < 0 || (size_t)written >= sizeof objects_folder
        || !file_make_folder(content_folder)
        || !file_make_folder(objects_folder)) {
        fprintf(stderr, "import_objects: cannot make the folder "
            "%s/objects\n", content_folder);
        return -1;
    }

    int done = 0;
    int failed = 0;
    printf("%s\n", objects_folder);
    for (uint32_t i = 0; i < count; i++) {
        int32_t id = 0;
        if (!things_id_at(&t, i, &id) || (only >= 0 && id != only)) {
            continue;
        }
        const char *problem = object_write(&archive, &reg, id,
            objects_folder);
        if (problem == NULL) {
            done++;
        } else {
            const char *name = "(no name)";
            things_text(&t, id, "DescText", &name);
            fprintf(stderr, "  object %d \"%s\" left out: %s\n", (int)id,
                name, problem);
            failed++;
        }
    }
    if (only >= 0 && done + failed == 0) {
        fprintf(stderr, "import_objects: no object has the ID %d\n",
            (int)only);
        return -1;
    }
    printf("%d objects written, %d left out\n", done, failed);
    return failed;
}

int main(int argc, char **argv)
{
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "usage: import_objects <graphics.res> "
            "<content folder> [object id]\n");
        return 2;
    }

    uint8_t *data = NULL;
    size_t size = 0;
    if (!file_read_all(argv[1], &data, &size)) {
        fprintf(stderr, "import_objects: cannot read %s\n", argv[1]);
        return 1;
    }
    int32_t only = argc == 4 ? atoi(argv[3]) : -1;
    int failed = import_objects(data, size, argv[2], only);
    free(data);
    /* Asking for one object and not getting it is a failure. Importing
     * all of them and leaving some out is reported and is not. */
    return failed < 0 || (only >= 0 && failed > 0) ? 1 : 0;
}
