#include "import/item_names.h"

#include <string.h>

#include "base/buffer.h"
#include "import/text.h"

/* Finds line number `index` of a text, counted from 0. A line ends at a
 * line feed, and a carriage return before that is not part of it. The
 * last line may end with the text. Returns false if there is no such
 * line. */
static bool line_at(const uint8_t *text, size_t size, uint32_t index,
    const uint8_t **out, size_t *out_length)
{
    size_t start = 0;
    for (uint32_t line = 0; start <= size; line++) {
        size_t end = start;
        while (end < size && text[end] != '\n') {
            end++;
        }
        if (line == index) {
            /* Past the last line feed there is a line only if there is
             * something in it. */
            if (end == size && start == size) {
                return false;
            }
            size_t length = end - start;
            if (length > 0 && text[start + length - 1] == '\r') {
                length--;
            }
            *out = text + start;
            *out_length = length;
            return true;
        }
        start = end + 1;
    }
    return false;
}

/* Counts the lines of a text, as line_at() sees them. */
static uint32_t line_count(const uint8_t *text, size_t size)
{
    uint32_t count = 0;
    for (size_t i = 0; i < size; i++) {
        if (text[i] == '\n') {
            count++;
        }
    }
    return size > 0 && text[size - 1] != '\n' ? count + 1 : count;
}

bool item_names_open(const uint8_t *numbers, size_t numbers_size,
    const uint8_t *server, size_t server_size, const uint8_t *shown,
    size_t shown_size, ItemNames *out)
{
    if (numbers_size % 2 != 0 || numbers_size / 2 > 0xffffu) {
        return false;
    }
    uint32_t count = (uint32_t)(numbers_size / 2);
    if (line_count(server, server_size) < count
        || line_count(shown, shown_size) < count) {
        return false;
    }
    out->numbers = numbers;
    out->count = count;
    out->server = server;
    out->server_size = server_size;
    out->shown = shown;
    out->shown_size = shown_size;
    return true;
}

/* Writes the name shown of item number `index` in the files to `out`. */
static bool shown_at(const ItemNames *names, uint32_t index,
    char out[ITEM_NAME_SIZE])
{
    const uint8_t *line = NULL;
    size_t length = 0;
    ByteBuffer utf8 = {0};
    bool ok = line_at(names->shown, names->shown_size, index, &line,
            &length)
        && text_cp1251_to_utf8(line, length, &utf8)
        && utf8.size < ITEM_NAME_SIZE;
    if (ok) {
        if (utf8.size > 0) {
            memcpy(out, utf8.data, utf8.size);
        }
        out[utf8.size] = '\0';
    }
    buffer_free(&utf8);
    return ok;
}

bool item_names_shown(const ItemNames *names, int32_t material,
    int32_t slot, int32_t item_class, int32_t place,
    char out[ITEM_NAME_SIZE])
{
    if (material < 0 || material > 15 || slot < 0 || slot > 15
        || item_class < 0 || item_class > 7 || place < 0 || place > 31) {
        return false;
    }
    uint32_t wanted = ((uint32_t)material << 12) | ((uint32_t)slot << 8)
        | ((uint32_t)item_class << 5) | (uint32_t)place;
    for (uint32_t i = 0; i < names->count; i++) {
        uint32_t number = names->numbers[i * 2]
            | ((uint32_t)names->numbers[i * 2 + 1] << 8);
        if (number == wanted) {
            return shown_at(names, i, out);
        }
    }
    return false;
}

bool item_names_shown_for(const ItemNames *names, const char *server_name,
    char out[ITEM_NAME_SIZE])
{
    size_t wanted = strlen(server_name);
    for (uint32_t i = 0; i < names->count; i++) {
        const uint8_t *line = NULL;
        size_t length = 0;
        if (line_at(names->server, names->server_size, i, &line, &length)
            && length == wanted && memcmp(line, server_name, wanted) == 0) {
            return shown_at(names, i, out);
        }
    }
    return false;
}
