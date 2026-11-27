/* rom2: the game. So far it opens an empty window.
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

/* The size of the plain window, which Alt+Enter and `--window` give.
 * On the whole screen the size is the monitor's, and these are not
 * used. */
#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768

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

/* Returns false when the player wants to leave. */
static bool handle_events(SDL_Window *window)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            return false;
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
        }
    }
    return true;
}

int main(int argc, char **argv)
{
    /* The real pixels of the screen, also where the system scales
     * windows up, so that a pixel of the game is a pixel of the
     * monitor. */
    SDL_WindowFlags whole_screen = SDL_WINDOW_FULLSCREEN
        | SDL_WINDOW_MOUSE_GRABBED;
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE
        | SDL_WINDOW_HIGH_PIXEL_DENSITY | whole_screen;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--window") == 0) {
            flags &= ~whole_screen;
        } else {
            fprintf(stderr, "usage: rom2 [--window]\n");
            return 2;
        }
    }
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "rom2: cannot start: %s\n", SDL_GetError());
        return 1;
    }
    if (!SDL_CreateWindowAndRenderer("ROM2", WINDOW_WIDTH, WINDOW_HEIGHT,
            flags, &window, &renderer)) {
        fprintf(stderr, "rom2: cannot open a window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    /* Without this a frame is drawn as often as the machine can. */
    SDL_SetRenderVSync(renderer, 1);

    bool ok = true;
    while (ok && handle_events(window)) {
        ok = SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255)
            && SDL_RenderClear(renderer)
            && SDL_RenderPresent(renderer);
    }
    if (!ok) {
        fprintf(stderr, "rom2: cannot draw: %s\n", SDL_GetError());
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return ok ? 0 : 1;
}
