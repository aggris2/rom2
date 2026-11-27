/* rom2: the game. So far it shows one thing of the content, a frame at
 * a time: a unit, an object or a structure, named by its definition
 * file. The left and right arrow keys go through its frames, and a
 * number after the file says which to start at.
 *
 * The window covers the whole screen at the screen's own size, without
 * changing the mode of the monitor, and the mouse stays inside it.
 * Alt+Enter switches between that and a plain window, and `--window`
 * starts in one. Escape or the window's own button closes it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "base/file.h"
#include "base/png.h"
#include "base/toml.h"
#include "data/sprite_sheet.h"
#include "game/renderer.h"

/* The size of the plain window, which Alt+Enter and `--window` give.
 * On the whole screen the size is the monitor's, and these are not
 * used. */
#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768

/* The thing that is shown: how its sheet is cut, and the sheet. */
typedef struct Thing {
    SpriteSheet layout;
    Png sheet;
} Thing;

/* Reads a definition and the sheet it names, which lies beside it.
 * Returns false, with a message printed, if it cannot. */
static bool thing_read(const char *path, Thing *thing)
{
    uint8_t *data = NULL;
    size_t size = 0;
    if (!file_read_all(path, &data, &size)) {
        fprintf(stderr, "rom2: cannot read %s\n", path);
        return false;
    }
    TomlTable *top = NULL;
    size_t line = 0;
    bool ok = toml_read(data, size, &top, &line);
    free(data);
    if (!ok) {
        fprintf(stderr, "rom2: %s: line %zu is not understood\n", path,
            line);
        return false;
    }
    ok = sprite_sheet_read(top, &thing->layout);
    toml_free(top);
    if (!ok) {
        fprintf(stderr, "rom2: %s has no usable [sprite] part\n", path);
        return false;
    }

    /* The sheet's path: the definition's, with its file name changed. */
    const char *slash = strrchr(path, '/');
    size_t folder = slash == NULL ? 0 : (size_t)(slash - path) + 1;
    char sheet_path[1024];
    int length = snprintf(sheet_path, sizeof sheet_path, "%.*s%s",
        (int)folder, path, thing->layout.file);
    if (length < 0 || (size_t)length >= sizeof sheet_path) {
        fprintf(stderr, "rom2: the path of %s is too long\n", path);
        return false;
    }
    if (!file_read_all(sheet_path, &data, &size)) {
        fprintf(stderr, "rom2: cannot read %s\n", sheet_path);
        return false;
    }
    ok = png_read(data, size, &thing->sheet);
    free(data);
    if (!ok) {
        fprintf(stderr, "rom2: %s is not a PNG picture\n", sheet_path);
    }
    return ok;
}

/* How many whole cells the sheet has. */
static uint32_t thing_frames(const Thing *thing)
{
    return (thing->sheet.width / thing->layout.cell_width)
        * (thing->sheet.height / thing->layout.cell_height);
}

/* The record that draws one frame with its anchor at a point of the
 * screen. */
static Sprite thing_sprite(const Thing *thing, uint16_t sheet,
                           uint32_t frame, int x, int y)
{
    uint32_t columns = thing->sheet.width / thing->layout.cell_width;
    Sprite sprite;
    sprite.sheet = sheet;
    sprite.source_x
        = (uint16_t)(frame % columns * thing->layout.cell_width);
    sprite.source_y
        = (uint16_t)(frame / columns * thing->layout.cell_height);
    sprite.width = thing->layout.cell_width;
    sprite.height = thing->layout.cell_height;
    sprite.x = (int16_t)(x - thing->layout.anchor_x);
    sprite.y = (int16_t)(y - thing->layout.anchor_y);
    return sprite;
}

/* Switches between the whole screen and a plain window. On the whole
 * screen the mouse is kept inside the game, so that it cannot slip onto
 * a second monitor; in a plain window it is free. SDL lets it go by
 * itself while another program is in front. */
static void toggle_fullscreen(SDL_Window *window)
{
    bool fullscreen
        = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
    if (!SDL_SetWindowFullscreen(window, !fullscreen)
        || !SDL_SetWindowMouseGrab(window, !fullscreen)) {
        fprintf(stderr, "rom2: cannot switch the window: %s\n",
            SDL_GetError());
    }
}

/* Takes what the player did since the last frame. `*step` gets how
 * many frames to go on, or back if it is below 0. Returns false when
 * the player wants to leave. */
