#include "game/picture.h"

#include <string.h>

#include "game/shader.h"

/* What the vertex shader is told: as `Place` in picture.vert. */
typedef struct Place {
    float screen_width;
    float screen_height;
    float width;
    float height;
} Place;

/* Makes a texture of `width` by `height` and fills it with `pixels`,
 * which has `pixel_size` bytes for each pixel. Returns NULL on
 * failure. */
static SDL_GPUTexture *make_texture(SDL_GPUDevice *device,
                                    SDL_GPUTextureFormat format,
                                    uint32_t width, uint32_t height,
                                    const uint8_t *pixels,
                                    uint32_t pixel_size)
{
    uint32_t size = width * height * pixel_size;
    SDL_GPUTextureCreateInfo texture_info;
    SDL_zero(texture_info);
    texture_info.type = SDL_GPU_TEXTURETYPE_2D;
    texture_info.format = format;
    texture_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    texture_info.width = width;
    texture_info.height = height;
    texture_info.layer_count_or_depth = 1;
    texture_info.num_levels = 1;
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &texture_info);

    /* Pixels reach the card by way of a buffer made for the trip. */
    SDL_GPUTransferBufferCreateInfo buffer_info;
    SDL_zero(buffer_info);
    buffer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    buffer_info.size = size;
    SDL_GPUTransferBuffer *buffer = texture == NULL ? NULL
        : SDL_CreateGPUTransferBuffer(device, &buffer_info);
    void *room = buffer == NULL ? NULL
        : SDL_MapGPUTransferBuffer(device, buffer, false);
    SDL_GPUCommandBuffer *commands = room == NULL ? NULL
        : SDL_AcquireGPUCommandBuffer(device);
    bool ok = false;
    if (room != NULL) {
        memcpy(room, pixels, size);
        SDL_UnmapGPUTransferBuffer(device, buffer);
    }
    if (commands != NULL) {
        SDL_GPUTextureTransferInfo from;
        SDL_GPUTextureRegion to;
        SDL_zero(from);
        SDL_zero(to);
        from.transfer_buffer = buffer;
        to.texture = texture;
        to.w = width;
        to.h = height;
        to.d = 1;
        SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(commands);
        SDL_UploadToGPUTexture(pass, &from, &to, false);
        SDL_EndGPUCopyPass(pass);
        ok = SDL_SubmitGPUCommandBuffer(commands);
    }
    if (buffer != NULL) {
        SDL_ReleaseGPUTransferBuffer(device, buffer);
    }
    if (!ok && texture != NULL) {
        SDL_ReleaseGPUTexture(device, texture);
        texture = NULL;
    }
    return texture;
}

/* Makes the pipeline: the two shaders, and how what they draw is laid
 * over what is there, by how solid each pixel is. */
static SDL_GPUGraphicsPipeline *make_pipeline(SDL_GPUDevice *device,
                                              SDL_Window *window)
{
    SDL_GPUShader *vertex = shader_open(device, &shader_picture_vert,
        SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
    SDL_GPUShader *fragment = shader_open(device, &shader_picture_frag,
        SDL_GPU_SHADERSTAGE_FRAGMENT, 2, 0);
    SDL_GPUGraphicsPipeline *pipeline = NULL;
    if (vertex != NULL && fragment != NULL) {
        SDL_GPUColorTargetDescription target;
        SDL_zero(target);
        target.format = SDL_GetGPUSwapchainTextureFormat(device, window);
        target.blend_state.enable_blend = true;
        target.blend_state.src_color_blendfactor
            = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        target.blend_state.dst_color_blendfactor
            = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        target.blend_state.dst_alpha_blendfactor
            = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        SDL_GPUGraphicsPipelineCreateInfo info;
        SDL_zero(info);
        info.vertex_shader = vertex;
        info.fragment_shader = fragment;
        /* Four corners, each new one making a triangle with the two
         * before it. */
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
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

bool picture_open(Picture *picture, SDL_GPUDevice *device,
                  SDL_Window *window, const Png *png)
{
    if (png->kind != PNG_INDEXED) {
        SDL_SetError("the picture has no palette");
        return false;
    }
    /* The palette as the card takes it: red, green, blue, and how solid
     * the colour is. */
    uint8_t colours[256 * 4];
    for (int i = 0; i < 256; i++) {
        colours[i * 4] = png->palette[i].r;
        colours[i * 4 + 1] = png->palette[i].g;
        colours[i * 4 + 2] = png->palette[i].b;
        colours[i * 4 + 3] = png->alpha[i];
    }
    /* Every pixel is taken as it is, never mixed with its neighbours:
     * a number between two numbers of a palette means nothing. */
    SDL_GPUSamplerCreateInfo sampler_info;
    SDL_zero(sampler_info);
    sampler_info.min_filter = SDL_GPU_FILTER_NEAREST;
    sampler_info.mag_filter = SDL_GPU_FILTER_NEAREST;
    sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

    Picture made;
    memset(&made, 0, sizeof made);
    made.width = png->width;
    made.height = png->height;
    made.indices = make_texture(device, SDL_GPU_TEXTUREFORMAT_R8_UNORM,
        png->width, png->height, png->pixels, 1);
    made.palette = make_texture(device,
        SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 256, 1, colours, 4);
    made.sampler = SDL_CreateGPUSampler(device, &sampler_info);
    made.pipeline = make_pipeline(device, window);
    if (made.indices == NULL || made.palette == NULL
        || made.sampler == NULL || made.pipeline == NULL) {
        picture_close(&made, device);
        return false;
    }
    *picture = made;
    return true;
}

void picture_draw(const Picture *picture, SDL_GPUCommandBuffer *commands,
                  SDL_GPURenderPass *pass, uint32_t screen_width,
                  uint32_t screen_height)
{
    Place place;
    place.screen_width = (float)screen_width;
    place.screen_height = (float)screen_height;
    place.width = (float)picture->width;
    place.height = (float)picture->height;
    SDL_GPUTextureSamplerBinding textures[2];
    textures[0].texture = picture->indices;
    textures[0].sampler = picture->sampler;
    textures[1].texture = picture->palette;
    textures[1].sampler = picture->sampler;
    SDL_BindGPUGraphicsPipeline(pass, picture->pipeline);
    SDL_PushGPUVertexUniformData(commands, 0, &place, sizeof place);
    SDL_BindGPUFragmentSamplers(pass, 0, textures, 2);
    SDL_DrawGPUPrimitives(pass, 4, 1, 0, 0);
}

void picture_close(Picture *picture, SDL_GPUDevice *device)
{
    if (picture->pipeline != NULL) {
        SDL_ReleaseGPUGraphicsPipeline(device, picture->pipeline);
    }
    if (picture->sampler != NULL) {
        SDL_ReleaseGPUSampler(device, picture->sampler);
    }
    if (picture->palette != NULL) {
        SDL_ReleaseGPUTexture(device, picture->palette);
    }
    if (picture->indices != NULL) {
        SDL_ReleaseGPUTexture(device, picture->indices);
    }
    memset(picture, 0, sizeof *picture);
}
