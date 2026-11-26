/* The projectiles step of rom2_import: writes the projectiles of the
 * original game as ROM2 content.
 *
 * Every projectile gets a folder of its own in <content folder>/
 * projectiles, named after its ID and its sprite, with these files:
 *
 *     projectile.toml   the projectile's definition
 *     sprites.png       its pictures: a row for each direction
 *     alpha.png         how solid each pixel of them is
 *     palettes.png      its colours
 *
 * With a projectile ID only that projectile is written.
 */
#include "rom2_import/steps.h"

#include <stdio.h>
#include <string.h>

#include "base/buffer.h"
#include "base/file.h"
#include "import/pal.h"
#include "import/png.h"
#include "import/projectile_sheet.h"
#include "import/projectile_toml.h"
#include "import/projectiles.h"
#include "import/reg.h"
#include "import/res.h"
#include "import/sprite256.h"

/* The palette of the projectiles whose sprite file has none. */
#define SHARED_PALETTE "projectiles/projectiles.pal"

/* Opens the sprite file at `path` in the archive. */
static bool sprite_open(const ResArchive *archive, const char *path,
    Sprite256 *out)
{
    ResEntry entry;
    return res_find(archive, path, &entry) && !entry.is_folder
        && sprite256_open(archive->data + entry.offset, entry.size, out);
}

/* Reads the palette a projectile is drawn with: its sprite file's own,
 * or the shared one. */
static bool palette_read(const ResArchive *archive, const Sprite256 *sprite,
    Rgb out[256])
{
    ResEntry entry;
    if (sprite256_palette(sprite, out)) {
        return true;
    }
    return res_find(archive, SHARED_PALETTE, &entry) && !entry.is_folder
        && pal_bmp_read(archive->data + entry.offset, entry.size, out);
}

/* Writes the bytes of a buffer as the file `name` in a folder. */
static bool write_in(const char *folder, const char *name,
    const ByteBuffer *bytes)
{
    char path[1100];
    int written = snprintf(path, sizeof path, "%s/%s", folder, name);
    return written > 0 && (size_t)written < sizeof path
        && file_write_all(path, bytes->data, bytes->size);
}

/* Writes the folder of one projectile. Returns a message saying what
 * went wrong, or NULL. */
static const char *projectile_write(const ResArchive *archive,
    const Reg *reg, int32_t id, const char *projectiles_folder)
{
    char path[256];
    char edges_path[260];
    bool is_16a = false;
    Sprite256 sprite;
    Sprite256 edges;
    Rgb palette[256];
    ProjectileLayout layout;

    if (!projectiles_sprite_path(reg, id, path, sizeof path, &is_16a)) {
        return "the projectile names no sprite file";
    }
    if (!sprite_open(archive, path, &sprite)) {
        return "the projectile's sprite file is not in the archive";
    }
    if (!palette_read(archive, &sprite, palette)) {
        return "there is no palette for the projectile";
    }
    if (!projectiles_layout(reg, id, sprite.frame_count, &layout)) {
        return "the sprite file does not have the frames the registry "
            "describes";
    }

    char name[NAME_FOLDER_SIZE];
    char folder[1024];
    if (!projectile_toml_folder_name(reg, id, name)) {
        return "the projectile's ID does not make a folder name";
    }
    int written = snprintf(folder, sizeof folder, "%s/%s",
        projectiles_folder, name);
    if (written < 0 || (size_t)written >= sizeof folder - 20) {
        return "the name of the content folder is too long";
    }

    ProjectileSheet sheet;
    bool built;
    if (is_16a) {
        built = projectile_sheet_build_16a(&layout, &sprite, &sheet);
    } else {
        /* The soft edges are in a file of the same name with "b" before
         * the ending. */
        snprintf(edges_path, sizeof edges_path, "%.*sb.256",
            (int)(strlen(path) - 4), path);
        bool has_edges = sprite_open(archive, edges_path, &edges)
            && edges.frame_count == sprite.frame_count;
        built = projectile_sheet_build_256(&layout, &sprite,
            has_edges ? &edges : NULL, &sheet);
    }
    if (!built) {
        return "the sheet could not be built";
    }

    const char *problem = NULL;
    uint32_t width = projectile_sheet_width(&sheet);
    uint32_t height = projectile_sheet_height(&sheet);
    ByteBuffer toml = {0};
    ByteBuffer colours = {0};
    ByteBuffer alpha = {0};
    ByteBuffer palettes = {0};
    if (!projectile_toml_write(&toml, reg, id, &sheet)
        || !png_write_indexed(&colours, width, height, sheet.pixels,
            palette)
        || !png_write_grey(&alpha, width, height, sheet.solid)
        || !png_write_rgb(&palettes, 256, 1, palette)) {
        problem = "the projectile's files could not be made";
    } else if (!file_make_folder(folder)
        || !write_in(folder, "projectile.toml", &toml)
        || !write_in(folder, "sprites.png", &colours)
        || !write_in(folder, "alpha.png", &alpha)
        || !write_in(folder, "palettes.png", &palettes)) {
        problem = "the projectile's folder could not be written";
    }
    buffer_free(&toml);
    buffer_free(&colours);
    buffer_free(&alpha);
    buffer_free(&palettes);
    projectile_sheet_free(&sheet);
    if (problem == NULL) {
        printf("  %s, %u directions of %u pictures", name, layout.steps,
            layout.phases);
        if (layout.spare_frames > 0) {
            printf(", %u frames of the file not used", layout.spare_frames);
        }
        printf("\n");
    }
    return problem;
}

int import_projectiles(const ResArchive *graphics,
    const char *content_folder, int32_t only)
{
    ResEntry entry;
    Reg reg;
    uint32_t count = 0;
    if (!res_find(graphics, "projectiles/projectiles.reg", &entry)
        || entry.is_folder
        || !reg_open(graphics->data + entry.offset, entry.size, &reg)) {
        fprintf(stderr, "projectiles: no projectiles/"
            "projectiles.reg in graphics.res\n");
        return -1;
    }
    Things t = projectiles_things(&reg);
    if (!things_count(&t, &count)) {
        fprintf(stderr, "projectiles: the registry does not say "
            "how many projectiles it has\n");
        return -1;
    }

    char projectiles_folder[1000];
    int written = snprintf(projectiles_folder, sizeof projectiles_folder,
        "%s/projectiles", content_folder);
    if (written < 0 || (size_t)written >= sizeof projectiles_folder
        || !file_make_folder(content_folder)
        || !file_make_folder(projectiles_folder)) {
        fprintf(stderr, "projectiles: cannot make the folder "
            "%s/projectiles\n", content_folder);
        return -1;
    }

    int done = 0;
    int failed = 0;
    printf("%s\n", projectiles_folder);
    for (uint32_t i = 0; i < count; i++) {
        int32_t id = 0;
        if (!things_id_at(&t, i, &id) || (only >= 0 && id != only)) {
            continue;
        }
        const char *problem = projectile_write(graphics, &reg, id,
            projectiles_folder);
        if (problem == NULL) {
            done++;
        } else {
            fprintf(stderr, "  projectile %d left out: %s\n", (int)id,
                problem);
            failed++;
        }
    }
    if (only >= 0 && done + failed == 0) {
        fprintf(stderr, "projectiles: no projectile has the ID "
            "%d\n", (int)only);
        return -1;
    }
    printf("%d projectiles written, %d left out\n", done, failed);
    return failed;
}
