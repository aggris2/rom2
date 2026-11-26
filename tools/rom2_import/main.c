/* rom2_import: writes the original game's files as ROM2 content.
 *
 *     rom2_import <game folder> <content folder> [step [id]]
 *
 * <game folder> is the folder with the original's archives: graphics.res,
 * world.res and the others. Every step is run in turn, and each writes
 * one kind of thing into a folder of the content folder:
 *
 *     units, objects, structures, projectiles, spells, items, terrain,
 *     interface, campaign, maps, sounds, texts, videos
 *
 * Then it says how many of the original's files were read and how many
 * were not, and lists the files not read in not_imported.txt in the
 * content folder.
 *
 * With the name of a step only that step is run, and with an ID as well
 * only the one thing with that ID is written, for the steps that write a
 * folder for each thing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/buffer.h"
#include "base/file.h"
#include "import/res.h"
#include "rom2_import/steps.h"

/* The archives of the original game. */
enum {
    GRAPHICS, MAIN, MOVIES, MUSIC, PATCH, SCENARIO, SFX, SPEECH, VIDEO,
    WORLD, ARCHIVE_COUNT
};
static const char *const ARCHIVE_NAMES[ARCHIVE_COUNT] = {
    "graphics", "main", "movies", "music", "patch", "scenario", "sfx",
    "speech", "video", "world"
};

typedef struct Archives {
    uint8_t *data[ARCHIVE_COUNT];
    uint8_t *found[ARCHIVE_COUNT];
    ResArchive res[ARCHIVE_COUNT];
    bool open[ARCHIVE_COUNT];
} Archives;

static const char *const STEP_NAMES[] = {
    "units", "objects", "structures", "projectiles", "spells", "items",
    "terrain", "interface", "campaign", "maps", "sounds", "texts",
    "videos"
};
#define STEP_COUNT (sizeof STEP_NAMES / sizeof STEP_NAMES[0])

/* Opens every archive the game folder has. One that is not there, or is
 * not an archive, stays closed. */
static void archives_open(Archives *a, const char *game_folder)
{
    memset(a, 0, sizeof *a);
    for (int i = 0; i < ARCHIVE_COUNT; i++) {
        char path[1100];
        size_t size = 0;
        int written = snprintf(path, sizeof path, "%s/%s.res", game_folder,
            ARCHIVE_NAMES[i]);
        if (written < 0 || (size_t)written >= sizeof path
            || !file_read_all(path, &a->data[i], &size)) {
            continue;
        }
        if (!res_open(a->data[i], size, &a->res[i])) {
            fprintf(stderr, "rom2_import: %s is not a .res archive\n", path);
            continue;
        }
        /* One byte for each entry, to note the files that are read. */
        a->found[i] = calloc(a->res[i].header.entry_count + 1u, 1);
        a->res[i].found = a->found[i];
        a->open[i] = true;
    }
}

static void archives_close(Archives *a)
{
    for (int i = 0; i < ARCHIVE_COUNT; i++) {
        free(a->found[i]);
        free(a->data[i]);
    }
    memset(a, 0, sizeof *a);
}

/* Returns an open archive, or NULL. */
static const ResArchive *archive_of(const Archives *a, int which)
{
    return a->open[which] ? &a->res[which] : NULL;
}

/* Runs step number `step`. Returns what the step returns. */
static int step_run(const Archives *a, size_t step,
    const char *game_folder, const char *content, int32_t only)
{
    const ResArchive *graphics = archive_of(a, GRAPHICS);
    const ResArchive *world = archive_of(a, WORLD);
    switch (step) {
    case 0: return import_units(graphics, world, content, only);
    case 1: return import_objects(graphics, content, only);
    case 2: return import_structures(graphics, world, content, only);
    case 3: return import_projectiles(graphics, content, only);
    case 4: return import_spells(world, content, only);
    case 5: return import_items(graphics, world, content);
    case 6: return import_terrain(graphics, content);
    case 7: return import_interface(graphics, archive_of(a, MAIN),
        archive_of(a, MOVIES), content);
    case 8: return import_campaign(archive_of(a, SCENARIO),
        archive_of(a, MAIN), content);
    case 9: return import_maps(game_folder, content);
    case 10: return import_sounds(archive_of(a, SFX), archive_of(a, MUSIC),
        archive_of(a, SPEECH), content);
    case 11: return import_texts(archive_of(a, MAIN), content);
    case 12: return import_videos(archive_of(a, VIDEO), content);
    default: return -1;
    }
}

/* ---- What was read and what was not ---- */

/* The files not read are counted in groups: those of one archive, in
 * one of its top folders, with one ending. */
#define MAX_GROUPS 256
typedef struct Group {
    int archive;
    char folder[20];
    char ending[20];
    uint32_t files;
} Group;

typedef struct Report {
    uint32_t files[ARCHIVE_COUNT];
    uint32_t read[ARCHIVE_COUNT];
    Group groups[MAX_GROUPS];
    size_t group_count;
    ByteBuffer list;
} Report;

/* Counts one file that was not read into its group. */
static void group_add(Report *report, int archive, const char *path)
{
    Group key;
    memset(&key, 0, sizeof key);
    key.archive = archive;
    const char *slash = strchr(path, '/');
    if (slash != NULL) {
        snprintf(key.folder, sizeof key.folder, "%.*s/",
            (int)(slash - path), path);
    }
    const char *name = strrchr(path, '/');
    name = name != NULL ? name + 1 : path;
    const char *dot = strrchr(name, '.');
    snprintf(key.ending, sizeof key.ending, "%s", dot != NULL ? dot : "");

    for (size_t i = 0; i < report->group_count; i++) {
        Group *g = &report->groups[i];
        if (g->archive == archive && strcmp(g->folder, key.folder) == 0
            && strcmp(g->ending, key.ending) == 0) {
            g->files++;
            return;
        }
    }
    if (report->group_count < MAX_GROUPS) {
        key.files = 1;
        report->groups[report->group_count++] = key;
    }
}

