/* The objects step of rom2_import: writes the objects of the original game
 * as ROM2 content.
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
#include "import/steps/steps.h"

#include <stdio.h>
#include <string.h>

#include "base/buffer.h"
#include "base/file.h"
#include "import/object.h"
#include "base/png.h"
#include "import/reg.h"
#include "import/res.h"
#include "import/sprite256.h"
#include "import/steps/shared.h"

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
    if (!step_sprite_open(archive, path, &sprite)) {
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
    bool has_edges = step_sprite_open(archive, edges_path, &edges)
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
        || !step_sheet_write(&sheet.sheet, palette, folder, "sprites.png")
        || !step_sheet_write(&sheet.edges, palette, folder, "edges.png")) {
        problem = "the object's folder could not be written";
    }
    buffer_free(&toml);
    object_sheet_free(&sheet);
    if (problem == NULL) {
        printf("  %s, %u frames\n", name, count);
    }
    return problem;
}

/* object_write() in the shape step_things() calls. */
typedef struct ObjectsContext {
    const ResArchive *archive;
    const Reg *reg;
} ObjectsContext;

static const char *object_writer(void *context, int32_t id,
    const char *folder)
{
    const ObjectsContext *c = context;
    return object_write(c->archive, c->reg, id, folder);
}

int import_objects(const ResArchive *graphics, const char *content_folder,
    int32_t only)
{
    Reg reg;
    char folder[1000];
    if (!step_reg_open("objects", graphics, "objects/objects.reg", &reg)
        || !step_folder("objects", content_folder, "objects", folder,
            sizeof folder)) {
        return -1;
    }
    Things things = objects_things(&reg);
    ObjectsContext context = {graphics, &reg};
    return step_things("objects", "object", &things, folder, only,
        object_writer, &context);
}
