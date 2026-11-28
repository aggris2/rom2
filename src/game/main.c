/* rom2: the game. So far it opens a map and shows its ground.
 *
 * This file is the window and the loop. What is in the window is the
 * map screen's.
 *
 * The window covers the whole screen at the screen's own size, without
 * changing the mode of the monitor, and the mouse stays inside it.
 * Alt+Enter switches between that and a plain window, and `--window`
 * starts in one. Escape or the window's own button closes it.
 */
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "game/map_screen.h"
#include "game/renderer.h"

/* The size of the plain window, which Alt+Enter and `--window` give.
 * On the whole screen the size is the monitor's, and these are not
 * used. */
#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768

/* The longest a frame is taken to have lasted, in seconds. After a
 * pause, such as the window being dragged, the game goes on from where
 * it was and does not jump. */
#define LONGEST_FRAME 0.1f

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

/* Takes what happened since the last frame. A key that is not the
 * window's goes to the map, and so does a button of the mouse.
 * Returns false when the player wants to leave. */
static bool handle_events(SDL_Window *window, MapScreen *screen)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            return false;
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            map_screen_button(screen, event.button.button);
        }
        if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) {
            continue;
        }
        if (event.key.key == SDLK_ESCAPE) {
            return false;
        }
        if (event.key.key == SDLK_RETURN
            && (event.key.mod & SDL_KMOD_ALT) != 0) {
            toggle_fullscreen(window);
        } else {
            map_screen_key(screen, event.key.key);
        }
    }
    return true;
}

/* How long the frame that just passed is taken to have lasted, given
 * what the clock measured.
 *
 * A frame is shown when the monitor is ready for one, so in truth it
 * lasted a whole number of the monitor's own frames. The clock says a
 * little more or less each time, and movement worked out from that is
 * uneven: 5 pixels, then 6, then 5. So the time is rounded to whole
 * frames of the monitor the window is on. If the monitor's rate is not
 * known, the clock is taken as it is. */
static float frame_seconds(SDL_Window *window, float measured)
{
    const SDL_DisplayMode *mode
        = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window));
    if (mode == NULL || mode->refresh_rate <= 0.0f) {
        return measured > LONGEST_FRAME ? LONGEST_FRAME : measured;
    }
    float one = 1.0f / mode->refresh_rate;
    float frames = SDL_roundf(measured / one);
    float most = SDL_floorf(LONGEST_FRAME / one);
    frames = frames < 1.0f ? 1.0f : frames > most ? most : frames;
    return frames * one;
}

/* Runs the game in a window until the player leaves. */
static bool run(SDL_Window *window, const char *content, const char *map)
{
    Renderer *renderer = renderer_open(window);
    if (renderer == NULL) {
        fprintf(stderr, "rom2: cannot use the graphics card: %s\n",
            SDL_GetError());
        return false;
    }
    MapScreen *screen = map_screen_open(renderer, content, map);
    /* The window was hidden until there is something to show in it.
     * Opening the graphics card takes a moment, and a window shown
     * before that holds whatever was on the screen behind it. */
    bool ok = screen != NULL && SDL_ShowWindow(window);
    Uint64 before = SDL_GetTicksNS();
    while (ok && handle_events(window, screen)) {
        Uint64 now = SDL_GetTicksNS();
        float measured = (float)(now - before) / 1e9f;
        before = now;
        ok = map_screen_frame(screen, frame_seconds(window, measured));
    }
    map_screen_close(screen);
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
    const char *content = "content";
    const char *map = NULL;
    bool understood = true;
    for (int i = 1; i < argc && understood; i++) {
        if (strcmp(argv[i], "--window") == 0) {
            flags &= ~whole_screen;
        } else if (strcmp(argv[i], "--content") == 0 && i + 1 < argc) {
            content = argv[++i];
        } else if (argv[i][0] != '-' && map == NULL) {
            map = argv[i];
        } else {
            understood = false;
        }
    }
    if (!understood) {
        fprintf(stderr,
            "usage: rom2 [mission number | map file] [--content <folder>]"
            " [--window]\n"
            "\n"
            "Shows a map: mission 10 if none is named. The content is\n"
            "taken from the folder `content` if no other is named. The\n"
            "mouse on the edge of the screen moves over the map and a\n"
            "click selects a unit, which the right button lets go. B hides\n"
            "the health of the units that are not selected, W marks the\n"
            "squares closed to what walks, then hovers, then flies, G shows\n"
            "a grid, P pauses, F hides the count of frames a second, and\n"
            "Escape leaves.\n");
        return 2;
    }
    if (map == NULL) {
        map = "10";
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "rom2: cannot start: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *window = SDL_CreateWindow("ROM2", WINDOW_WIDTH,
        WINDOW_HEIGHT, flags);
    if (window == NULL) {
        fprintf(stderr, "rom2: cannot open a window: %s\n", SDL_GetError());
    }
    bool ok = window != NULL && run(window, content, map);
    if (window != NULL) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
    return ok ? 0 : 1;
}
