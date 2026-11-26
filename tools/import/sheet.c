#include "import/sheet.h"

#include <stdlib.h>

bool sheet_create(Sheet *out, uint32_t cell_width, uint32_t cell_height,
    uint32_t columns, uint32_t rows)
{
    if (cell_width == 0 || cell_height == 0 || columns == 0 || rows == 0
        || cell_width > SHEET_MAX_SIDE / columns
        || cell_height > SHEET_MAX_SIDE / rows) {
        return false;
    }
    size_t width = (size_t)cell_width * columns;
    size_t height = (size_t)cell_height * rows;
    uint8_t *pixels = calloc(width * height, 1);
    if (pixels == NULL) {
        return false;
    }
    out->cell_width = cell_width;
    out->cell_height = cell_height;
    out->columns = columns;
    out->rows = rows;
    out->pixels = pixels;
    return true;
}

void sheet_free(Sheet *sheet)
{
    free(sheet->pixels);
    sheet->pixels = NULL;
    sheet->cell_width = 0;
    sheet->cell_height = 0;
    sheet->columns = 0;
    sheet->rows = 0;
}

uint32_t sheet_width(const Sheet *sheet)
{
    return sheet->cell_width * sheet->columns;
}

uint32_t sheet_height(const Sheet *sheet)
{
    return sheet->cell_height * sheet->rows;
}

bool sheet_put(Sheet *sheet, uint32_t column, uint32_t row,
    const uint8_t *pixels, uint32_t width, uint32_t height, uint32_t x,
    uint32_t y, bool mirror)
{
    if (column >= sheet->columns || row >= sheet->rows
        || x > sheet->cell_width || width > sheet->cell_width - x
        || y > sheet->cell_height || height > sheet->cell_height - y) {
        return false;
    }

    size_t stride = sheet_width(sheet);
    size_t left = (size_t)column * sheet->cell_width + x;
    size_t top = (size_t)row * sheet->cell_height + y;
    for (uint32_t fy = 0; fy < height; fy++) {
        const uint8_t *from = pixels + (size_t)fy * width;
        uint8_t *to = sheet->pixels + (top + fy) * stride + left;
        for (uint32_t fx = 0; fx < width; fx++) {
            uint8_t pixel = from[mirror ? width - 1 - fx : fx];
            if (pixel != 0) {
                to[fx] = pixel;
            }
        }
    }
    return true;
}
