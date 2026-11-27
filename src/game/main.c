/* rom2: the game. So far it opens a window and shows one picture of the
 * content in it: a sprite sheet or the ground tiles, any PNG with a
 * palette.
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
#include "game/picture.h"
#include "game/shader.h"

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

/* Opens the graphics card for the window, for the forms of shader the
 * game has. SDL picks the way of drawing: Vulkan on Linux and Windows,
 * Metal on macOS.
 *
 * A new frame is shown when the monitor is ready for one, so that no
 * more frames are drawn than can be seen. Returns NULL on failure. */
static SDL_GPUDevice *open_device(SDL_Window *window)
{
    SDL_GPUDevice *device = SDL_CreateGPUDevice(SHADER_FORMATS, false,
        NULL);
    if (device == NULL) {
        return NULL;
    }
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_DestroyGPUDevice(device);
        return NULL;
    }
    if (!SDL_SetGPUSwapchainParameters(device, window,
            SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC)) {
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyGPUDevice(device);
        return NULL;
    }
    return device;
}

/* Draws one frame: so far a picture on a black screen. */
static bool draw_frame(SDL_GPUDevice *device, SDL_Window *window,
                       const Picture *picture)
{
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
    if (commands == NULL) {
        return false;
    }
    /* The picture the window shows next. There is none while the
     * window is hidden or has no size, and then nothing is drawn. */
    SDL_GPUTexture *screen = NULL;
    Uint32 width = 0;
    Uint32 height = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, window, &screen,
            &width, &height)) {
        SDL_CancelGPUCommandBuffer(commands);
        return false;
    }
    if (screen != NULL) {
        SDL_GPUColorTargetInfo target;
        SDL_zero(target);
        target.texture = screen;
        target.clear_color.a = 1.0f;
        target.load_op = SDL_GPU_LOADOP_CLEAR;
        target.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, &target,
            1, NULL);
        if (pass != NULL) {
            picture_draw(picture, commands, pass, width, height);
            SDL_EndGPURenderPass(pass);
        }
    }
    return SDL_SubmitGPUCommandBuffer(commands);
}

/* Reads a PNG file. Returns false, with a message printed, if it
 * cannot. */
static bool read_picture(const char *path, Png *png)
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

int main(int argc, char **argv)
{
    /* The real pixels of the screen, also where the system scales
     * windows up, so that a pixel of the game is a pixel of the
     * monitor. */
    SDL_WindowFlags whole_screen = SDL_WINDOW_FULLSCREEN
        | SDL_WINDOW_MOUSE_GRABBED;
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE
        | SDL_WINDOW_HIGH_PIXEL_DENSITY | whole_screen;
    const char *path = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--window") == 0) {
            flags &= ~whole_screen;
        } else if (path == NULL) {
            path = argv[i];
        } else {
            path = NULL;
            break;
        }
    }
    if (path == NULL) {
        fprintf(stderr, "usage: rom2 <picture.png> [--window]\n");
        return 2;
    }
    Png png;
    if (!read_picture(path, &png)) {
        return 1;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "rom2: cannot start: %s\n", SDL_GetError());
        png_free(&png);
        return 1;
    }
    /* The window stays hidden until there is something to show in it.
     * Opening the graphics card takes a moment, and a window shown
     * before that holds whatever was on the screen behind it. */
    SDL_Window *window = SDL_CreateWindow("ROM2", WINDOW_WIDTH,
        WINDOW_HEIGHT, flags | SDL_WINDOW_HIDDEN);
    if (window == NULL) {
        fprintf(stderr, "rom2: cannot open a window: %s\n", SDL_GetError());
        SDL_Quit();
        png_free(&png);
        return 1;
    }
    SDL_GPUDevice *device = open_device(window);
    if (device == NULL) {
        fprintf(stderr, "rom2: cannot use the graphics card: %s\n",
            SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        png_free(&png);
        return 1;
    }
    /* Once the picture is on the card, the copy in memory is not
     * needed. */
    Picture picture;
    memset(&picture, 0, sizeof picture);
    bool ok = picture_open(&picture, device, window, &png);
    png_free(&png);
    if (!ok) {
        fprintf(stderr, "rom2: cannot show %s: %s\n", path, SDL_GetError());
    }

    ok = ok && SDL_ShowWindow(window);
    while (ok && handle_events(window)) {
        ok = draw_frame(device, window, &picture);
        if (!ok) {
            fprintf(stderr, "rom2: cannot draw: %s\n", SDL_GetError());
        }
    }
    picture_close(&picture, device);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return ok ? 0 : 1;
}
