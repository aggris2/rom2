/* import_spells: writes the spells of the original game as ROM2
 * content.
 *
 *     import_spells <world.res> <content folder> [id]
 *
 * Every spell gets a folder of its own in <content folder>/spells, named
 * after its number and its name, with its definition in it, spell.toml.
 * The spells come from data.bin in world.res.
 *
 * With a spell number only that spell is written.
 */
#include <stdio.h>
#include <stdlib.h>

#include "base/buffer.h"
#include "base/file.h"
#include "import/databin.h"
#include "import/res.h"
#include "import/spell_toml.h"

/* Writes the folder of one spell. Returns a message saying what went
 * wrong, or NULL. */
static const char *spell_write(const DataBin *bin, int32_t id,
    const DataBinRow *row, const char *spells_folder)
{
    char name[NAME_FOLDER_SIZE];
    char folder[1024];
    char path[1048];
    if (!spell_toml_folder_name(id, row, name)) {
        return "the spell's number does not make a folder name";
    }
    int written = snprintf(folder, sizeof folder, "%s/%s", spells_folder,
        name);
    if (written < 0 || (size_t)written >= sizeof folder) {
        return "the name of the content folder is too long";
    }
    snprintf(path, sizeof path, "%s/spell.toml", folder);

    const char *problem = NULL;
    ByteBuffer toml = {0};
    if (!spell_toml_write(&toml, bin, id, row)) {
        problem = "the spell's values could not be written out";
    } else if (!file_make_folder(folder)
        || !file_write_all(path, toml.data, toml.size)) {
        problem = "the spell's folder could not be written";
    }
    buffer_free(&toml);
    if (problem == NULL) {
        printf("  %s\n", name);
    }
    return problem;
}

/* Does the work once the archive is in memory. `only` is the one spell
 * to write, or -1 for all. Returns how many spells could not be written,
 * or -1 if nothing could be done at all. */
static int import_spells(const uint8_t *data, size_t size,
    const char *content_folder, int32_t only)
{
    /* static: a row is too large for the stack to be at ease. */
    static DataBinRow row;
    ResArchive archive;
    ResEntry entry;
    DataBin bin;
    if (!res_open(data, size, &archive)
        || !res_find(&archive, "data/data.bin", &entry) || entry.is_folder
        || !databin_open(archive.data + entry.offset, entry.size, &bin)) {
        fprintf(stderr, "import_spells: no usable data/data.bin in the "
            "archive\n");
        return -1;
    }

    char spells_folder[1000];
    int written = snprintf(spells_folder, sizeof spells_folder, "%s/spells",
        content_folder);
    if (written < 0 || (size_t)written >= sizeof spells_folder
        || !file_make_folder(content_folder)
        || !file_make_folder(spells_folder)) {
        fprintf(stderr, "import_spells: cannot make the folder %s/spells\n",
            content_folder);
        return -1;
    }

    /* A spell's number is the place of its row, counted from 1. A row
     * without numbers is an empty place in the table. */
    int done = 0;
    int failed = 0;
    int32_t id = 0;
    printf("%s\n", spells_folder);
    DataBinRows rows = databin_rows(&bin, DATABIN_SPELLS);
    while (databin_next(&rows, &row)) {
        id++;
        if (row.number_count == 0 || (only >= 0 && id != only)) {
            continue;
        }
        const char *problem = spell_write(&bin, id, &row, spells_folder);
        if (problem == NULL) {
            done++;
        } else {
            fprintf(stderr, "  spell %d \"%s\" left out: %s\n", (int)id,
                row.name, problem);
            failed++;
        }
    }
    if (only >= 0 && done + failed == 0) {
        fprintf(stderr, "import_spells: no spell has the number %d\n",
            (int)only);
        return -1;
    }
    printf("%d spells written, %d left out\n", done, failed);
    return failed;
}

int main(int argc, char **argv)
{
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "usage: import_spells <world.res> "
            "<content folder> [id]\n");
        return 2;
    }

    uint8_t *data = NULL;
    size_t size = 0;
    if (!file_read_all(argv[1], &data, &size)) {
        fprintf(stderr, "import_spells: cannot read %s\n", argv[1]);
        return 1;
    }
    int32_t only = argc == 4 ? atoi(argv[3]) : -1;
    int failed = import_spells(data, size, argv[2], only);
    free(data);
    return failed != 0 ? 1 : 0;
}
