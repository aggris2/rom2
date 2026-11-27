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

/* A sprite is two triangles, so six corners. */
#define SPRITE_CORNERS 6

/* How long to wait, in milliseconds, when there is no picture of the
 * window to draw into. */
#define NOTHING_TO_DRAW_WAIT 20

/* A square of ground is two triangles: six entries in the list that
 * says which of its four corners make each. */
#define SQUARE_INDICES 6

/* What the vertex shaders are told: as `Screen` in the two .vert
 * files. */
typedef struct ScreenBlock {
    float width;
    float height;
    float light_whole;
    float unused;
} ScreenBlock;

/* What is sent to the card anew each frame: a buffer on the card, the
 * buffer its bytes travel there in, and how many bytes both hold. */
typedef struct Batch {
    SDL_GPUBuffer *buffer;
    SDL_GPUTransferBuffer *transfer;
    size_t room;
} Batch;

struct Renderer {
    SDL_Window *window;
    SDL_GPUDevice *device;
    SDL_GPUGraphicsPipeline *sprite_pipeline;
    SDL_GPUGraphicsPipeline *ground_pipeline;
    SDL_GPUSampler *sampler;
    Sheet *sheets;
    size_t sheet_count;
    Batch sprites;      /* the corners of a frame's sprites */
    Batch ground;       /* the corners of a frame's ground */
    /* Which corners make the triangles of each square of ground: the
     * same for every square, so it is made once, for `squares` of them,
     * and made longer when a frame has more. */
    SDL_GPUBuffer *indices;
    size_t squares;
    uint16_t *kept;     /* the sheet of each sprite that is drawn */
    size_t kept_room;
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

/* Makes a pipeline: two shaders, what a corner is made of, and whether
 * what is drawn is laid over what is there by how solid each pixel is.
 * A corner is `size` bytes and has `count` parts, which `parts` lists
 * in the order of the shader's inputs. */
static SDL_GPUGraphicsPipeline *make_pipeline(SDL_GPUDevice *device,
                                              SDL_Window *window,
                                              const ShaderData *vertex_data,
                                              const ShaderData *fragment_data,
                                              Uint32 size,
                                              const SDL_GPUVertexAttribute
                                                  *parts,
                                              Uint32 count, bool blend)
{
    SDL_GPUShader *vertex = shader_open(device, vertex_data,
        SDL_GPU_SHADERSTAGE_VERTEX, 0, 1);
    SDL_GPUShader *fragment = shader_open(device, fragment_data,
        SDL_GPU_SHADERSTAGE_FRAGMENT, 2, 0);
    SDL_GPUGraphicsPipeline *pipeline = NULL;
    if (vertex != NULL && fragment != NULL) {
        SDL_GPUVertexBufferDescription buffer;
        SDL_zero(buffer);
        buffer.pitch = size;
        buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUColorTargetDescription target;
        SDL_zero(target);
        target.format = SDL_GetGPUSwapchainTextureFormat(device, window);
        target.blend_state.enable_blend = blend;
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
        info.vertex_input_state.num_vertex_attributes = count;
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

/* Gives back what a batch holds on the card. */
static void batch_close(Batch *batch, SDL_GPUDevice *device)
{
    if (batch->buffer != NULL) {
        SDL_ReleaseGPUBuffer(device, batch->buffer);
    }
    if (batch->transfer != NULL) {
        SDL_ReleaseGPUTransferBuffer(device, batch->transfer);
    }
    batch->buffer = NULL;
    batch->transfer = NULL;
    batch->room = 0;
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
    /* A sprite's corner is a place and a point of its sheet, as floats.
     * The ground's corners are handed over as the view wrote them: see
     * GroundCorner. The ground has no holes to see through. */
    const SDL_GPUVertexAttribute sprite_parts[2] = {
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Corner, x)},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Corner, u)}
    };
    const SDL_GPUVertexAttribute ground_parts[4] = {
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_SHORT2,
            offsetof(GroundCorner, x)},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_USHORT2,
            offsetof(GroundCorner, point_x)},
        {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_USHORT2,
            offsetof(GroundCorner, tile_x)},
        {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_USHORT2,
            offsetof(GroundCorner, light)}
    };
    renderer->sprite_pipeline = make_pipeline(renderer->device, window,
        &shader_sprite_vert, &shader_sprite_frag, sizeof(Corner),
        sprite_parts, 2, true);
    renderer->ground_pipeline = make_pipeline(renderer->device, window,
        &shader_ground_vert, &shader_ground_frag, sizeof(GroundCorner),
        ground_parts, 4, false);
    /* A new frame is shown when the monitor is ready for one, so that
     * no more frames are drawn than can be seen. */
    if (renderer->sampler == NULL || renderer->sprite_pipeline == NULL
        || renderer->ground_pipeline == NULL
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
    free(renderer->kept);
    batch_close(&renderer->sprites, device);
    batch_close(&renderer->ground, device);
    if (renderer->indices != NULL) {
        SDL_ReleaseGPUBuffer(device, renderer->indices);
    }
    if (renderer->sprite_pipeline != NULL) {
        SDL_ReleaseGPUGraphicsPipeline(device, renderer->sprite_pipeline);
    }
    if (renderer->ground_pipeline != NULL) {
        SDL_ReleaseGPUGraphicsPipeline(device, renderer->ground_pipeline);
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

/* Makes room in a batch for `size` bytes. */
static bool batch_make_room(Batch *batch, SDL_GPUDevice *device,
                            size_t size)
{
    if (size <= batch->room) {
        return true;
    }
    /* Twice what is asked for, so that a frame with a little more than
     * the last does not start again. */
    if (size > UINT32_MAX / 2) {
        return SDL_SetError("there is too much to draw");
    }
    SDL_GPUBufferCreateInfo buffer_info;
    SDL_zero(buffer_info);
    buffer_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    buffer_info.size = (Uint32)(size * 2);
    SDL_GPUTransferBufferCreateInfo transfer_info;
    SDL_zero(transfer_info);
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = buffer_info.size;
    SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(device, &buffer_info);
    SDL_GPUTransferBuffer *transfer = buffer == NULL ? NULL
        : SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (transfer == NULL) {
        if (buffer != NULL) {
            SDL_ReleaseGPUBuffer(device, buffer);
        }
        return false;
    }
    batch_close(batch, device);
    batch->buffer = buffer;
    batch->transfer = transfer;
    batch->room = size * 2;
    return true;
}

/* Sends the first `size` bytes of a batch's travelling buffer to the
 * card, as part of `commands`. */
static void batch_send(const Batch *batch, SDL_GPUCommandBuffer *commands,
                       size_t size)
{
    SDL_GPUTransferBufferLocation from;
    SDL_GPUBufferRegion to;
    SDL_zero(from);
    SDL_zero(to);
    from.transfer_buffer = batch->transfer;
    to.buffer = batch->buffer;
    to.size = (Uint32)size;
    SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(commands);
    SDL_UploadToGPUBuffer(pass, &from, &to, true);
    SDL_EndGPUCopyPass(pass);
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

/* Sends the sprites of a frame to the card, as part of `commands`.
 * `*sent` gets how many were sent, those with a sheet there is, and
 * `renderer->kept` which sheet each of them has. */
static bool send_sprites(Renderer *renderer, SDL_GPUCommandBuffer *commands,
                         const Frame *frame, size_t *sent)
{
    *sent = 0;
    if (frame->sprite_count == 0) {
        return true;
    }
    if (frame->sprite_count > renderer->kept_room) {
        uint16_t *kept = realloc(renderer->kept,
            frame->sprite_count * sizeof *kept);
        if (kept == NULL) {
            return SDL_OutOfMemory();
        }
        renderer->kept = kept;
        renderer->kept_room = frame->sprite_count;
    }
    if (!batch_make_room(&renderer->sprites, renderer->device,
            frame->sprite_count * SPRITE_CORNERS * sizeof(Corner))) {
        return false;
    }
    Corner *corners = SDL_MapGPUTransferBuffer(renderer->device,
        renderer->sprites.transfer, true);
    if (corners == NULL) {
        return false;
    }
    for (size_t i = 0; i < frame->sprite_count; i++) {
        if (frame->sprites[i].sheet < renderer->sheet_count) {
            put_corners(&corners[*sent * SPRITE_CORNERS], &frame->sprites[i]);
            renderer->kept[(*sent)++] = frame->sprites[i].sheet;
        }
    }
    SDL_UnmapGPUTransferBuffer(renderer->device,
        renderer->sprites.transfer);
    if (*sent > 0) {
        batch_send(&renderer->sprites, commands,
            *sent * SPRITE_CORNERS * sizeof(Corner));
    }
    return true;
}

/* Makes the list of triangles long enough for `squares` squares of
 * ground: for each, its corners 0, 1, 2 and then 1, 3, 2. */
static bool make_indices(Renderer *renderer, size_t squares)
{
    if (squares <= renderer->squares) {
        return true;
    }
    /* Twice what is asked for, so that a slightly larger window does
     * not start again. */
    squares *= 2;
    if (squares > UINT32_MAX / (SQUARE_INDICES * sizeof(Uint32))) {
        return SDL_SetError("there is too much to draw");
    }
    Uint32 size = (Uint32)(squares * SQUARE_INDICES * sizeof(Uint32));
    SDL_GPUBufferCreateInfo buffer_info;
    SDL_zero(buffer_info);
    buffer_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    buffer_info.size = size;
    SDL_GPUTransferBufferCreateInfo transfer_info;
    SDL_zero(transfer_info);
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = size;
    SDL_GPUDevice *device = renderer->device;
    SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(device, &buffer_info);
    SDL_GPUTransferBuffer *transfer = buffer == NULL ? NULL
        : SDL_CreateGPUTransferBuffer(device, &transfer_info);
    Uint32 *indices = transfer == NULL ? NULL
        : SDL_MapGPUTransferBuffer(device, transfer, false);
    SDL_GPUCommandBuffer *commands = indices == NULL ? NULL
        : SDL_AcquireGPUCommandBuffer(device);
    bool ok = false;
    if (indices != NULL) {
        const Uint32 order[SQUARE_INDICES] = {0, 1, 2, 1, 3, 2};
        for (size_t i = 0; i < squares * SQUARE_INDICES; i++) {
            indices[i] = (Uint32)(i / SQUARE_INDICES * FRAME_SQUARE_CORNERS)
                + order[i % SQUARE_INDICES];
        }
        SDL_UnmapGPUTransferBuffer(device, transfer);
    }
    if (commands != NULL) {
        SDL_GPUTransferBufferLocation from;
        SDL_GPUBufferRegion to;
        SDL_zero(from);
        SDL_zero(to);
        from.transfer_buffer = transfer;
        to.buffer = buffer;
        to.size = size;
        SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(commands);
        SDL_UploadToGPUBuffer(pass, &from, &to, false);
        SDL_EndGPUCopyPass(pass);
        ok = SDL_SubmitGPUCommandBuffer(commands);
    }
    if (transfer != NULL) {
        SDL_ReleaseGPUTransferBuffer(device, transfer);
    }
    if (!ok) {
        if (buffer != NULL) {
            SDL_ReleaseGPUBuffer(device, buffer);
        }
        return false;
    }
    if (renderer->indices != NULL) {
        SDL_ReleaseGPUBuffer(device, renderer->indices);
    }
    renderer->indices = buffer;
    renderer->squares = squares;
    return true;
}

/* Sends the ground of a frame to the card, as part of `commands`: the
 * corners as the view wrote them. `*sent` gets how many squares were
 * sent: none if the sheet of the ground is not there. */
static bool send_ground(Renderer *renderer, SDL_GPUCommandBuffer *commands,
                        const Frame *frame, size_t *sent)
{
    *sent = 0;
    size_t squares = frame->ground_count / FRAME_SQUARE_CORNERS;
    if (squares == 0 || frame->ground_sheet >= renderer->sheet_count) {
        return true;
    }
    size_t size = squares * FRAME_SQUARE_CORNERS * sizeof(GroundCorner);
    if (!batch_make_room(&renderer->ground, renderer->device, size)) {
        return false;
    }
    void *out = SDL_MapGPUTransferBuffer(renderer->device,
        renderer->ground.transfer, true);
    if (out == NULL) {
        return false;
    }
    memcpy(out, frame->ground, size);
    SDL_UnmapGPUTransferBuffer(renderer->device, renderer->ground.transfer);
    batch_send(&renderer->ground, commands, size);
    *sent = squares;
    return true;
}

/* Tells the pass which two pictures of a sheet the shader reads. */
static void bind_sheet(const Renderer *renderer, SDL_GPURenderPass *pass,
                       uint16_t number)
{
    const Sheet *sheet = &renderer->sheets[number];
    SDL_GPUTextureSamplerBinding textures[2];
    textures[0].texture = sheet->indices;
    textures[0].sampler = renderer->sampler;
    textures[1].texture = sheet->palette;
    textures[1].sampler = renderer->sampler;
    SDL_BindGPUFragmentSamplers(pass, 0, textures, 2);
}

/* Draws what was sent: `ground` squares of ground in one call, then
 * the sprites, those that follow each other with one sheet in one call
 * each. */
static void draw_sent(const Renderer *renderer,
                      SDL_GPUCommandBuffer *commands,
                      SDL_GPURenderPass *pass, const Frame *frame,
                      size_t ground, size_t sprites, Uint32 width,
                      Uint32 height)
{
    ScreenBlock screen;
    screen.width = (float)width;
    screen.height = (float)height;
    screen.light_whole = (float)FRAME_LIGHT_WHOLE;
    screen.unused = 0.0f;
    SDL_GPUBufferBinding buffer;
    SDL_zero(buffer);
    SDL_PushGPUVertexUniformData(commands, 0, &screen, sizeof screen);
    if (ground > 0) {
        SDL_GPUBufferBinding indices;
        SDL_zero(indices);
        indices.buffer = renderer->indices;
        buffer.buffer = renderer->ground.buffer;
        SDL_BindGPUGraphicsPipeline(pass, renderer->ground_pipeline);
        SDL_BindGPUVertexBuffers(pass, 0, &buffer, 1);
        SDL_BindGPUIndexBuffer(pass, &indices,
            SDL_GPU_INDEXELEMENTSIZE_32BIT);
        bind_sheet(renderer, pass, frame->ground_sheet);
        SDL_DrawGPUIndexedPrimitives(pass,
            (Uint32)(ground * SQUARE_INDICES), 1, 0, 0, 0);
    }
    if (sprites > 0) {
        buffer.buffer = renderer->sprites.buffer;
        SDL_BindGPUGraphicsPipeline(pass, renderer->sprite_pipeline);
        SDL_BindGPUVertexBuffers(pass, 0, &buffer, 1);
    }
    size_t first = 0;
    while (first < sprites) {
        size_t end = first + 1;
        while (end < sprites
            && renderer->kept[end] == renderer->kept[first]) {
            end++;
        }
        bind_sheet(renderer, pass, renderer->kept[first]);
        SDL_DrawGPUPrimitives(pass, (Uint32)((end - first) * SPRITE_CORNERS), 1,
            (Uint32)(first * SPRITE_CORNERS), 0);
        first = end;
    }
}

bool renderer_draw(Renderer *renderer, const Frame *frame)
{
    /* The list of triangles is sent on its own, before the frame. */
    if (!make_indices(renderer,
            frame->ground_count / FRAME_SQUARE_CORNERS)) {
        return false;
    }
    SDL_GPUCommandBuffer *commands
        = SDL_AcquireGPUCommandBuffer(renderer->device);
    if (commands == NULL) {
        return false;
    }
    /* The picture the window shows next. */
    SDL_GPUTexture *screen = NULL;
    Uint32 width = 0;
    Uint32 height = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, renderer->window,
            &screen, &width, &height)) {
        SDL_CancelGPUCommandBuffer(commands);
        return false;
    }
    if (screen == NULL) {
        /* There is none while the window is minimized or has no size.
         * Nothing is sent or drawn then, and a short wait keeps the
         * loop from running as fast as it can for nothing. */
        SDL_CancelGPUCommandBuffer(commands);
        SDL_Delay(NOTHING_TO_DRAW_WAIT);
        return true;
    }
    /* From here on the frame has to be handed in: one that has the
     * window's picture cannot be cancelled. */
    size_t ground = 0;
    size_t sprites = 0;
    bool sent = send_ground(renderer, commands, frame, &ground)
        && send_sprites(renderer, commands, frame, &sprites);
    SDL_GPUColorTargetInfo target;
    SDL_zero(target);
    target.texture = screen;
    target.clear_color.a = 1.0f;
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, &target, 1,
        NULL);
    if (pass != NULL) {
        if (sent) {
            draw_sent(renderer, commands, pass, frame, ground, sprites,
                width, height);
        }
        SDL_EndGPURenderPass(pass);
    }
    return SDL_SubmitGPUCommandBuffer(commands) && sent;
}
