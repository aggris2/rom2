/* Handing a compiled shader to the graphics card.
 *
 * A shader is a small program the card runs: a vertex shader says where
 * each corner goes, and a fragment shader what colour each pixel gets.
 * They are written in src/game/shaders, where build.sh compiles them
 * into data.c.
 */
#ifndef ROM2_GAME_SHADER_H
#define ROM2_GAME_SHADER_H

#include <SDL3/SDL.h>

#include "game/shaders/data.h"

/* The forms of shader the game has. A device is opened for these. */
#define SHADER_FORMATS (SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL)

/* Gives the card the form of the shader it takes. `textures` and
 * `blocks` are how many pictures the shader reads and how many blocks
 * of values it is told: the card has to know. Release the shader with
 * SDL_ReleaseGPUShader(). Returns NULL, with SDL's error set, if the
 * card takes neither form or refuses the shader. */
SDL_GPUShader *shader_open(SDL_GPUDevice *device, const ShaderData *data,
                           SDL_GPUShaderStage stage, uint32_t textures,
                           uint32_t blocks);

#endif
