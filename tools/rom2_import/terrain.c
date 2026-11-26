/* The terrain step of rom2_import: writes the ground tiles of the original
 * game as ROM2 content.
 *
 * In <content folder>/terrain this makes:
 *
 *     tiles.png      every ground tile in one picture: a column for
 *                    each of the original's tile pictures, and in it
 *                    that picture's tiles from the top down
 *     dirt.png       the tiles of the original's dirt.bmp, as they are
 *     terrain.toml   the size of a tile and how many each column has
 *
 * A map names a ground tile by two numbers, which are the column and the
 * row of tiles.png.
 */
#include "rom2_import/steps.h"

#include <stdio.h>
#include <string.h>

#include "base/buffer.h"
#include "base/file.h"
#include "import/bmp.h"
#include "import/png.h"
#include "import/res.h"
#include "import/sheet.h"
#include "import/toml.h"

/* The side of a tile in pixels, and the most tiles a column may have:
 * the number a map can name, which is 16. */
#define TILE 32u
#define MAX_ROWS 16u
/* The original has its tile pictures in four groups of up to 16. */
#define GROUPS 4u
#define GROUP_SIZE 16u
#define MAX_COLUMNS (GROUPS * GROUP_SIZE)

/* Writes the bytes of a buffer as the file `name` in a folder. */
static bool write_in(const char *folder, const char *name,
    const ByteBuffer *bytes)
{
    char path[1100];
    int written = snprintf(path, sizeof path, "%s/%s", folder, name);
    return written > 0 && (size_t)written < sizeof path
        && file_write_all(path, bytes->data, bytes->size);
}

/* Reads the 8-bit BMP at `path` in the archive. */
static bool bmp_open(const ResArchive *archive, const char *path, Bmp8 *out)
{
    ResEntry entry;
    return res_find(archive, path, &entry) && !entry.is_folder
        && bmp8_read(archive->data + entry.offset, entry.size, out);
}

/* Does the work. Returns a message saying what went wrong, or NULL. */
static const char *terrain_write(const ResArchive *archive,
    const char *content_folder)
{
    /* The tile pictures are terrain/tile1-00.bmp to tile4-15.bmp. Their
     * place in that order is the number a map names them by. They are
     * read first, to learn how many there are and how tall. */
    static Bmp8 pictures[MAX_COLUMNS];
    int32_t rows[MAX_COLUMNS];
    uint32_t columns = 0;
    uint32_t most_rows = 0;
    const char *problem = NULL;
    for (uint32_t n = 0; n < MAX_COLUMNS && problem == NULL; n++) {
        char path[64];
        snprintf(path, sizeof path, "terrain/tile%u-%02u.bmp",
            n / GROUP_SIZE + 1, n % GROUP_SIZE);
        if (!bmp_open(archive, path, &pictures[n])) {
            /* The last group is not full: the pictures end here. */
            break;
        }
        columns = n + 1;
        const Bmp8 *p = &pictures[n];
        if (p->width != TILE || p->height % TILE != 0
            || p->height / TILE > MAX_ROWS) {
            problem = "a tile picture is not a column of 32 by 32 tiles";
        } else if (memcmp(p->palette, pictures[0].palette,
                sizeof p->palette) != 0) {
            problem = "the tile pictures do not share one palette";
        } else {
            rows[n] = (int32_t)(p->height / TILE);
            if (p->height / TILE > most_rows) {
                most_rows = p->height / TILE;
            }
        }
    }
    if (problem == NULL && columns == 0) {
        problem = "no terrain/tile1-00.bmp in the archive";
    }

    char folder[1024];
    int written = snprintf(folder, sizeof folder, "%s/terrain",
        content_folder);
    Sheet sheet = {0};
    ByteBuffer out = {0};
    if (problem == NULL && (written < 0 || (size_t)written >= sizeof folder
            || !file_make_folder(content_folder)
            || !file_make_folder(folder))) {
        problem = "cannot make the terrain folder";
    }
    if (problem == NULL
        && !sheet_create(&sheet, TILE, TILE, columns, most_rows)) {
        problem = "the picture of the tiles could not be made";
    }
    /* Each tile to its cell. A cell is filled whole, so the 0 pixels a
     * sheet leaves alone are right as they are. */
    for (uint32_t n = 0; problem == NULL && n < columns; n++) {
        for (uint32_t row = 0; row < (uint32_t)rows[n]; row++) {
            if (!sheet_put(&sheet, n, row,
                    pictures[n].pixels + (size_t)row * TILE * TILE, TILE,
                    TILE, 0, 0, false)) {
                problem = "a tile could not be put on the picture";
            }
        }
    }
    if (problem == NULL
        && (!png_write_indexed_solid(&out, sheet_width(&sheet),
                sheet_height(&sheet), sheet.pixels, pictures[0].palette)
            || !write_in(folder, "tiles.png", &out))) {
        problem = "tiles.png could not be written";
    }

    /* The dirt tiles have a palette of their own and stay a picture of
     * their own. */
    Bmp8 dirt = {0};
    bool has_dirt = problem == NULL
        && bmp_open(archive, "terrain/dirt.bmp", &dirt);
    out.size = 0;
    if (has_dirt
        && (!png_write_indexed_solid(&out, dirt.width, dirt.height,
                dirt.pixels, dirt.palette)
            || !write_in(folder, "dirt.png", &out))) {
        problem = "dirt.png could not be written";
    }

    out.size = 0;
    if (problem == NULL
        && (!toml_comment(&out, "The ground tiles. A map names a tile by "
                "its column and its row in the sheet.")
            || !toml_int(&out, "tile_size", (int32_t)TILE)
            || !toml_text(&out, "sheet", "tiles.png")
            || !toml_int(&out, "columns", (int32_t)columns)
            || !toml_int_list(&out, "rows", rows, columns)
            || (has_dirt && !toml_text(&out, "dirt", "dirt.png"))
            || !write_in(folder, "terrain.toml", &out))) {
        problem = "terrain.toml could not be written";
    }
    if (problem == NULL) {
        uint32_t tiles = 0;
        for (uint32_t n = 0; n < columns; n++) {
            tiles += (uint32_t)rows[n];
        }
        printf("%s\n  tiles.png, %u columns of up to %u tiles, %u tiles, "
            "%u x %u pixels\n", folder, columns, most_rows, tiles,
            sheet_width(&sheet), sheet_height(&sheet));
        if (has_dirt) {
            printf("  dirt.png, %u x %u pixels\n", dirt.width, dirt.height);
        }
    }

    buffer_free(&out);
    sheet_free(&sheet);
    bmp8_free(&dirt);
    for (uint32_t n = 0; n < MAX_COLUMNS; n++) {
        bmp8_free(&pictures[n]);
    }
    return problem;
}

int import_terrain(const ResArchive *graphics, const char *content_folder)
{
    const char *problem = terrain_write(graphics, content_folder);
    if (problem != NULL) {
        fprintf(stderr, "terrain: %s\n", problem);
        return -1;
    }
    return 0;
}
