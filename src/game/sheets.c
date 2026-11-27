#include "game/sheets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "base/file.h"
#include "base/png.h"
#include "base/toml.h"

/* Reads a PNG file, with a message if it cannot. */
static bool read_png(const char *path, Png *png)
{
    uint8_t *data = NULL;
    size_t size = 0;
    if (!file_read_all(path, &data, &size)) {
        fprintf(stderr, "rom2: cannot read %s\n", path);
        return false;
    }
    bool ok = png_read(data, size, png);
    free(data);
    if (!ok) {
        fprintf(stderr, "rom2: %s is not a PNG picture\n", path);
    }
    return ok;
}

bool sheets_load_picture(Renderer *renderer, const char *path,
                         uint16_t *sheet, uint32_t *width,
                         uint32_t *height)
{
    Png png;
    if (!read_png(path, &png)) {
        return false;
    }
    bool ok = png.kind == PNG_INDEXED;
    if (!ok) {
        fprintf(stderr, "rom2: %s has no palette\n", path);
    } else {
        ok = renderer_add_sheet(renderer, png.width, png.height,
            png.pixels, NULL, png.palette, 1, sheet);
        if (!ok) {
            fprintf(stderr, "rom2: cannot use %s: %s\n", path,
                SDL_GetError());
        }
    }
    if (ok) {
        *width = png.width;
        *height = png.height;
    }
    png_free(&png);
    return ok;
}

bool sheets_read_definition(const char *path, TomlTable **top)
{
    uint8_t *data = NULL;
    size_t size = 0;
    if (!file_read_all(path, &data, &size)) {
        fprintf(stderr, "rom2: cannot read %s\n", path);
        return false;
    }
    size_t line = 0;
    bool ok = toml_read(data, size, top, &line);
    free(data);
    if (!ok) {
        fprintf(stderr, "rom2: %s: line %zu is not understood\n", path,
            line);
    }
    return ok;
}

/* Writes the path of a file that lies beside a definition. */
static bool path_beside(const char *definition, const char *file,
                        char *out, size_t room)
{
    const char *slash = strrchr(definition, '/');
    int folder = slash == NULL ? 0 : (int)(slash - definition) + 1;
    int written = snprintf(out, room, "%.*s%s", folder, definition, file);
    if (written < 0 || (size_t)written >= room) {
        fprintf(stderr, "rom2: the path of %s is too long\n", definition);
        return false;
    }
    return true;
}

/* Puts a sheet on the card with the palettes its definition names, or
 * with the one of its own picture if it names none. */
static bool add_sheet(Renderer *renderer, const char *definition,
                      const SpriteSheet *layout, const Png *figure,
                      const uint8_t *colours, const uint8_t *solid,
                      uint16_t *sheet)
{
    char path[1024];
    Png rows;
    const Rgb *palettes = figure->palette;
    uint32_t count = 1;
    bool has_rows = layout->palettes[0] != '\0';
    if (has_rows) {
        if (!path_beside(definition, layout->palettes, path, sizeof path)
            || !read_png(path, &rows)) {
            return false;
        }
        /* Three bytes for a pixel, as an Rgb is. */
        _Static_assert(sizeof(Rgb) == 3, "an Rgb is three bytes");
        if (rows.kind != PNG_RGB || rows.width != 256) {
            fprintf(stderr, "rom2: %s is not a picture of palettes\n",
                path);
            png_free(&rows);
            return false;
        }
        palettes = (const Rgb *)rows.pixels;
        count = rows.height;
    }
    bool ok = renderer_add_sheet(renderer, figure->width, figure->height,
        colours, solid, palettes, count, sheet);
    if (!ok) {
        fprintf(stderr, "rom2: cannot use the sheet of %s: %s\n",
            definition, SDL_GetError());
    }
    if (has_rows) {
        png_free(&rows);
    }
    return ok;
}

bool sheets_load_sprites(Renderer *renderer, const char *definition,
                         const SpriteSheet *layout, uint16_t *sheet)
{
    char path[1024];
    Png figure;
    Png edges;
    if (!path_beside(definition, layout->file, path, sizeof path)
        || !read_png(path, &figure)) {
        return false;
    }
    bool has_edges = layout->edges[0] != '\0';
    if (has_edges && (!path_beside(definition, layout->edges, path,
                sizeof path) || !read_png(path, &edges))) {
        png_free(&figure);
        return false;
    }
    size_t count = (size_t)figure.width * figure.height;
    uint8_t *colours = malloc(count > 0 ? count * 2 : 1);
    uint8_t *solid = colours == NULL ? NULL : colours + count;
    bool ok = colours != NULL;
    if (!ok) {
        fprintf(stderr, "rom2: out of memory\n");
    } else if (!sprite_sheet_join(&figure, has_edges ? &edges : NULL,
            colours, solid)) {
        fprintf(stderr, "rom2: the pictures of %s do not go together\n",
            definition);
        ok = false;
    } else {
        ok = add_sheet(renderer, definition, layout, &figure, colours,
            solid, sheet);
    }
    free(colours);
    png_free(&figure);
    if (has_edges) {
        png_free(&edges);
    }
    return ok;
}
