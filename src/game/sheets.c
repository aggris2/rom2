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
            png.pixels, NULL, png.palette, sheet);
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

/* Reads the [sprite] part of a definition file. */
static bool read_layout(const char *definition, SpriteSheet *layout)
{
    uint8_t *data = NULL;
    size_t size = 0;
    if (!file_read_all(definition, &data, &size)) {
        fprintf(stderr, "rom2: cannot read %s\n", definition);
        return false;
    }
    TomlTable *top = NULL;
    size_t line = 0;
    bool ok = toml_read(data, size, &top, &line);
    free(data);
    if (!ok) {
        fprintf(stderr, "rom2: %s: line %zu is not understood\n",
            definition, line);
        return false;
    }
    ok = sprite_sheet_read(top, layout);
    toml_free(top);
    if (!ok) {
        fprintf(stderr, "rom2: %s has no usable [sprite] part\n",
            definition);
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

bool sheets_load_sprites(Renderer *renderer, const char *definition,
                         SpriteSheet *layout, uint16_t *sheet)
{
    SpriteSheet read;
    char path[1024];
    Png figure;
    Png edges;
    if (!read_layout(definition, &read)
        || !path_beside(definition, read.file, path, sizeof path)
        || !read_png(path, &figure)) {
        return false;
    }
    bool has_edges = read.edges[0] != '\0';
    if (has_edges && (!path_beside(definition, read.edges, path,
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
    } else if (!renderer_add_sheet(renderer, figure.width, figure.height,
            colours, solid, figure.palette, sheet)) {
        fprintf(stderr, "rom2: cannot use the sheet of %s: %s\n",
            definition, SDL_GetError());
        ok = false;
    }
    free(colours);
    png_free(&figure);
    if (has_edges) {
        png_free(&edges);
    }
    if (ok) {
        *layout = read;
    }
    return ok;
}
