#include "base/toml.h"

#include <float.h>
#include <limits.h>
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

/* ---- Reading ---- */

typedef enum Kind {
    KIND_INT,
    KIND_FLOAT,
    KIND_BOOL,
    KIND_TEXT,
    KIND_INT_LIST,
    KIND_TEXT_LIST,
    KIND_TABLE,
    KIND_TABLE_LIST
} Kind;

/* A key of a table and what it holds. */
typedef struct Entry {
    struct Entry *next;
    const char *key;
    Kind kind;
    union {
        int32_t whole;
        double number;
        bool yes;
        const char *text;
        int32_t *ints;
        const char **texts;
        TomlTable *table;       /* the one, or the first of a list */
    } as;
    size_t count;               /* of a list */
} Entry;

struct TomlTable {
    Entry *first;
    Entry *last;
    TomlTable *next;            /* in a list of tables */
    /* True once a header has named this table, so that a second one
     * can be refused. A table that "[a.b]" made on its way to b has
     * not been named yet. */
    bool named;
};

/* Every piece of memory a file's tables take is one of these, in a
 * chain, so that all of it is freed by walking the chain. */
typedef union Block {
    union Block *next;
    double align;
} Block;

/* The table of the top, with the chain it and all below it are in. */
typedef struct Top {
    TomlTable table;
    Block *blocks;
} Top;

/* Where the reading is, and whether it stopped for want of memory. */
typedef struct Parse {
    const char *at;
    const char *end;
    Block **blocks;
    bool no_memory;
} Parse;

static void free_blocks(Block *block)
{
    while (block != NULL) {
        Block *next = block->next;
        free(block);
        block = next;
    }
}

/* Gives `size` bytes, set to zero and freed with the file's tables.
 * Returns NULL if memory runs out. */
static void *take_room(Parse *p, size_t size)
{
    Block *block = calloc(1, sizeof *block + size);
    if (block == NULL) {
        p->no_memory = true;
        return NULL;
    }
    block->next = *p->blocks;
    *p->blocks = block;
    return block + 1;
}

void toml_free(TomlTable *top)
{
    if (top != NULL) {
        /* The table of the top is itself the last block of its chain. */
        free_blocks(((Top *)top)->blocks);
    }
}

static Entry *find(const TomlTable *table, const char *key)
{
    for (Entry *entry = table->first; entry != NULL; entry = entry->next) {
        if (strcmp(entry->key, key) == 0) {
            return entry;
        }
    }
    return NULL;
}

static const Entry *find_kind(const TomlTable *table, const char *key,
                              Kind kind)
{
    const Entry *entry = find(table, key);
    return entry != NULL && entry->kind == kind ? entry : NULL;
}

/* Adds an entry to the end of a table. Returns NULL if memory runs
 * out. */
static Entry *add(Parse *p, TomlTable *table, const char *key, Kind kind)
{
    Entry *entry = take_room(p, sizeof *entry);
    if (entry == NULL) {
        return NULL;
    }
    entry->key = key;
    entry->kind = kind;
    if (table->last == NULL) {
        table->first = entry;
    } else {
        table->last->next = entry;
    }
    table->last = entry;
    return entry;
}

static void skip_spaces(Parse *p)
{
    while (p->at < p->end && (*p->at == ' ' || *p->at == '\t')) {
        p->at++;
    }
}

/* True if the next character is `c`, which is then passed. */
static bool take(Parse *p, char c)
{
    if (p->at < p->end && *p->at == c) {
        p->at++;
        return true;
    }
    return false;
}

/* A character TOML does not allow as it is, in a text or a comment. */
static bool is_control(char c)
{
    return ((unsigned char)c < 0x20 && c != '\t') || c == 0x7f;
}

/* Reads the four digits after "\u" and writes the character they name
 * as UTF-8 at `out`. Returns how many bytes it wrote, or 0 if they are
 * not four digits, or name the character 0, which a text in C cannot
 * hold, or half of a pair, which is no character. */