static bool handle_events(SDL_Window *window, int *step)
{
    SDL_Event event;
    *step = 0;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            return false;
        }
        if (event.type != SDL_EVENT_KEY_DOWN) {
            continue;
        }
        /* A key held down goes on stepping through the frames. */
        if (event.key.key == SDLK_RIGHT) {
            *step += 1;
        } else if (event.key.key == SDLK_LEFT) {
            *step -= 1;
        }
        if (event.key.repeat) {
            continue;
        }
        if (event.key.key == SDLK_ESCAPE) {
            return false;
        }
        if (event.key.key == SDLK_RETURN
            && (event.key.mod & SDL_KMOD_ALT) != 0) {
            toggle_fullscreen(window);
        }
    }
    return true;
}

/* Shows the thing in a window, from the frame `frame` on, until the
 * player leaves. */
static bool show(SDL_Window *window, const Thing *thing, uint32_t frame)
{
    Renderer *renderer = renderer_open(window);
    uint16_t sheet = 0;
    if (renderer == NULL
        || !renderer_add_sheet(renderer, &thing->sheet, &sheet)) {
        fprintf(stderr, "rom2: cannot use the graphics card: %s\n",
            SDL_GetError());
        renderer_close(renderer);
        return false;
    }
    /* The window was hidden until there is something to show in it.
     * Opening the graphics card takes a moment, and a window shown
     * before that holds whatever was on the screen behind it. */
    bool ok = SDL_ShowWindow(window);
    uint32_t frames = thing_frames(thing);
    int step = 0;
    while (ok && handle_events(window, &step)) {
        /* Round and round: after the last frame comes the first. */
        int64_t next = ((int64_t)frame + step) % (int64_t)frames;
        frame = (uint32_t)(next < 0 ? next + frames : next);
        int width = 0;
        int height = 0;
        renderer_size(renderer, &width, &height);
        Sprite sprite = thing_sprite(thing, sheet, frame, width / 2,
            height / 2);
        ok = renderer_draw(renderer, &sprite, 1);
        if (!ok) {
            fprintf(stderr, "rom2: cannot draw: %s\n", SDL_GetError());
        }
    }
    renderer_close(renderer);
    return ok;
}

int main(int argc, char **argv)
{
    /* The real pixels of the screen, also where the system scales
     * windows up, so that a pixel of the game is a pixel of the
     * monitor. */
    SDL_WindowFlags whole_screen = SDL_WINDOW_FULLSCREEN
        | SDL_WINDOW_MOUSE_GRABBED;
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN
        | SDL_WINDOW_HIGH_PIXEL_DENSITY | whole_screen;
    const char *path = NULL;
    const char *first = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--window") == 0) {
            flags &= ~whole_screen;
        } else if (path == NULL) {
            path = argv[i];
        } else if (first == NULL) {
            first = argv[i];
        } else {
            path = NULL;
            break;
        }
    }
    /* The frame to start at is digits and nothing else. */
    char *end = NULL;
    unsigned long frame = first == NULL ? 0 : strtoul(first, &end, 10);
    if (path == NULL || (first != NULL
            && (*first < '0' || *first > '9' || *end != '\0'))) {
        fprintf(stderr,
            "usage: rom2 <definition.toml> [frame] [--window]\n");
        return 2;
    }
    Thing thing;
    if (!thing_read(path, &thing)) {
        return 1;
    }
    if (frame >= thing_frames(&thing)) {
        fprintf(stderr, "rom2: %s has %u frames, counted from 0\n", path,
            (unsigned)thing_frames(&thing));
        png_free(&thing.sheet);
        return 1;
    }
    bool ok = SDL_Init(SDL_INIT_VIDEO);
    if (!ok) {
        fprintf(stderr, "rom2: cannot start: %s\n", SDL_GetError());
    }
    SDL_Window *window = !ok ? NULL
        : SDL_CreateWindow("ROM2", WINDOW_WIDTH, WINDOW_HEIGHT, flags);
    if (ok && window == NULL) {
        fprintf(stderr, "rom2: cannot open a window: %s\n", SDL_GetError());
    }
    ok = window != NULL && show(window, &thing, (uint32_t)frame);
    if (window != NULL) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
    png_free(&thing.sheet);
    return ok ? 0 : 1;
}
