/* A picture with a palette, on the graphics card, and drawing it.
 *
 * The card holds the picture as it is in its file: a number for each
 * pixel, and the 256 colours the numbers stand for. A shader looks the
 * colour up as it draws.
 */
#ifndef ROM2_GAME_PICTURE_H
#define ROM2_GAME_PICTURE_H

#include <stdbool.h>
#include <stdint.h>

#include <SDL3/SDL.h>

#include "base/png.h"

typedef struct Picture {
    SDL_GPUTexture *indices;    /* a number for each pixel */
    SDL_GPUTexture *palette;    /* 256 colours in a row */
    SDL_GPUSampler *sampler;
    SDL_GPUGraphicsPipeline *pipeline;
    uint32_t width;
    uint32_t height;
} Picture;

/* Puts a picture on the card, to be drawn into `window`. Returns false,
 * with SDL's error set and nothing to close, if the picture has no
 * palette or the card refuses any part of it. */
bool picture_open(Picture *picture, SDL_GPUDevice *device,
                  SDL_Window *window, const Png *png);

/* Draws the picture at its own size in the top left corner of a screen
 * of `screen_width` by `screen_height` pixels. `commands` is the list
 * the pass belongs to. */
void picture_draw(const Picture *picture, SDL_GPUCommandBuffer *commands,
                  SDL_GPURenderPass *pass, uint32_t screen_width,
                  uint32_t screen_height);

/* Takes the picture off the card and sets it to zero. */
void picture_close(Picture *picture, SDL_GPUDevice *device);

#endif
