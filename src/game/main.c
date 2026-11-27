/* rom2: the game. So far it opens an empty window, which Escape or the
 * window's own button closes.
 */
#include <stdio.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768

/* Returns false when the player wants to leave. */
static bool handle_events(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            return false;
        }
        if (event.type == SDL_EVENT_KEY_DOWN
            && event.key.key == SDLK_ESCAPE) {
            return false;
        }
    }
    return true;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "rom2: cannot start: %s\n", SDL_GetError());
        return 1;
    }
    if (!SDL_CreateWindowAndRenderer("ROM2", WINDOW_WIDTH, WINDOW_HEIGHT,
            SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        fprintf(stderr, "rom2: cannot open a window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    /* Without this a frame is drawn as often as the machine can. */
    SDL_SetRenderVSync(renderer, 1);

    bool ok = true;
    while (ok && handle_events()) {
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
