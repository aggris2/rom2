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

bool sheets_load_cursor(Renderer *renderer, const char *definition,
                        const Cursor *cursor, uint16_t *sheet)
{
    char path[1024];
    Png colours;
    Png solid;
    if (!path_beside(definition, cursor->sheet, path, sizeof path)
        || !read_png(path, &colours)) {
        return false;
    }
    if (!path_beside(definition, cursor->alpha, path, sizeof path)
        || !read_png(path, &solid)) {
        png_free(&colours);
        return false;
    }
    bool ok = colours.kind == PNG_INDEXED && solid.kind == PNG_GREY
        && colours.width == solid.width && colours.height == solid.height;
    if (!ok) {
        fprintf(stderr, "rom2: the pictures of %s do not go together\n",
            definition);
    } else if (!renderer_add_sheet(renderer, colours.width, colours.height,
            colours.pixels, solid.pixels, colours.palette, 1, sheet)) {
        fprintf(stderr, "rom2: cannot use the sheet of %s: %s\n",
            definition, SDL_GetError());
        ok = false;
    }
    png_free(&colours);
    png_free(&solid);
    return ok;
}

bool sheets_load_font(Renderer *renderer, const char *definition,
                      const Font *font, Rgb colour, uint16_t *sheet)
{
    char path[1024];
    Png solid;
    if (font->coloured) {
        fprintf(stderr, "rom2: %s is a font in colours\n", definition);
        return false;
    }
    if (!path_beside(definition, font->glyphs, path, sizeof path)
        || !read_png(path, &solid)) {
        return false;
    }
    /* The picture says how solid each pixel is. Every pixel is of the
     * one colour, the second of a palette of two. */
    size_t count = (size_t)solid.width * solid.height;
    uint8_t *colours = malloc(count > 0 ? count : 1);
    Rgb palette[256];
    memset(palette, 0, sizeof palette);
    palette[1] = colour;
    bool ok = solid.kind == PNG_GREY && colours != NULL;
    if (!ok) {
        fprintf(stderr, "rom2: cannot use the picture of %s\n", definition);
    } else {
        memset(colours, 1, count);
        ok = renderer_add_sheet(renderer, solid.width, solid.height,
            colours, solid.pixels, palette, 1, sheet);
        if (!ok) {
            fprintf(stderr, "rom2: cannot use the sheet of %s: %s\n",
                definition, SDL_GetError());
        }
    }
    free(colours);
    png_free(&solid);
    return ok;
}

bool sheets_load_bars(Renderer *renderer, const char *ball,
                      uint16_t *sheet)
{
    enum { WIDTH = BARS_SHEET_WIDTH, HEIGHT = BARS_SHEET_HEIGHT };
    static uint8_t colours[WIDTH * HEIGHT];
    static uint8_t solid[WIDTH * HEIGHT];
    Rgb palette[256];
    Png png;
    if (!read_png(ball, &png)) {
        return false;
    }
    if (png.kind != PNG_RGB || png.width != BARS_BALL
        || png.height != BARS_BALL) {
        fprintf(stderr, "rom2: %s is not the ball of the bars\n", ball);
        png_free(&png);
        return false;
    }
    memset(colours, 0, sizeof colours);
    memset(solid, 0, sizeof solid);
    memset(palette, 0, sizeof palette);
    /* A line has its place in the palette, and so has each pixel of
     * the ball after them. */
    for (int line = 0; line < BAR_LINES; line++) {
        palette[line] = bars_colour((BarLine)line);
        memset(colours + line * WIDTH, line, WIDTH);
        memset(solid + line * WIDTH, SPRITE_SHEET_SOLID, WIDTH);
        memset(colours + (BAR_LINES + line) * WIDTH, line, WIDTH);
        memset(solid + (BAR_LINES + line) * WIDTH, SPRITE_SHEET_EDGE,
            WIDTH);
    }
    for (int i = 0; i < BARS_BALL * BARS_BALL; i++) {
        Rgb colour = {png.pixels[i * 3], png.pixels[i * 3 + 1],
            png.pixels[i * 3 + 2]};
        size_t at = (size_t)(2 * BAR_LINES + i / BARS_BALL) * WIDTH
            + (size_t)(i % BARS_BALL);
        palette[BAR_LINES + i] = colour;
        colours[at] = (uint8_t)(BAR_LINES + i);
        solid[at] = colour.r == 0 && colour.g == 0 && colour.b == 0
            ? 0 : SPRITE_SHEET_SOLID;
    }
    png_free(&png);
    if (!renderer_add_sheet(renderer, WIDTH, HEIGHT, colours, solid,
            palette, 1, sheet)) {
        fprintf(stderr, "rom2: cannot use the sheet of the bars: %s\n",
            SDL_GetError());
        return false;
    }
    return true;
}
