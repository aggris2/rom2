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

/* Lists what is in a folder: the names of its files and of the folders
 * in it, without "." and "..", in no order that can be relied on.
 *
 * On success `*out_names` is one block the caller owns and must free():
 * the names one after the other, each closed by a zero, `*out_count` of
 * them. An empty folder gives a block with nothing in it and a count
 * of 0.
 *
 * Returns false, and leaves the out-pointers alone, if the folder
 * cannot be read or memory runs out. */
bool file_list_folder(const char *path, char **out_names,
    size_t *out_count);

#endif
