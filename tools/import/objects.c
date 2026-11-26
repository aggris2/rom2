#include "import/objects.h"

#include <stdio.h>
#include <string.h>

Things objects_things(const Reg *reg)
{
    Things t = {reg, "ObjectCount", "Object"};
    return t;
}

bool objects_sprite_path(const Reg *reg, int32_t id, char *out,
    size_t size)
{
    Things t = objects_things(reg);
    int32_t file = 0;
    char key[32];
    char path[256];
    const char *text = NULL;
    if (!things_int(&t, id, "File", &file)) {
        return false;
    }
    snprintf(key, sizeof key, "File%d", (int)file);
    if (!reg_text(reg, "Files", key, &text)) {
        return false;
    }
    int written = snprintf(path, sizeof path, "objects/%s.256", text);
    if (written < 0 || (size_t)written >= sizeof path
        || (size_t)written >= size) {
        return false;
    }
    for (char *p = path; *p != '\0'; p++) {
        if (*p == '\\') {
            *p = '/';
        }
    }
    memcpy(out, path, (size_t)written + 1);
    return true;
}

bool objects_frame_range(const Reg *reg, int32_t id, uint32_t file_frames,
    uint32_t *out_first, uint32_t *out_count)
{
    Things t = objects_things(reg);
    char section[THINGS_SECTION_SIZE];
    int32_t first = 0;
    int32_t count = 1;
    if (!things_section(&t, id, section)) {
        return false;
    }
    things_int(&t, id, "Index", &first);
    things_int(&t, id, "Phases", &count);
    if (first < 0 || count < 0 || (uint32_t)first >= file_frames) {
        return false;
    }
    uint32_t left = file_frames - (uint32_t)first;
    *out_first = (uint32_t)first;
    *out_count = (uint32_t)count < left ? (uint32_t)count : left;
    return true;
}
