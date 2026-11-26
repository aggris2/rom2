/* Reading the things a registry lists: units, objects and the like.
 *
 * Such a registry has one section for each thing, named after the kind
 * of thing and a position: "Unit0", "Unit1", or "Object0", "Object1".
 * The number in the name is only a position. A thing is known by its
 * "ID", which is the number maps use for it. The section "Global" says
 * how many things there are.
 *
 * A thing may name a "Parent", the ID of another thing. A value the
 * thing does not state itself is then taken from its parent, and so on
 * up.
 */
#ifndef ROM2_IMPORT_THINGS_H
#define ROM2_IMPORT_THINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "import/reg.h"

/* The room a section name needs: a prefix of up to 16 characters, up to
 * ten digits and the closing zero. */
#define THINGS_SECTION_SIZE 32

/* A parent's parent's parent, and so on: no further than this. The
 * original game's things go two steps at most. A longer chain means a
 * damaged registry, or things that name each other in a circle. */
#define THINGS_MAX_PARENTS 8

/* One kind of thing in one registry. */
typedef struct Things {
    const Reg *reg;
    /* The name in "Global" of the number of things: "UnitCount". */
    const char *count_key;
    /* What a thing's section name starts with: "Unit". */
    const char *prefix;
} Things;

/* Reads the number of things. Returns false, and leaves `*out` alone, if
 * the registry does not say or the number is negative. */
bool things_count(const Things *t, uint32_t *out);

/* Reads the ID of the thing at a position, which is counted from 0.
 * Returns false, and leaves `*out` alone, if there is no such section or
 * it has no ID. */
bool things_id_at(const Things *t, uint32_t position, int32_t *out);

/* Finds the section of the thing with the given ID and writes its name,
 * such as "Unit8", to `out`.
 *
 * Returns false, and leaves `out` alone, if no thing has that ID. */
bool things_section(const Things *t, int32_t id,
    char out[THINGS_SECTION_SIZE]);

/* Says whether the thing with the given ID, or one of its parents,
 * states a value called `key`, of any kind. */
bool things_has(const Things *t, int32_t id, const char *key);

/* Reads the whole number `key` of the thing with the given ID, from the
 * thing or the nearest parent that states it.
 *
 * Returns false, and leaves `*out` alone, if no thing has that ID, or
 * neither the thing nor any parent states the value. */
bool things_int(const Things *t, int32_t id, const char *key, int32_t *out);

/* Reads a text the same way. `*out` is set to point at the text inside
 * the registry's own bytes. */
bool things_text(const Things *t, int32_t id, const char *key,
    const char **out);

/* Reads a list of whole numbers the same way. `out` has room for
 * `capacity` numbers, and `*out_count` is set to how many there are.
 *
 * A list that a thing states but that does not fit `capacity` makes this
 * fail. The parent's list is not used instead. */
bool things_int_list(const Things *t, int32_t id, const char *key,
    int32_t *out, size_t capacity, size_t *out_count);

#endif
