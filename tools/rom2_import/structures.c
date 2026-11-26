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
#include "rom2_import/steps.h"

#include <stdio.h>
#include <string.h>

#include "base/buffer.h"
#include "base/file.h"
#include "import/databin.h"
#include "import/png.h"
#include "import/reg.h"
#include "import/res.h"
#include "import/sprite256.h"
#include "import/structure_sheet.h"
#include "import/structure_toml.h"
#include "import/structures.h"

/* Writes a sheet as a PNG file in a folder. A sheet without pixels, one
 * the structure does not have, writes nothing. */
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
    if (!sprite_open(archive, path, &sprite)) {
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
    bool has_edges = sprite_open(archive, edges_path, &edges)
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
        || !sheet_write(&sheet.sheet, palette, folder, "sprites.png")
        || !sheet_write(&sheet.edges, palette, folder, "edges.png")
        || !sheet_write(&sheet.ruin, palette, folder, "ruin.png")
        || !sheet_write(&sheet.ruin_edges, palette, folder,
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

int import_structures(const ResArchive *graphics, const ResArchive *world,
    const char *content_folder, int32_t only)
{
    ResEntry entry;
    Reg reg;
    DataBin bin;
    uint32_t count = 0;
    if (!res_find(graphics, "structures/structures.reg", &entry)
        || entry.is_folder
        || !reg_open(graphics->data + entry.offset, entry.size, &reg)) {
        fprintf(stderr, "structures: no structures/structures.reg "
            "in graphics.res\n");
        return -1;
    }
    if (!res_find(world, "data/data.bin", &entry) || entry.is_folder
        || !databin_open(world->data + entry.offset, entry.size, &bin)) {
        fprintf(stderr, "structures: no usable data/data.bin in "
            "world.res\n");
        return -1;
    }
    Things t = structures_things(&reg);
    if (!things_count(&t, &count)) {
        fprintf(stderr, "structures: the registry does not say "
            "how many structures it has\n");
        return -1;
    }

    char structures_folder[1000];
    int written = snprintf(structures_folder, sizeof structures_folder,
        "%s/structures", content_folder);
    if (written < 0 || (size_t)written >= sizeof structures_folder
        || !file_make_folder(content_folder)
        || !file_make_folder(structures_folder)) {
        fprintf(stderr, "structures: cannot make the folder "
            "%s/structures\n", content_folder);
        return -1;
    }

    int done = 0;
    int failed = 0;
    printf("%s\n", structures_folder);
    for (uint32_t i = 0; i < count; i++) {
        int32_t id = 0;
        if (!things_id_at(&t, i, &id) || (only >= 0 && id != only)) {
            continue;
        }
        const char *problem = structure_write(graphics, &reg, &bin, id,
            structures_folder);
        if (problem == NULL) {
            done++;
        } else {
            const char *name = "(no name)";
            things_text(&t, id, "DescText", &name);
            fprintf(stderr, "  structure %d \"%s\" left out: %s\n", (int)id,
                name, problem);
            failed++;
        }
    }
    if (only >= 0 && done + failed == 0) {
        fprintf(stderr, "structures: no structure has the ID %d\n",
            (int)only);
        return -1;
    }
    printf("%d structures written, %d left out\n", done, failed);
    return failed;
}
