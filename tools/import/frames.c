#include "import/frames.h"

#include <stdlib.h>

void frames_free(Frames *frames)
{
    for (uint32_t i = 0; i < frames->count; i++) {
        frame256_free(&frames->items[i]);
    }
    free(frames->items);
    frames->items = NULL;
    frames->count = 0;
}

bool frames_read(const Sprite256 *sprite, Frames *out)
{
    Frames frames = {NULL, 0};
    frames.items = calloc(sprite->frame_count > 0 ? sprite->frame_count : 1,
        sizeof *frames.items);
    if (frames.items == NULL) {
        return false;
    }
    Reader r = sprite256_frames(sprite);
    for (uint32_t i = 0; i < sprite->frame_count; i++) {
        Frame256 *frame = &frames.items[i];
        if (!sprite256_frame_read(&r, frame)) {
            frames_free(&frames);
            return false;
        }
        frames.count = i + 1;
    }
    *out = frames;
    return true;
}

/* A frame is the middle of the box. */
static uint32_t centred(uint32_t box, uint32_t frame)
{
    return (box - frame) / 2;
}

FrameWindow frame_window_empty(uint32_t box_width, uint32_t box_height)
{
    FrameWindow window = {box_width, box_height, 0, 0};
    return window;
}

bool frames_measure(const Frames *frames, uint32_t index,
    uint32_t box_width, uint32_t box_height, FrameWindow *window)
{
    if (index >= frames->count) {
        return false;
    }
    const Frame256 *frame = &frames->items[index];
    if (frame->width > box_width || frame->height > box_height) {
        return false;
    }
    if (frame->width == 0 || frame->height == 0) {
        return true;
    }
    uint32_t x = centred(box_width, frame->width);
    uint32_t y = centred(box_height, frame->height);
    if (x < window->left) {
        window->left = x;
    }
    if (y < window->top) {
        window->top = y;
    }
    if (y + frame->height > window->bottom) {
        window->bottom = y + frame->height;
    }
    return true;
}

void frame_window_close(FrameWindow *window, uint32_t box_width,
    uint32_t box_height)
{
    if (window->left >= box_width || window->top >= window->bottom) {
        /* Nothing to show: one empty pixel in the middle of the box. */
        window->left = box_width / 2;
        window->top = box_height / 2;
        window->bottom = window->top + 1;
        window->right = window->left + 1;
        return;
    }
    /* As far from the right edge as from the left. A frame that could
     * not be centred exactly has its spare pixel on the right, and
     * mirrored on the left, and this leaves room for both. */
    window->right = box_width - window->left;
}

bool frames_put(const Frames *frames, uint32_t index, bool mirror,
    uint32_t box_width, uint32_t box_height, const FrameWindow *window,
    Sheet *sheet, uint32_t column, uint32_t row)
{
    if (index >= frames->count) {
        return false;
    }
    const Frame256 *frame = &frames->items[index];
    if (frame->width > box_width || frame->height > box_height) {
        return false;
    }
    if (frame->width == 0 || frame->height == 0) {
        return true;
    }
    uint32_t x = centred(box_width, frame->width);
    uint32_t y = centred(box_height, frame->height);
    if (mirror) {
        x = box_width - x - frame->width;
    }
    if (x < window->left || y < window->top) {
        return false;
    }
    return sheet_put(sheet, column, row, frame->pixels, frame->width,
        frame->height, x - window->left, y - window->top, mirror);
}
