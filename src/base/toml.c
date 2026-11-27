#include "base/toml.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
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

bool toml_bool(ByteBuffer *b, const char *key, bool value)
{
    return put(b, key) && put(b, value ? " = true\n" : " = false\n");
}

bool toml_float(ByteBuffer *b, const char *key, double value)
{
    /* Not finite: an infinity or a result that is not a number. Such a
     * value is not equal to itself, or is past the largest number. */
    if (value != value || value > DBL_MAX || value < -DBL_MAX) {
        return false;
    }
    /* 17 digits always read back the same number. Fewer are tried
     * first, so that 0.25 is written as 0.25. A number of ordinary size
     * is not left in the form with an exponent that few digits give it,
     * so that 900 is written as 900.0 and not as 9e+02. */
    double size = value < 0 ? -value : value;
    bool ordinary = size == 0.0 || (size >= 1e-5 && size < 1e15);
    char digits[40];
    for (int precision = 1; precision <= 17; precision++) {
        snprintf(digits, sizeof digits, "%.*g", precision, value);
        if (strtod(digits, NULL) == value
            && !(ordinary && strchr(digits, 'e') != NULL)) {
            break;
        }
    }
    if (strpbrk(digits, ".e") == NULL) {
        strcat(digits, ".0");
    }
    return put(b, key) && put(b, " = ") && put(b, digits) && put(b, "\n");
}

bool toml_table_list(ByteBuffer *b, const char *name)
{
    return put(b, "\n[[") && put(b, name) && put(b, "]]\n");
}

bool toml_int(ByteBuffer *b, const char *key, int32_t value)
{
    return put(b, key) && put(b, " = ") && put_int(b, value)
        && put(b, "\n");
}

/* Adds a text in quotes, with what TOML cannot hold as it is written
 * another way. */
static bool put_quoted(ByteBuffer *b, const char *text)
{
    if (!put(b, "\"")) {
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
    return put(b, "\"");
}

bool toml_text(ByteBuffer *b, const char *key, const char *text)
{
    return put(b, key) && put(b, " = ") && put_quoted(b, text)
        && put(b, "\n");
}

bool toml_text_list(ByteBuffer *b, const char *key,
    const char *const *texts, size_t count)
{
    if (!put(b, key) || !put(b, " = [")) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if ((i > 0 && !put(b, ", ")) || !put_quoted(b, texts[i])) {
            return false;
        }
    }
    return put(b, "]\n");
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
