#include "game/shader.h"

SDL_GPUShader *shader_open(SDL_GPUDevice *device, const ShaderData *data,
                           SDL_GPUShaderStage stage)
{
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
    SDL_GPUShaderCreateInfo info;
    SDL_zero(info);
    info.stage = stage;
    if ((formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0) {
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code = data->spirv;
        info.code_size = data->spirv_size;
        info.entrypoint = "main";
    } else if ((formats & SDL_GPU_SHADERFORMAT_MSL) != 0) {
        /* The tool that writes MSL names the start of a shader so. */
        info.format = SDL_GPU_SHADERFORMAT_MSL;
        info.code = data->msl;
        info.code_size = data->msl_size;
        info.entrypoint = "main0";
    } else {
        SDL_SetError("the graphics card takes no shader the game has");
        return NULL;
    }
    return SDL_CreateGPUShader(device, &info);
}
