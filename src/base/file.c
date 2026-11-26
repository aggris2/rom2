#include "base/file.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

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

bool file_write_all(const char *path, const uint8_t *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return false;
    }
    size_t written = 0;
    if (size > 0) {
        written = fwrite(data, 1, size, f);
    }
    /* fclose() is where a full disk is often reported, so its result
     * counts as well. */
    bool closed = fclose(f) == 0;
    return written == size && closed;
}

bool file_make_folder(const char *path)
{
#ifdef _WIN32
    int result = _mkdir(path);
#else
    int result = mkdir(path, 0777);
#endif
    return result == 0 || errno == EEXIST;
}