static size_t read_code(Parse *p, char *out)
{
    unsigned code = 0;
    for (int i = 0; i < 4; i++) {
        char c = p->at < p->end ? *p->at++ : 'x';
        unsigned digit = c >= '0' && c <= '9' ? (unsigned)(c - '0')
            : c >= 'a' && c <= 'f' ? (unsigned)(c - 'a' + 10)
            : c >= 'A' && c <= 'F' ? (unsigned)(c - 'A' + 10) : 16;
        if (digit == 16) {
            return 0;
        }
        code = code * 16 + digit;
    }
    if (code == 0 || (code >= 0xd800 && code <= 0xdfff)) {
        return 0;
    }
    if (code < 0x80) {
        out[0] = (char)code;
        return 1;
    }
    if (code < 0x800) {
        out[0] = (char)(0xc0 | (code >> 6));
        out[1] = (char)(0x80 | (code & 0x3f));
        return 2;
    }
    out[0] = (char)(0xe0 | (code >> 12));
    out[1] = (char)(0x80 | ((code >> 6) & 0x3f));
    out[2] = (char)(0x80 | (code & 0x3f));
    return 3;
}

/* Reads a text in double quotes. Returns NULL if it is not one. */
static const char *read_text(Parse *p)
{
    if (!take(p, '"')) {
        return NULL;
    }
    /* A text is never longer than what stands for it in the file, so
     * what is left of the line is room enough. */
    size_t most = 0;
    while (p->at + most < p->end && p->at[most] != '\n') {
        most++;
    }
    char *text = take_room(p, most + 1);
    size_t length = 0;
    while (text != NULL && p->at < p->end) {
        char c = *p->at++;
        if (c == '"') {
            return text;
        }
        if (is_control(c)) {
            break;
        }
        if (c != '\\') {
            text[length++] = c;
            continue;
        }
        char escaped = p->at < p->end ? *p->at++ : '\0';
        if (escaped == 'u') {
            size_t wrote = read_code(p, text + length);
            if (wrote == 0) {
                break;
            }
            length += wrote;
        } else if (escaped == '"' || escaped == '\\') {
            text[length++] = escaped;
        } else if (escaped == 'n') {
            text[length++] = '\n';
        } else if (escaped == 't') {
            text[length++] = '\t';
        } else if (escaped == 'r') {
            text[length++] = '\r';
        } else {
            break;
        }
    }
    return NULL;
}

