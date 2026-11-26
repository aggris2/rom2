#include "import/structure_sheet.h"

#include <string.h>

#include "import/frames.h"

/* Puts tile `tile` of the file at tile `index` of the picture in cell
 * `column`. */
static bool put_tile(const StructureLayout *layout, const Frames *frames,
    uint32_t tile, uint32_t index, Sheet *sheet, uint32_t column)
{
    if (tile >= frames->count) {
        return false;
    }
    const Frame256 *frame = &frames->items[tile];
    if (frame->width != STRUCTURE_TILE || frame->height != STRUCTURE_TILE) {
        return false;
    }
    return sheet_put(sheet, column, 0, frame->pixels, frame->width,
        frame->height, (index % layout->width) * STRUCTURE_TILE,
        (index / layout->width) * STRUCTURE_TILE, false);
}

/* Builds the sheet of the animation, or of the pieces, and that of the
 * ruin, from one set of frames. */
static bool build(const StructureLayout *layout, const Frames *frames,
    Sheet *sheet, Sheet *ruin)
{
    uint32_t tiles = layout->width * layout->height;
    uint32_t columns = layout->is_pieces ? layout->pieces : layout->phases;
    if (!sheet_create(sheet, layout->width * STRUCTURE_TILE,
            layout->height * STRUCTURE_TILE, columns, 1)) {
        return false;
    }
    for (uint32_t column = 0; column < columns; column++) {
        for (uint32_t index = 0; index < tiles; index++) {
            uint32_t tile = column;
            if (!layout->is_pieces
                && !structures_tile(layout, column, index, &tile)) {
                return false;
            }
            if (!put_tile(layout, frames, tile, index, sheet, column)) {
                return false;
            }
        }
    }
    if (!layout->has_ruin) {
        return true;
    }
    if (!sheet_create(ruin, layout->width * STRUCTURE_TILE,
            layout->height * STRUCTURE_TILE, 1, 1)) {
        return false;
    }
    for (uint32_t index = 0; index < tiles; index++) {
        uint32_t tile = 0;
        if (!structures_ruin_tile(layout, index, &tile)
            || !put_tile(layout, frames, tile, index, ruin, 0)) {
            return false;
        }
    }
    return true;
}

bool structure_sheet_build(const StructureLayout *layout,
    const Sprite256 *sprite, const Sprite256 *edges, StructureSheet *out)
{
    if (layout->width == 0 || layout->height == 0
        || (edges != NULL && edges->frame_count != sprite->frame_count)) {
        return false;
    }
    Frames frames;
    Frames edge_frames = {NULL, 0};
    if (!frames_read(sprite, &frames)) {
        return false;
    }
    if (edges != NULL && !frames_read(edges, &edge_frames)) {
        frames_free(&frames);
        return false;
    }

    StructureSheet result;
    memset(&result, 0, sizeof result);
    bool ok = build(layout, &frames, &result.sheet, &result.ruin)
        && (edges == NULL || build(layout, &edge_frames, &result.edges,
            &result.ruin_edges));
    frames_free(&frames);
    frames_free(&edge_frames);
    if (!ok) {
        structure_sheet_free(&result);
        return false;
    }
    *out = result;
    return true;
}

void structure_sheet_free(StructureSheet *sheet)
{
    sheet_free(&sheet->sheet);
    sheet_free(&sheet->edges);
    sheet_free(&sheet->ruin);
    sheet_free(&sheet->ruin_edges);
    memset(sheet, 0, sizeof *sheet);
}
