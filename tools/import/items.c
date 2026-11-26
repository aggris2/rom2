#include "import/items.h"

#include <stdio.h>

uint32_t items_variants(const DataBinRow *row,
    ItemVariant out[ITEM_MAX_VARIANTS])
{
    if (row->number_count < ITEM_CLASSES) {
        return 0;
    }
    /* The masks are the last numbers of the row. */
    const int32_t *masks = row->numbers + row->number_count - ITEM_CLASSES;
    uint32_t count = 0;
    for (uint32_t c = 0; c < ITEM_CLASSES; c++) {
        for (uint32_t m = 0; m < ITEM_MATERIALS; m++) {
            if (((uint32_t)masks[c] >> m) & 1u) {
                out[count].material = (uint8_t)m;
                out[count].item_class = (uint8_t)c;
                count++;
            }
        }
    }
    return count;
}

bool items_slot(const DataBin *bin, DataBinTable table,
    const DataBinRow *row, int32_t *out)
{
    uint32_t index = 0;
    if (!databin_column(bin, table, "Slot", &index)
        || index >= row->number_count) {
        return false;
    }
    *out = row->numbers[index];
    return true;
}

bool items_file_name(int32_t material, int32_t slot, int32_t item_class,
    int32_t place, char out[ITEM_FILE_NAME_SIZE])
{
    if (material < 0 || material > 99 || slot < 0 || slot > 99
        || item_class < 0 || item_class > 9 || place < 0 || place > 99) {
        return false;
    }
    snprintf(out, ITEM_FILE_NAME_SIZE, "%02d%02d%d%02d", (int)material,
        (int)slot, (int)item_class, (int)place);
    return true;
}
