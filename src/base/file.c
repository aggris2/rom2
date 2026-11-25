#include "base/file.h"

#include <stdio.h>
#include <stdlib.h>

bool file_read_all(const char *path, uint8_t **out_data, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return false;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    long length = ftell(f);
    if (length < 0) {
        fclose(f);
        return false;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }
    size_t size = (size_t)length;
    uint8_t *data = malloc(size > 0 ? size : 1);
    if (data == NULL) {
        fclose(f);
        return false;
    }

    size_t got = fread(data, 1, size, f);
    if (got != size) {
        fclose(f);
        free(data);
        return false;
    }
    fclose(f);
    *out_data = data;
    *out_size = size;
    return true;
}
