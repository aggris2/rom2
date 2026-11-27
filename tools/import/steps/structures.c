/* The structures step of rom2_import: writes the structures of the original
 * game as ROM2 content.
 *
 * Every structure gets a folder of its own in <content folder>/structures,
 * named after its ID and its name, with these files in it:
 *
 *     structure.toml   the structure's definition, with its numbers from
 *                      data.bin in world.res
 *     sprites.png      the pictures of its animation, in one row
 *     ruin.png         the structure destroyed, if it can be
 *     edges.png, ruin_edges.png
 *                      the soft edges of those two, to draw under them
 *
 * With a structure ID only that structure is written.
 */
#include "import/steps/steps.h"

#include <stdio.h>
#include <string.h>

#include "base/buffer.h"
#include "base/file.h"
#include "import/databin.h"
#include "base/png.h"
#include "import/reg.h"
#include "import/res.h"
#include "import/sprite256.h"
#include "import/structure.h"
#include "import/steps/shared.h"

/* Writes the folder of one structure. Returns a message saying what
 * went wrong, or NULL. */
static const char *structure_write(const ResArchive *archive,
    const Reg *reg, const DataBin *bin, int32_t id,
    const char *structures_folder)
{
    char path[256];
    char edges_path[260];
    Sprite256 sprite;
    Sprite256 edges;
    Rgb palette[256];
    StructureLayout layout;

    if (!structures_sprite_path(reg, id, path, sizeof path)) {
        return "the structure names no sprite file";
    }
    if (!step_sprite_open(archive, path, &sprite)) {
        return "the structure's sprite file is not in the archive";
    }
    if (!sprite256_palette(&sprite, palette)) {
        return "the structure's sprite file has no palette";
    }
    if (!structures_layout(reg, id, sprite.frame_count, &layout)) {
        return "the sprite file does not have the tiles the registry "
            "describes";
    }

    /* The soft edges are in a file of the same name with "b" before the
     * ending. A structure without one is drawn without them. */
    snprintf(edges_path, sizeof edges_path, "%.*sb.256",
        (int)(strlen(path) - 4), path);
    bool has_edges = step_sprite_open(archive, edges_path, &edges)
        && edges.frame_count == sprite.frame_count;

    char name[NAME_FOLDER_SIZE];
    char folder[1024];
    if (!structure_toml_folder_name(reg, id, name)) {
        return "the structure's ID does not make a folder name";
    }
    int written = snprintf(folder, sizeof folder, "%s/%s",
        structures_folder, name);
    if (written < 0 || (size_t)written >= sizeof folder - 20) {
        return "the name of the content folder is too long";
    }

    StructureSheet sheet;
    if (!structure_sheet_build(&layout, &sprite, has_edges ? &edges : NULL,
            &sheet)) {
        return "the sheets could not be built";
    }

    const char *problem = NULL;
    ByteBuffer toml = {0};
    char toml_path[1048];
    snprintf(toml_path, sizeof toml_path, "%s/structure.toml", folder);
    if (!structure_toml_write(&toml, reg, id, &layout, &sheet, bin)) {
        problem = "the structure's values could not be written out";
    } else if (!file_make_folder(folder)
        || !file_write_all(toml_path, toml.data, toml.size)
        || !step_sheet_write(&sheet.sheet, palette, folder, "sprites.png")
        || !step_sheet_write(&sheet.edges, palette, folder, "edges.png")
        || !step_sheet_write(&sheet.ruin, palette, folder, "ruin.png")
        || !step_sheet_write(&sheet.ruin_edges, palette, folder,
            "ruin_edges.png")) {
        problem = "the structure's folder could not be written";
    }
    buffer_free(&toml);
    structure_sheet_free(&sheet);
    if (problem == NULL) {
        printf("  %s, %u %s%s\n", name,
            layout.is_pieces ? layout.pieces : layout.phases,
            layout.is_pieces ? "pieces" : "pictures",
            layout.has_ruin ? " and a ruin" : "");
    }
    return problem;
}

/* structure_write() in the shape step_things() calls. */
typedef struct StructuresContext {
    const ResArchive *archive;
    const Reg *reg;
    const DataBin *bin;
} StructuresContext;

static const char *structure_writer(void *context, int32_t id,
    const char *folder)
{
    const StructuresContext *c = context;
    return structure_write(c->archive, c->reg, c->bin, id, folder);
}

int import_structures(const ResArchive *graphics, const ResArchive *world,
    const char *content_folder, int32_t only)
{
    Reg reg;
    DataBin bin;
    char folder[1000];
    if (!step_reg_open("structures", graphics, "structures/structures.reg",
            &reg)
        || !step_databin_open("structures", world, &bin)
        || !step_folder("structures", content_folder, "structures", folder,
            sizeof folder)) {
        return -1;
    }
    Things things = structures_things(&reg);
    StructuresContext context = {graphics, &reg, &bin};
    return step_things("structures", "structure", &things, folder, only,
        structure_writer, &context);
}