static bool is_name_character(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

/* Reads a name, of a key or in a header: bare, or in double quotes.
 * Returns NULL if there is none. */
static const char *read_name(Parse *p)
{
    if (p->at < p->end && *p->at == '"') {
        return read_text(p);
    }
    const char *start = p->at;
    while (p->at < p->end && is_name_character(*p->at)) {
        p->at++;
    }
    size_t length = (size_t)(p->at - start);
    char *name = length == 0 ? NULL : take_room(p, length + 1);
    if (name != NULL) {
        memcpy(name, start, length);
    }
    return name;
}

/* Passes digits and says whether there were any. */
static bool skip_digits(Parse *p)
{
    const char *start = p->at;
    while (p->at < p->end && *p->at >= '0' && *p->at <= '9') {
        p->at++;
    }
    return p->at > start;
}

/* Reads a number into `entry`, as a whole number or as one with a
 * fraction, and sets its kind. */
static bool read_number(Parse *p, Entry *entry)
{
    char text[64];
    const char *start = p->at;
    bool fraction = false;
    if (!take(p, '-')) {
        take(p, '+');
    }
    if (!skip_digits(p)) {
        return false;
    }
    if (take(p, '.')) {
        fraction = true;
        if (!skip_digits(p)) {
            return false;
        }
    }
    if (take(p, 'e') || take(p, 'E')) {
        fraction = true;
        if (!take(p, '-')) {
            take(p, '+');
        }
        if (!skip_digits(p)) {
            return false;
        }
    }
    size_t length = (size_t)(p->at - start);
    if (length >= sizeof text) {
        return false;
    }
    memcpy(text, start, length);
    text[length] = '\0';
    if (fraction) {
        double value = strtod(text, NULL);
        entry->kind = KIND_FLOAT;
        entry->as.number = value;
        /* Too large to hold comes back as one that is not finite. */
        return value >= -DBL_MAX && value <= DBL_MAX;
    }
    /* Ten digits and a sign at most, so that it fits what strtoll()
     * gives. */
    long long value = length > 11 ? LLONG_MAX : strtoll(text, NULL, 10);
    entry->kind = KIND_INT;
    entry->as.whole = (int32_t)value;
    return value >= INT32_MIN && value <= INT32_MAX;
}

/* Reads a list on one line, whose '[' is passed, into `entry`: of
 * texts if its first value is one, and of whole numbers if not. */
static bool read_list(Parse *p, Entry *entry)
{
    /* A list has at most one value more than it has commas. */
    size_t most = 1;
    for (const char *c = p->at; c < p->end && *c != '\n'; c++) {
        most += *c == ',' ? 1 : 0;
    }
    skip_spaces(p);
    bool texts = p->at < p->end && *p->at == '"';
    entry->kind = texts ? KIND_TEXT_LIST : KIND_INT_LIST;
    if (texts) {
        entry->as.texts = take_room(p, most * sizeof *entry->as.texts);
    } else {
        entry->as.ints = take_room(p, most * sizeof *entry->as.ints);
    }
    if (p->no_memory) {
        return false;
    }
    for (;;) {
        skip_spaces(p);
        if (take(p, ']')) {
            return true;
        }
        if (texts) {
            const char *text = read_text(p);
            if (text == NULL) {
                return false;
            }
            entry->as.texts[entry->count++] = text;
        } else {
            Entry number;
            if (!read_number(p, &number) || number.kind != KIND_INT) {
                return false;
            }
            entry->as.ints[entry->count++] = number.as.whole;
        }
        skip_spaces(p);
        /* A comma goes between two values, and may follow the last. */
        if (!take(p, ',') && (p->at >= p->end || *p->at != ']')) {
            return false;
        }
    }
}

/* True if the text `word` is next, and nothing of a name after it.
 * It is then passed. */
static bool take_word(Parse *p, const char *word)
{
    size_t length = strlen(word);
    if ((size_t)(p->end - p->at) < length
        || memcmp(p->at, word, length) != 0
        || (p->at + length < p->end
            && is_name_character(p->at[length]))) {
        return false;
    }
    p->at += length;
    return true;
}

/* Reads the value of a key into `entry`. */
static bool read_value(Parse *p, Entry *entry)
{
    if (p->at < p->end && *p->at == '"') {
        entry->kind = KIND_TEXT;
        entry->as.text = read_text(p);
        return entry->as.text != NULL;
    }
    if (take(p, '[')) {
        return read_list(p, entry);
    }
    if (take_word(p, "true") || take_word(p, "false")) {
        entry->kind = KIND_BOOL;
        entry->as.yes = p->at[-2] == 'u';
        return true;
    }
    return read_number(p, entry);
}

/* True if nothing but spaces and a comment is left of the line, which
 * is then passed with its end. */
static bool end_line(Parse *p)
{
    skip_spaces(p);
    if (take(p, '#')) {
        while (p->at < p->end && *p->at != '\n' && *p->at != '\r') {
            if (is_control(*p->at)) {
                return false;
            }
            p->at++;
        }
    }
    take(p, '\r');
    return p->at >= p->end || take(p, '\n');
}

/* Reads a header, whose first '[' is passed, and gives the table that
 * the lines after it belong to, or NULL. */
static TomlTable *read_header(Parse *p, TomlTable *top)
{
    bool list = take(p, '[');
    TomlTable *table = top;
    bool last = false;
    while (!last) {
        skip_spaces(p);
        const char *name = read_name(p);
        if (name == NULL) {
            return NULL;
        }
        skip_spaces(p);
        last = !take(p, '.');
        /* A list of tables has one name. */
        if (list && !last) {
            return NULL;
        }
        Kind kind = list ? KIND_TABLE_LIST : KIND_TABLE;
        Entry *entry = find(table, name);
        if (entry == NULL) {
            entry = add(p, table, name, kind);
        } else if (entry->kind != kind) {
            return NULL;
        }
        /* A table is made the first time it is named, and for a list
         * each time. */
        TomlTable *made = entry == NULL || (!list && entry->count > 0)
            ? NULL : take_room(p, sizeof *made);
        if (p->no_memory) {
            return NULL;
        }
        if (made != NULL) {
            TomlTable **end = &entry->as.table;
            while (*end != NULL) {
                end = &(*end)->next;
            }
            *end = made;
            entry->count++;
            table = made;
        } else {
            table = entry->as.table;
        }
    }
    if (!take(p, ']') || (list && !take(p, ']'))
        || (!list && table->named)) {
        return NULL;
    }
    table->named = true;
    return table;
}

/* Reads one line. `*table` is the table its key goes into, which a
 * header changes. */
static bool read_line(Parse *p, TomlTable *top, TomlTable **table)
{
    skip_spaces(p);
    if (take(p, '[')) {
        *table = read_header(p, top);
        return *table != NULL && end_line(p);
    }
    if (p->at < p->end && *p->at != '#' && *p->at != '\n'
        && *p->at != '\r') {
        const char *key = read_name(p);
        skip_spaces(p);
        if (key == NULL || find(*table, key) != NULL || !take(p, '=')) {
            return false;
        }
        skip_spaces(p);
        Entry *entry = add(p, *table, key, KIND_INT);
        if (entry == NULL || !read_value(p, entry)) {
            return false;
        }
    }
    return end_line(p);
}

bool toml_read(const uint8_t *data, size_t size, TomlTable **out,
    size_t *error_line)
{
    Block *blocks = NULL;
    Parse p;
    p.at = (const char *)data;
    p.end = p.at + size;
    p.blocks = &blocks;
    p.no_memory = false;
    Top *top = take_room(&p, sizeof *top);
    TomlTable *table = top == NULL ? NULL : &top->table;
    size_t line = 0;
    bool ok = top != NULL;
    while (ok && p.at < p.end) {
        line++;
        ok = read_line(&p, &top->table, &table) && !p.no_memory;
    }
    if (!ok) {
        free_blocks(blocks);
        if (error_line != NULL) {
            *error_line = p.no_memory ? 0 : line;
        }
        return false;
    }
    top->blocks = blocks;
    *out = &top->table;
    return true;
}

bool toml_get_int(const TomlTable *table, const char *key, int32_t *out)
{
    const Entry *entry = find_kind(table, key, KIND_INT);
    if (entry != NULL) {
        *out = entry->as.whole;
    }
    return entry != NULL;
}

bool toml_get_bool(const TomlTable *table, const char *key, bool *out)
{
    const Entry *entry = find_kind(table, key, KIND_BOOL);
    if (entry != NULL) {
        *out = entry->as.yes;
    }
    return entry != NULL;
}

bool toml_get_float(const TomlTable *table, const char *key, double *out)
{
    const Entry *entry = find(table, key);
    if (entry != NULL && entry->kind == KIND_INT) {
        *out = entry->as.whole;
        return true;
    }
    if (entry != NULL && entry->kind == KIND_FLOAT) {
        *out = entry->as.number;
        return true;
    }
    return false;
}

bool toml_get_text(const TomlTable *table, const char *key,
    const char **out)
{
    const Entry *entry = find_kind(table, key, KIND_TEXT);
    if (entry != NULL) {
        *out = entry->as.text;
    }
    return entry != NULL;
}

/* A list with nothing in it has no kind of its own: it is read as one
 * of whole numbers, and is as good an empty list of texts. */
static const Entry *find_list(const TomlTable *table, const char *key,
                              Kind kind)
{
    const Entry *entry = find(table, key);
    return entry != NULL && (entry->kind == kind
            || (entry->kind == KIND_INT_LIST && entry->count == 0))
        ? entry : NULL;
}

bool toml_get_int_list(const TomlTable *table, const char *key,
    const int32_t **out_values, size_t *out_count)
{
    const Entry *entry = find_list(table, key, KIND_INT_LIST);
    if (entry != NULL) {
        *out_values = entry->as.ints;
        *out_count = entry->count;
    }
    return entry != NULL;
}

bool toml_get_text_list(const TomlTable *table, const char *key,
    const char *const **out_texts, size_t *out_count)
{
    const Entry *entry = find_list(table, key, KIND_TEXT_LIST);
    if (entry != NULL) {
        *out_texts = entry->as.texts;
        *out_count = entry->count;
    }
    return entry != NULL;
}

const TomlTable *toml_get_table(const TomlTable *table, const char *key)
{
    const Entry *entry = find_kind(table, key, KIND_TABLE);
    return entry == NULL ? NULL : entry->as.table;
}

size_t toml_get_table_count(const TomlTable *table, const char *key)
{
    const Entry *entry = find_kind(table, key, KIND_TABLE_LIST);
    return entry == NULL ? 0 : entry->count;
}

const TomlTable *toml_get_table_at(const TomlTable *table, const char *key,
    size_t index)
{
    const Entry *entry = find_kind(table, key, KIND_TABLE_LIST);
    const TomlTable *at = entry == NULL ? NULL : entry->as.table;
    while (at != NULL && index > 0) {
        at = at->next;
        index--;
    }
    return at;
}
