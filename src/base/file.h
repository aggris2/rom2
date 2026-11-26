/* Reading and writing whole files on disk.
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

/* Writes `size` bytes to a file, replacing the file if it exists. `data`
 * may be NULL when `size` is 0, which makes an empty file.
 *
 * Returns false if the file cannot be created or not every byte could be
 * written. A file may then be left behind with only part of the bytes. */
bool file_write_all(const char *path, const uint8_t *data, size_t size);

/* Makes a folder. The folder it is to be in must exist already.
 *
 * Returns true if the folder was made or was there already, and false if
 * it could not be made. */
bool file_make_folder(const char *path);

#endif
