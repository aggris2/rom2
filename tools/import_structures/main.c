/* import_structures: writes the structures of the original game as ROM2
 * content.
 *
 *     import_structures <graphics.res> <world.res> <content folder> [id]
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
#include <stdio.h>
#include <stdlib.h>
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

/* Does the work once the archives are in memory. `only` is the one
 * structure to write, or -1 for all. Returns how many structures could
 * not be written, or -1 if nothing could be done at all. */
static int import_structures(const uint8_t *data, size_t size,
    const uint8_t *world_data, size_t world_size,
    const char *content_folder, int32_t only)
{
    ResArchive archive;
    ResArchive world;
    ResEntry entry;
    Reg reg;
    DataBin bin;
    uint32_t count = 0;
    if (!res_open(data, size, &archive)) {
        fprintf(stderr, "import_structures: not a .res archive\n");
        return -1;
    }
    if (!res_find(&archive, "structures/structures.reg", &entry)
        || entry.is_folder
        || !reg_open(archive.data + entry.offset, entry.size, &reg)) {
        fprintf(stderr, "import_structures: no structures/structures.reg "
            "in the archive\n");
        return -1;
    }
    if (!res_open(world_data, world_size, &world)
        || !res_find(&world, "data/data.bin", &entry) || entry.is_folder
        || !databin_open(world.data + entry.offset, entry.size, &bin)) {
        fprintf(stderr, "import_structures: no usable data/data.bin in "
            "the second archive\n");
        return -1;
    }
    Things t = structures_things(&reg);
    if (!things_count(&t, &count)) {
        fprintf(stderr, "import_structures: the registry does not say "
            "how many structures it has\n");
        return -1;
    }

    char structures_folder[1000];
    int written = snprintf(structures_folder, sizeof structures_folder,
        "%s/structures", content_folder);
    if (written < 0 || (size_t)written >= sizeof structures_folder
        || !file_make_folder(content_folder)
        || !file_make_folder(structures_folder)) {
        fprintf(stderr, "import_structures: cannot make the folder "
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
        const char *problem = structure_write(&archive, &reg, &bin, id,
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
        fprintf(stderr, "import_structures: no structure has the ID %d\n",
            (int)only);
        return -1;
    }
    printf("%d structures written, %d left out\n", done, failed);
    return failed;
}

int main(int argc, char **argv)
{
    if (argc != 4 && argc != 5) {
        fprintf(stderr, "usage: import_structures <graphics.res> "
            "<world.res> <content folder> [id]\n");
        return 2;
    }

    uint8_t *data = NULL;
    uint8_t *world = NULL;
    size_t size = 0;
    size_t world_size = 0;
    if (!file_read_all(argv[1], &data, &size)) {
        fprintf(stderr, "import_structures: cannot read %s\n", argv[1]);
        return 1;
    }
    if (!file_read_all(argv[2], &world, &world_size)) {
        fprintf(stderr, "import_structures: cannot read %s\n", argv[2]);
        free(data);
        return 1;
    }
    int32_t only = argc == 5 ? atoi(argv[4]) : -1;
    int failed = import_structures(data, size, world, world_size, argv[3],
        only);
    free(world);
    free(data);
    /* Asking for one structure and not getting it is a failure.
     * Importing all of them and leaving some out is reported and is
     * not. */
    return failed < 0 || (only >= 0 && failed > 0) ? 1 : 0;
}
