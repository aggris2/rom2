#include "base/file.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <dirent.h>
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

/* The names being gathered: one block, each name closed by a zero. */
typedef struct Names {
    char *block;
    size_t size;
    size_t room;
    size_t count;
} Names;

/* Adds a name to the block. "." and ".." are passed over. */
static bool names_add(Names *names, const char *name)
{
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return true;
    }
    size_t length = strlen(name) + 1;
    if (length > names->room - names->size) {
        size_t room = names->room * 2 + length + 256;
        char *block = realloc(names->block, room);
        if (block == NULL) {
            return false;
        }
        names->block = block;
        names->room = room;
    }
    memcpy(names->block + names->size, name, length);
    names->size += length;
    names->count++;
    return true;
}

bool file_list_folder(const char *path, char **out_names,
    size_t *out_count)
{
    /* One byte to start with, so an empty folder still gives a block
     * to free. */
    Names names = {malloc(1), 0, 1, 0};
    bool ok = names.block != NULL;
    if (!ok) {
        return false;
    }
#ifdef _WIN32
    /* Everything in the folder: its path, a separator and a star. */
    size_t length = strlen(path);
    char *pattern = malloc(length + 3);
    if (pattern == NULL) {
        free(names.block);
        return false;
    }
    memcpy(pattern, path, length);
    memcpy(pattern + length, "/*", 3);
    struct _finddata_t found;
    intptr_t handle = _findfirst(pattern, &found);
    free(pattern);
    if (handle == -1) {
        /* An empty folder still has "." in it, so this is a folder
         * that cannot be read. */
        free(names.block);
        return false;
    }
    do {
        ok = names_add(&names, found.name);
    } while (ok && _findnext(handle, &found) == 0);
    _findclose(handle);
#else
    DIR *folder = opendir(path);
    if (folder == NULL) {
        free(names.block);
        return false;
    }
    struct dirent *entry;
    while (ok && (entry = readdir(folder)) != NULL) {
        ok = names_add(&names, entry->d_name);
    }
    closedir(folder);
#endif
    if (!ok) {
        free(names.block);
        return false;
    }
    *out_names = names.block;
    *out_count = names.count;
    return true;
}
