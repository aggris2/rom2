#include "game/renderer.h"

#include <stdlib.h>
#include <string.h>

#include "game/shader.h"

/* A picture with a palette on the card, as it is in its file: a number
 * for each pixel, and the 256 colours the numbers stand for. */
typedef struct Sheet {
    SDL_GPUTexture *indices;
    SDL_GPUTexture *palette;
} Sheet;

/* A corner of a sprite, as sprite.vert takes it. */
typedef struct Corner {
    float x;        /* on the screen */
    float y;
    float u;        /* in the sheet */
    float v;
} Corner;

/* A sprite is two triangles. */
#define CORNERS 6

struct Renderer {
    SDL_Window *window;
    SDL_GPUDevice *device;
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_GPUSampler *sampler;
    Sheet *sheets;
    size_t sheet_count;
    /* The corners of a frame's sprites: on the card, and the buffer
     * they travel there in. Both have room for `room` sprites. */
    SDL_GPUBuffer *corners;
    SDL_GPUTransferBuffer *transfer;
    size_t room;
};

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

/* Makes the pipeline: the two shaders, what a corner is made of, and
 * how what is drawn is laid over what is there, by how solid each pixel
 * is. */
static SDL_GPUGraphicsPipeline *make_pipeline(SDL_GPUDevice *device,
                                              SDL_Window *window)
{
    SDL_GPUShader *vertex = shader_open(device, &shader_sprite_vert,
        SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
    SDL_GPUShader *fragment = shader_open(device, &shader_sprite_frag,
        SDL_GPU_SHADERSTAGE_FRAGMENT, 2, 0);
    SDL_GPUGraphicsPipeline *pipeline = NULL;
    if (vertex != NULL && fragment != NULL) {
        SDL_GPUVertexBufferDescription buffer;
        SDL_zero(buffer);
        buffer.pitch = sizeof(Corner);
        buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        SDL_GPUVertexAttribute parts[2];
        SDL_zero(parts);
        parts[0].location = 0;
        parts[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        parts[0].offset = offsetof(Corner, x);
        parts[1].location = 1;
        parts[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        parts[1].offset = offsetof(Corner, u);

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
        info.vertex_input_state.vertex_buffer_descriptions = &buffer;
        info.vertex_input_state.num_vertex_buffers = 1;
        info.vertex_input_state.vertex_attributes = parts;
        info.vertex_input_state.num_vertex_attributes = 2;
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

Renderer *renderer_open(SDL_Window *window)
{
    Renderer *renderer = calloc(1, sizeof *renderer);
    if (renderer == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }
    renderer->window = window;
    /* SDL picks the way of drawing for the forms of shader the game
     * has: Vulkan on Linux and Windows, Metal on macOS. */
    renderer->device = SDL_CreateGPUDevice(SHADER_FORMATS, false, NULL);
    if (renderer->device == NULL) {
        free(renderer);
        return NULL;
    }
    if (!SDL_ClaimWindowForGPUDevice(renderer->device, window)) {
        SDL_DestroyGPUDevice(renderer->device);
        free(renderer);
        return NULL;
    }
    /* Every pixel of a sheet is taken as it is, never mixed with its
     * neighbours: a number between two numbers of a palette means
     * nothing. */
    SDL_GPUSamplerCreateInfo sampler_info;
    SDL_zero(sampler_info);
    sampler_info.min_filter = SDL_GPU_FILTER_NEAREST;
    sampler_info.mag_filter = SDL_GPU_FILTER_NEAREST;
    sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    renderer->sampler = SDL_CreateGPUSampler(renderer->device,
        &sampler_info);
    renderer->pipeline = make_pipeline(renderer->device, window);
    /* A new frame is shown when the monitor is ready for one, so that
     * no more frames are drawn than can be seen. */
    if (renderer->sampler == NULL || renderer->pipeline == NULL
        || !SDL_SetGPUSwapchainParameters(renderer->device, window,
            SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC)) {
        renderer_close(renderer);
        return NULL;
    }
    return renderer;
}

void renderer_close(Renderer *renderer)
{
    if (renderer == NULL) {
        return;
    }
    SDL_GPUDevice *device = renderer->device;
    for (size_t i = 0; i < renderer->sheet_count; i++) {
        SDL_ReleaseGPUTexture(device, renderer->sheets[i].indices);
        SDL_ReleaseGPUTexture(device, renderer->sheets[i].palette);
    }
    free(renderer->sheets);
    if (renderer->corners != NULL) {
        SDL_ReleaseGPUBuffer(device, renderer->corners);
    }
    if (renderer->transfer != NULL) {
        SDL_ReleaseGPUTransferBuffer(device, renderer->transfer);
    }
    if (renderer->pipeline != NULL) {
        SDL_ReleaseGPUGraphicsPipeline(device, renderer->pipeline);
    }
    if (renderer->sampler != NULL) {
        SDL_ReleaseGPUSampler(device, renderer->sampler);
    }
    SDL_ReleaseWindowFromGPUDevice(device, renderer->window);
    SDL_DestroyGPUDevice(device);
    free(renderer);
}

bool renderer_add_sheet(Renderer *renderer, const Png *png, uint16_t *out)
{
    if (png->kind != PNG_INDEXED) {
        return SDL_SetError("the picture has no palette");
    }
    if (renderer->sheet_count > UINT16_MAX) {
        return SDL_SetError("there are too many sheets");
    }
    Sheet *sheets = realloc(renderer->sheets,
        (renderer->sheet_count + 1) * sizeof *sheets);
    if (sheets == NULL) {
        return SDL_OutOfMemory();
    }
    renderer->sheets = sheets;
    /* The palette as the card takes it: red, green, blue, and how solid
     * the colour is. */
    uint8_t colours[256 * 4];
    for (int i = 0; i < 256; i++) {
        colours[i * 4] = png->palette[i].r;
        colours[i * 4 + 1] = png->palette[i].g;
        colours[i * 4 + 2] = png->palette[i].b;
        colours[i * 4 + 3] = png->alpha[i];
    }
    Sheet sheet;
    sheet.indices = make_texture(renderer->device,
        SDL_GPU_TEXTUREFORMAT_R8_UNORM, png->width, png->height,
        png->pixels, 1);
    sheet.palette = sheet.indices == NULL ? NULL
        : make_texture(renderer->device,
            SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 256, 1, colours, 4);
    if (sheet.palette == NULL) {
        if (sheet.indices != NULL) {
            SDL_ReleaseGPUTexture(renderer->device, sheet.indices);
        }
        return false;
    }
    *out = (uint16_t)renderer->sheet_count;
    renderer->sheets[renderer->sheet_count++] = sheet;
    return true;
}

void renderer_size(const Renderer *renderer, int *width, int *height)
{
    *width = 0;
    *height = 0;
    SDL_GetWindowSizeInPixels(renderer->window, width, height);
}

/* Makes room for the corners of `sprites` sprites. */
static bool make_room(Renderer *renderer, size_t sprites)
{
    if (sprites <= renderer->room) {
        return true;
    }
    /* Twice what is asked for, so that a frame with a few sprites more
     * than the last does not start again. */
    size_t room = sprites * 2;
    if (room > UINT32_MAX / (CORNERS * sizeof(Corner))) {
        return SDL_SetError("there are too many sprites");
    }
    SDL_GPUBufferCreateInfo buffer_info;
    SDL_zero(buffer_info);
    buffer_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    buffer_info.size = (Uint32)(room * CORNERS * sizeof(Corner));
    SDL_GPUTransferBufferCreateInfo transfer_info;
    SDL_zero(transfer_info);
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = buffer_info.size;
    SDL_GPUBuffer *corners = SDL_CreateGPUBuffer(renderer->device,
        &buffer_info);
    SDL_GPUTransferBuffer *transfer = corners == NULL ? NULL
        : SDL_CreateGPUTransferBuffer(renderer->device, &transfer_info);
    if (transfer == NULL) {
        if (corners != NULL) {
            SDL_ReleaseGPUBuffer(renderer->device, corners);
        }
        return false;
    }
    if (renderer->corners != NULL) {
        SDL_ReleaseGPUBuffer(renderer->device, renderer->corners);
        SDL_ReleaseGPUTransferBuffer(renderer->device, renderer->transfer);
    }
    renderer->corners = corners;
    renderer->transfer = transfer;
    renderer->room = room;
    return true;
}

/* Writes the six corners of a sprite. */
static void put_corners(Corner *out, const Sprite *sprite)
{
    float left = sprite->x;
    float top = sprite->y;
    float right = left + (float)sprite->width;
    float bottom = top + (float)sprite->height;
    float u = sprite->source_x;
    float v = sprite->source_y;
    float u_end = u + (float)sprite->width;
    float v_end = v + (float)sprite->height;
    Corner top_left = {left, top, u, v};
    Corner top_right = {right, top, u_end, v};
    Corner low_left = {left, bottom, u, v_end};
    Corner low_right = {right, bottom, u_end, v_end};
    out[0] = top_left;
    out[1] = top_right;
    out[2] = low_left;
    out[3] = top_right;
    out[4] = low_right;
    out[5] = low_left;
}

/* Sends the corners of the sprites to the card, as part of `commands`.
 * Returns how many sprites were sent, those with a sheet there is, and
 * writes at `kept` which sheet each of them has. */
static size_t send_corners(Renderer *renderer,
                           SDL_GPUCommandBuffer *commands,
                           const Sprite *sprites, size_t count,
                           uint16_t *kept)
{
    Corner *corners = SDL_MapGPUTransferBuffer(renderer->device,
        renderer->transfer, true);
    if (corners == NULL) {
        return 0;
    }
    size_t sent = 0;
    for (size_t i = 0; i < count; i++) {
        if (sprites[i].sheet < renderer->sheet_count) {
            put_corners(&corners[sent * CORNERS], &sprites[i]);
            kept[sent++] = sprites[i].sheet;
        }
    }
    SDL_UnmapGPUTransferBuffer(renderer->device, renderer->transfer);
    if (sent > 0) {
        SDL_GPUTransferBufferLocation from;
        SDL_GPUBufferRegion to;
        SDL_zero(from);
        SDL_zero(to);
        from.transfer_buffer = renderer->transfer;
        to.buffer = renderer->corners;
        to.size = (Uint32)(sent * CORNERS * sizeof(Corner));
        SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(commands);
        SDL_UploadToGPUBuffer(pass, &from, &to, true);
        SDL_EndGPUCopyPass(pass);
    }
    return sent;
}

/* Draws the sprites that were sent. Those that follow each other with
 * one sheet go to the card in one call. */
static void draw_sprites(const Renderer *renderer,
                         SDL_GPUCommandBuffer *commands,
                         SDL_GPURenderPass *pass, const uint16_t *kept,
                         size_t sent, Uint32 width, Uint32 height)
{
    float screen[2];
    screen[0] = (float)width;
    screen[1] = (float)height;
    SDL_GPUBufferBinding buffer;
    SDL_zero(buffer);
    buffer.buffer = renderer->corners;
    SDL_BindGPUGraphicsPipeline(pass, renderer->pipeline);
    SDL_PushGPUVertexUniformData(commands, 0, screen, sizeof screen);
    SDL_BindGPUVertexBuffers(pass, 0, &buffer, 1);
    size_t first = 0;
    while (first < sent) {
        size_t end = first + 1;
        while (end < sent && kept[end] == kept[first]) {
            end++;
        }
        const Sheet *sheet = &renderer->sheets[kept[first]];
        SDL_GPUTextureSamplerBinding textures[2];
        textures[0].texture = sheet->indices;
        textures[0].sampler = renderer->sampler;
        textures[1].texture = sheet->palette;
        textures[1].sampler = renderer->sampler;
        SDL_BindGPUFragmentSamplers(pass, 0, textures, 2);
        SDL_DrawGPUPrimitives(pass, (Uint32)((end - first) * CORNERS), 1,
            (Uint32)(first * CORNERS), 0);
        first = end;
    }
}

bool renderer_draw(Renderer *renderer, const Sprite *sprites, size_t count)
{
    uint16_t *kept = count == 0 ? NULL : malloc(count * sizeof *kept);
    if (count > 0 && (kept == NULL || !make_room(renderer, count))) {
        free(kept);
        return kept == NULL ? SDL_OutOfMemory() : false;
    }
    SDL_GPUCommandBuffer *commands
        = SDL_AcquireGPUCommandBuffer(renderer->device);
    if (commands == NULL) {
        free(kept);
        return false;
    }
    size_t sent = count == 0 ? 0
        : send_corners(renderer, commands, sprites, count, kept);
    /* The picture the window shows next. There is none while the
     * window is hidden or has no size, and then nothing is drawn. */
    SDL_GPUTexture *screen = NULL;
    Uint32 width = 0;
    Uint32 height = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, renderer->window,
            &screen, &width, &height)) {
        SDL_CancelGPUCommandBuffer(commands);
        free(kept);
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
            if (sent > 0) {
                draw_sprites(renderer, commands, pass, kept, sent, width,
                    height);
            }
            SDL_EndGPURenderPass(pass);
        }
    }
    free(kept);
    return SDL_SubmitGPUCommandBuffer(commands);
}
