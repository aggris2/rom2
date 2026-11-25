/* Reading whole files from disk.
 *
 * This is the only code in base that touches the file system. Everything
 * else works on bytes that are already in memory, so it can be tested
 * without a file.
 */
#ifndef ROM2_BASE_FILE_H
#define ROM2_BASE_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Reads a whole file into a new block of memory. On success stores the
 * block and its size through the out-pointers and returns true. The caller
 * owns the block and must free() it.
 *
 * Returns false, and leaves the out-pointers alone, if the file cannot be
 * opened or read. */
bool file_read_all(const char *path, uint8_t **out_data, size_t *out_size);

#endif
