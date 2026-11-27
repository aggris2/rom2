/* rom2: the game. So far it opens a window and draws one triangle.
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

/* Makes the pipeline that draws the triangle: its two shaders, and
 * what they draw into. The triangle is here to prove the way from a
 * shader's source to the screen, and goes when the game draws something
 * real. Returns NULL on failure. */
static SDL_GPUGraphicsPipeline *open_triangle(SDL_GPUDevice *device,
                                              SDL_Window *window)
{
    SDL_GPUShader *vertex = shader_open(device, &shader_triangle_vert,
        SDL_GPU_SHADERSTAGE_VERTEX);
    SDL_GPUShader *fragment = shader_open(device, &shader_triangle_frag,
        SDL_GPU_SHADERSTAGE_FRAGMENT);
    SDL_GPUGraphicsPipeline *pipeline = NULL;
    if (vertex != NULL && fragment != NULL) {
        SDL_GPUColorTargetDescription target;
        SDL_zero(target);
        target.format = SDL_GetGPUSwapchainTextureFormat(device, window);
        SDL_GPUGraphicsPipelineCreateInfo info;
        SDL_zero(info);
        info.vertex_shader = vertex;
        info.fragment_shader = fragment;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.target_info.color_target_descriptions = &target;
        info.target_info.num_color_targets = 1;
        pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    /* The pipeline keeps what it needs of the shaders. */
    if (vertex != NULL) {
        SDL_ReleaseGPUShader(device, vertex);
    }
    if (fragment != NULL) {
        SDL_ReleaseGPUShader(device, fragment);
    }
    return pipeline;
}

/* Draws one frame: so far a triangle on a black screen. */
static bool draw_frame(SDL_GPUDevice *device, SDL_Window *window,
                       SDL_GPUGraphicsPipeline *triangle)
{
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
    if (commands == NULL) {
        return false;
    }
    /* The picture the window shows next. There is none while the
     * window is hidden or has no size, and then nothing is drawn. */
    SDL_GPUTexture *screen = NULL;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, window, &screen,
            NULL, NULL)) {
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
            SDL_BindGPUGraphicsPipeline(pass, triangle);
            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            SDL_EndGPURenderPass(pass);
        }
    }
    return SDL_SubmitGPUCommandBuffer(commands);
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
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "rom2: cannot start: %s\n", SDL_GetError());
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
        return 1;
    }
    SDL_GPUDevice *device = open_device(window);
    if (device == NULL) {
        fprintf(stderr, "rom2: cannot use the graphics card: %s\n",
            SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GPUGraphicsPipeline *triangle = open_triangle(device, window);
    if (triangle == NULL) {
        fprintf(stderr, "rom2: cannot make the shaders: %s\n",
            SDL_GetError());
    }

    bool ok = triangle != NULL && SDL_ShowWindow(window);
    while (ok && handle_events(window)) {
        ok = draw_frame(device, window, triangle);
        if (!ok) {
            fprintf(stderr, "rom2: cannot draw: %s\n", SDL_GetError());
        }
    }
    if (triangle != NULL) {
        SDL_ReleaseGPUGraphicsPipeline(device, triangle);
    }
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return ok ? 0 : 1;
}
