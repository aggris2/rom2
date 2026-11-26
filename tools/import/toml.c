#include "import/toml.h"

#include <stdio.h>
#include <string.h>

static bool put(ByteBuffer *b, const char *text)
{
    return buffer_append(b, (const uint8_t *)text, strlen(text));
}

static bool put_int(ByteBuffer *b, int32_t value)
{
    char digits[16];
    snprintf(digits, sizeof digits, "%d", (int)value);
    return put(b, digits);
}

bool toml_comment(ByteBuffer *b, const char *text)
{
    return put(b, "# ") && put(b, text) && put(b, "\n");
}

bool toml_table(ByteBuffer *b, const char *name)
{
    return put(b, "\n[") && put(b, name) && put(b, "]\n");
}

bool toml_int(ByteBuffer *b, const char *key, int32_t value)
{
    return put(b, key) && put(b, " = ") && put_int(b, value)
        && put(b, "\n");
}

bool toml_text(ByteBuffer *b, const char *key, const char *text)
{
    if (!put(b, key) || !put(b, " = \"")) {
        return false;
    }
    for (const char *p = text; *p != '\0'; p++) {
        uint8_t c = (uint8_t)*p;
        bool ok;
        if (c == '"' || c == '\\') {
            ok = buffer_push(b, '\\') && buffer_push(b, c);
        } else if (c < 0x20 || c == 0x7f) {
            /* TOML does not allow these as they are. */
            char code[8];
            snprintf(code, sizeof code, "\\u%04X", (unsigned)c);
            ok = put(b, code);
        } else {
            ok = buffer_push(b, c);
        }
        if (!ok) {
            return false;
        }
    }
    return put(b, "\"\n");
}

bool toml_int_list(ByteBuffer *b, const char *key, const int32_t *values,
    size_t count)
{
    if (!put(b, key) || !put(b, " = [")) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if ((i > 0 && !put(b, ", ")) || !put_int(b, values[i])) {
            return false;
        }
    }
    return put(b, "]\n");
}
