/* The names the original game shows for its items.
 *
 * Three files give them. data/itemname.bin in world.res is a number of
 * 16 bits for each item, low byte first: the item's material in its top
 * 4 bits, then 4 bits of its slot, 3 of its class and 5 of its place in
 * its table. text/itemserv.txt and text/itemname.txt in main.res have a
 * line for each of those numbers, in the same order: the first the name
 * the server knows the item by, such as "Common Bronze Bracers" or
 * "Book Air", the second the name the player sees, "Bronze Bracers" or
 * "Book".
 */
#ifndef ROM2_IMPORT_ITEM_NAMES_H
#define ROM2_IMPORT_ITEM_NAMES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The room a name needs, with its closing zero. */
#define ITEM_NAME_SIZE 192

/* The three files, open. Does not own their bytes. */
typedef struct ItemNames {
    const uint8_t *numbers;
    uint32_t count;
    const uint8_t *server;
    size_t server_size;
    const uint8_t *shown;
    size_t shown_size;
} ItemNames;

/* Opens the three files held in memory: the numbers, the names for the
 * server and the names shown.
 *
 * Returns false, and leaves `*out` alone, if the numbers are not a whole
 * count of 16 bits each, or a text has fewer lines than there are
 * numbers. */
bool item_names_open(const uint8_t *numbers, size_t numbers_size,
    const uint8_t *server, size_t server_size, const uint8_t *shown,
    size_t shown_size, ItemNames *out);

/* Writes the name shown for the item with this material, slot, class
 * and place to `out`, as UTF-8.
 *
 * Returns false, and leaves `out` alone, if a number does not fit its
 * bits, no item has that number, or its name does not fit. */
bool item_names_shown(const ItemNames *names, int32_t material,
    int32_t slot, int32_t item_class, int32_t place,
    char out[ITEM_NAME_SIZE]);

/* Writes the name shown for the item the server knows as `server_name`
 * to `out`, as UTF-8. The magic items are found so: their names in
 * data.bin are the server's.
 *
 * Returns false, and leaves `out` alone, if the server knows no item by
 * that name, or the name shown does not fit. */
bool item_names_shown_for(const ItemNames *names, const char *server_name,
    char out[ITEM_NAME_SIZE]);

#endif
