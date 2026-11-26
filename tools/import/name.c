#include "import/name.h"

void name_copy(char *dst, size_t dst_size, const uint8_t *field,
    size_t field_size)
{
    if (dst_size == 0) {
        return;
    }
    size_t i = 0;
    while (i < dst_size - 1 && i < field_size && field[i] != 0) {
        dst[i] = (char)field[i];
        i++;
    }
    dst[i] = '\0';
}