/* Goes through a folder of an archive and the folders in it, and counts
 * each file as read or not. */
static void report_folder(Report *report, const Archives *a, int archive,
    uint32_t first, uint32_t count, const char *path, int depth)
{
    for (uint32_t i = 0; i < count && depth < 16; i++) {
        ResEntry entry;
        char inner[600];
        if (!res_entry_at(&a->res[archive], first + i, &entry)) {
            continue;
        }
        /* A path too long to hold is passed over. The original's are
         * far shorter. */
        int length = snprintf(inner, sizeof inner, "%s%s%s", path,
            entry.name, entry.is_folder ? "/" : "");
        if (length < 0 || (size_t)length >= sizeof inner) {
            continue;
        }
        if (entry.is_folder) {
            report_folder(report, a, archive, entry.offset, entry.size,
                inner, depth + 1);
            continue;
        }
        report->files[archive]++;
        if (a->found[archive][first + i] != 0) {
            report->read[archive]++;
            continue;
        }
        group_add(report, archive, inner);
        char line[700];
        length = snprintf(line, sizeof line, "%s.res  %s\n",
            ARCHIVE_NAMES[archive], inner);
        if (length > 0 && (size_t)length < sizeof line) {
            buffer_append(&report->list, (const uint8_t *)line,
                (size_t)length);
        }
    }
}

/* Prints how many files of each archive were read, and the files not
 * read in their groups, and writes the list of them. */
static void report_print(const Archives *a, const char *content)
{
    /* static: too large for the stack to be at ease. */
    static Report report;
    memset(&report, 0, sizeof report);
    uint32_t files = 0;
    uint32_t read = 0;

    printf("\nThe original's files\n  %-10s %6s %6s %8s\n", "archive",
        "files", "read", "not read");
    for (int i = 0; i < ARCHIVE_COUNT; i++) {
        if (!a->open[i]) {
            printf("  %-10s not in the game folder\n", ARCHIVE_NAMES[i]);
            continue;
        }
        report_folder(&report, a, i, a->res[i].header.root_first,
            a->res[i].header.root_count, "", 0);
        printf("  %-10s %6u %6u %8u\n", ARCHIVE_NAMES[i], report.files[i],
            report.read[i], report.files[i] - report.read[i]);
        files += report.files[i];
        read += report.read[i];
    }
    printf("  %-10s %6u %6u %8u\n", "all", files, read, files - read);

    printf("\nNot read yet\n");
    for (size_t i = 0; i < report.group_count; i++) {
        const Group *g = &report.groups[i];
        printf("  %-10s %-18s %-8s %6u\n", ARCHIVE_NAMES[g->archive],
            g->folder, g->ending, g->files);
    }

    char path[1100];
    int written = snprintf(path, sizeof path, "%s/not_imported.txt",
        content);
    if (written > 0 && (size_t)written < sizeof path
        && file_write_all(path, report.list.data, report.list.size)) {
        printf("\nEach of them is listed in %s\n", path);
    }
    buffer_free(&report.list);
}

int main(int argc, char **argv)
{
    if (argc < 3 || argc > 5) {
        fprintf(stderr, "usage: rom2_import <game folder> "
            "<content folder> [step [id]]\nsteps:");
        for (size_t i = 0; i < STEP_COUNT; i++) {
            fprintf(stderr, " %s", STEP_NAMES[i]);
        }
        fprintf(stderr, "\n");
        return 2;
    }
    const char *content = argv[2];
    size_t one_step = STEP_COUNT;
    if (argc >= 4) {
        for (size_t i = 0; i < STEP_COUNT; i++) {
            if (strcmp(argv[3], STEP_NAMES[i]) == 0) {
                one_step = i;
            }
        }
        if (one_step == STEP_COUNT) {
            fprintf(stderr, "rom2_import: there is no step \"%s\"\n",
                argv[3]);
            return 2;
        }
    }
    int32_t only = argc == 5 ? atoi(argv[4]) : -1;

    /* static: too large for the stack to be at ease. */
    static Archives archives;
    archives_open(&archives, argv[1]);
    if (!archives.open[GRAPHICS] || !archives.open[WORLD]) {
        fprintf(stderr, "rom2_import: %s has no graphics.res and "
            "world.res\n", argv[1]);
        archives_close(&archives);
        return 1;
    }

    /* Asking for one thing and not getting it is a failure. Leaving
     * some things out of a whole step is reported and is not. */
    bool failed = false;
    for (size_t step = 0; step < STEP_COUNT; step++) {
        if (one_step != STEP_COUNT && step != one_step) {
            continue;
        }
        /* The files a step reads are noted with its number. */
        for (int i = 0; i < ARCHIVE_COUNT; i++) {
            archives.res[i].mark = (uint8_t)(step + 1);
        }
        printf("\n%s\n", STEP_NAMES[step]);
        int left_out = step_run(&archives, step, argv[1], content, only);
        if (left_out < 0 || (only >= 0 && left_out > 0)) {
            failed = true;
        }
    }
    if (one_step == STEP_COUNT) {
        report_print(&archives, content);
    }
    archives_close(&archives);
    return failed ? 1 : 0;
}
