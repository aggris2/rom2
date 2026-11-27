/* The campaign, maps, sounds, texts, videos and rules steps of
 * rom2_import: the files of the original game that ROM2 takes as they
 * are, or nearly.
 *
 *     campaign  scenario.res  N.alm             to  campaign/missions/N/
 *               main.res      text/missionN.txt
 *     maps      the game folder  *.alm       to  maps/
 *     sounds    sfx.res       *.wav          to  sounds/effects/
 *               sfx.res       sfx.reg        to  sounds/effects/sounds.toml
 *               music.res     *.wav          to  sounds/music/
 *               speech.res    *.wav          to  sounds/speech/
 *     texts     main.res      text/ *.txt    to  texts/
 *               patch.res     *.txt          to  texts/
 *     videos    video.res     *.smk, *.reg   to  videos/
 *     rules     world.res     data/ai.reg    to  rules/ai.toml
 *               world.res     data/map.reg   to  rules/map.toml
 *               scenario.res  npc.reg        to  rules/npc.toml
 *
 * A mission is a map and a text with one number. Each gets a folder
 * named after the number, written with three digits, with map.alm and
 * mission.txt in it. The maps that lie loose in the game folder, which
 * are the ones played by several players, go to maps/ with their names
 * in small letters.
 *
 * The sounds, the texts and the videos keep their names and the folders
 * they are in. The maps, the sounds and the videos are copied byte for
 * byte. The texts are written as UTF-8 with plain line ends: the
 * original has them in the Windows code page 1251, with a carriage
 * return before each line feed. The texts of the missions are left out
 * of texts/, since the campaign has them.
 *
 * A registry becomes a TOML file with the same sections and values
 * under the original's names: one beside each video, which says how it
 * fades, and the three of the rules. The registry of the sounds is
 * written as a list instead: the number the game knows each sound by,
 * and its file.
 */
#include "import/steps/steps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/buffer.h"
#include "base/file.h"
#include "import/reg.h"
#include "import/reg_toml.h"
#include "import/text.h"
#include "base/toml.h"
#include "import/steps/shared.h"

/* Writes a text of the original as UTF-8. */
static bool text_write(const uint8_t *data, size_t size, const char *path)
{
    ByteBuffer text = {0};
    bool ok = text_cp1251_to_utf8(data, size, &text)
        && file_write_all(path, text.data, text.size);
    buffer_free(&text);
    return ok;
}

/* Writes a registry of the original as a TOML file with the same
 * sections and values. */
static bool reg_write(const uint8_t *data, size_t size, const char *path)
{
    Reg reg;
    ByteBuffer text = {0};
    bool ok = reg_open(data, size, &reg)
        && toml_comment(&text, "From a registry of the original, under "
            "the names it has there.")
        && reg_toml_write(&text, &reg)
        && file_write_all(path, text.data, text.size);
    buffer_free(&text);
    return ok;
}

/* Says that an archive a step needs is not there. */
static int no_archive(const char *step, const char *archive)
{
    fprintf(stderr, "%s: %s is not in the game folder\n", step, archive);
    return -1;
}

/* Reads the number of a mission from a name such as "10.alm": digits
 * and then `ending`, with nothing else. Returns -1 for any other name. */
static int mission_number(const char *name, const char *ending)
{
    int number = 0;
    int digits = 0;
    while (name[digits] >= '0' && name[digits] <= '9' && digits < 6) {
        number = number * 10 + (name[digits] - '0');
        digits++;
    }
    return digits > 0 && strcmp(name + digits, ending) == 0 ? number : -1;
}

int import_campaign(const ResArchive *scenario,
    const ResArchive *main_archive, const char *content_folder)
{
    char campaign[1000];
    char missions[1040];
    if (scenario == NULL) {
        return no_archive("campaign", "scenario.res");
    }
    if (main_archive == NULL) {
        return no_archive("campaign", "main.res");
    }
    if (!step_folder("campaign", content_folder, "campaign", campaign,
            sizeof campaign)
        || !step_folder("campaign", campaign, "missions", missions,
            sizeof missions)) {
        return -1;
    }

    /* The missions are the maps in the root of scenario.res. */
    int written = 0;
    int failed = 0;
    for (uint32_t i = 0; i < scenario->header.root_count; i++) {
        ResEntry map;
        ResEntry text;
        char folder[1100];
        char path[1150];
        char text_path[64];
        uint32_t index = scenario->header.root_first + i;
        if (!res_entry_at(scenario, index, &map) || map.is_folder) {
            continue;
        }
        int number = mission_number(map.name, ".alm");
        if (number < 0) {
            continue;
        }
        snprintf(folder, sizeof folder, "%s/%03d", missions, number);
        snprintf(path, sizeof path, "%s/map.alm", folder);
        snprintf(text_path, sizeof text_path, "text/mission%d.txt", number);
        bool ok = file_make_folder(folder)
            && file_write_all(path, scenario->data + map.offset, map.size);
        if (ok) {
            res_note(scenario, index);
        }
        snprintf(path, sizeof path, "%s/mission.txt", folder);
        ok = ok && res_find(main_archive, text_path, &text)
            && !text.is_folder
            && text_write(main_archive->data + text.offset, text.size,
                path);
        if (ok) {
            written++;
        } else {
            fprintf(stderr, "  mission %d left out: its map or its text "
                "could not be written\n", number);
            failed++;
        }
    }
    printf("%s\n%d missions written, %d left out\n", missions, written,
        failed);
    return failed;
}

/* Orders two names for qsort. */
static int by_name(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* Says whether `name` ends in ".alm", in small or capital letters. */
static bool is_map(const char *name)
{
    size_t length = strlen(name);
    if (length <= 4 || name[length - 4] != '.') {
        return false;
    }
    const char *end = name + length - 3;
    return (end[0] == 'a' || end[0] == 'A') && (end[1] == 'l'
        || end[1] == 'L') && (end[2] == 'm' || end[2] == 'M');
}

int import_maps(const char *game_folder, const char *content_folder)
{
    char folder[1000];
    char *names = NULL;
    size_t count = 0;
    if (!file_list_folder(game_folder, &names, &count)) {
        fprintf(stderr, "maps: cannot read the folder %s\n", game_folder);
        return -1;
    }
    if (!step_folder("maps", content_folder, "maps", folder,
            sizeof folder)) {
        free(names);
        return -1;
    }

    /* A folder is listed in no order, so the names are sorted: a run
     * then says the same thing every time. */
    const char **sorted = malloc((count + 1) * sizeof *sorted);
    if (sorted == NULL) {
        free(names);
        return -1;
    }
    size_t maps = 0;
    const char *name = names;
    for (size_t i = 0; i < count; i++) {
        if (is_map(name)) {
            sorted[maps++] = name;
        }
        name += strlen(name) + 1;
    }
    qsort(sorted, maps, sizeof *sorted, by_name);

    int written = 0;
    int failed = 0;
    for (size_t i = 0; i < maps; i++) {
        char from[1300];
        char to[1300];
        uint8_t *data = NULL;
        size_t size = 0;
        int a = snprintf(from, sizeof from, "%s/%s", game_folder,
            sorted[i]);
        int b = snprintf(to, sizeof to, "%s/%s", folder, sorted[i]);
        bool ok = a > 0 && (size_t)a < sizeof from && b > 0
            && (size_t)b < sizeof to;
        /* The name in small letters: "CROSS2.ALM" becomes "cross2.alm". */
        for (char *c = to + strlen(folder); ok && *c != '\0'; c++) {
            if (*c >= 'A' && *c <= 'Z') {
                *c = (char)(*c - 'A' + 'a');
            }
        }
        ok = ok && file_read_all(from, &data, &size)
            && file_write_all(to, data, size);
        free(data);
        if (ok) {
            written++;
        } else {
            fprintf(stderr, "  %s left out: it could not be copied\n",
                sorted[i]);
            failed++;
        }
    }
    free(sorted);
    free(names);
    printf("%s\n%d maps written, %d left out\n", folder, written, failed);
    return failed;
}

/* Writes sounds.toml from the registry of the sounds: for each sound
 * the number the game knows it by and its file. The registry has them
 * as "Sfx100" = "units\sword", in a section "Sfx". */
static bool sound_list_write(const ResArchive *sfx, const char *folder)
{
    ResEntry file;
    Reg reg;
    RegEntry section;
    char path[1100];
    int written = snprintf(path, sizeof path, "%s/effects/sounds.toml",
        folder);
    if (written < 0 || (size_t)written >= sizeof path
        || !res_find(sfx, "sfx.reg", &file) || file.is_folder
        || !reg_open(sfx->data + file.offset, file.size, &reg)) {
        return false;
    }
    ByteBuffer text = {0};
    bool ok = toml_comment(&text, "The sound effects by the number the "
        "game knows each by. The files are in this folder.");
    bool found = false;
    for (uint32_t i = 0; ok && i < reg.root_count; i++) {
        if (!reg_entry_at(&reg, reg.root_first + i, &section)
            || section.type != REG_SECTION
            || strcmp(section.name, "Sfx") != 0) {
            continue;
        }
        found = true;
        for (uint32_t k = 0; ok && k < section.b; k++) {
            RegEntry entry;
            const char *name = NULL;
            char wav[300];
            ok = reg_entry_at(&reg, section.a + k, &entry)
                && strncmp(entry.name, "Sfx", 3) == 0
                && mission_number(entry.name + 3, "") >= 0
                && reg_text(&reg, "Sfx", entry.name, &name);
            if (!ok) {
                break;
            }
            written = snprintf(wav, sizeof wav, "%s.wav", name);
            ok = written > 0 && (size_t)written < sizeof wav;
            for (char *c = wav; *c != '\0'; c++) {
                if (*c == '\\') {
                    *c = '/';
                }
            }
            ok = ok && toml_table_list(&text, "sound")
                && toml_int(&text, "id",
                    mission_number(entry.name + 3, ""))
                && toml_text(&text, "file", wav);
        }
    }
    ok = ok && found && file_write_all(path, text.data, text.size);
    buffer_free(&text);
    return ok;
}

int import_sounds(const ResArchive *sfx, const ResArchive *music,
    const ResArchive *speech, const char *content_folder)
{
    char folder[1000];
    if (!step_folder("sounds", content_folder, "sounds", folder,
            sizeof folder)) {
        return -1;
    }
    const ResArchive *archives[3] = {sfx, music, speech};
    static const char *const NAMES[3] = {"sfx.res", "music.res",
        "speech.res"};
    static const char *const FOLDERS[3] = {"effects", "music", "speech"};
    TreeCount total = {0, 0};
    int missing = 0;
    printf("%s\n", folder);
    for (int i = 0; i < 3; i++) {
        if (archives[i] == NULL) {
            no_archive("sounds", NAMES[i]);
            missing++;
            continue;
        }
        int before = total.written;
        tree_write_in(archives[i], NULL, folder, FOLDERS[i], ".wav",
            ".wav", tree_copy, &total);
        printf("  %s, %d sounds\n", FOLDERS[i], total.written - before);
    }
    if (sfx != NULL && !sound_list_write(sfx, folder)) {
        fprintf(stderr, "  sounds.toml left out: the registry of the "
            "sounds could not be read or written\n");
        total.failed++;
    }
    printf("%d sounds written, %d left out\n", total.written,
        total.failed);
    return missing == 3 ? -1 : total.failed + missing;
}

/* Says whether a text is that of a mission, such as "mission10.txt".
 * The campaign step writes those. */
static bool mission_text(const char *name)
{
    return strncmp(name, "mission", 7) == 0
        && mission_number(name + 7, ".txt") >= 0;
}

int import_texts(const ResArchive *main_archive, const ResArchive *patch,
    const char *content_folder)
{
    char folder[1000];
    if (main_archive == NULL) {
        return no_archive("texts", "main.res");
    }
    if (!step_folder("texts", content_folder, "texts", folder,
            sizeof folder)) {
        return -1;
    }
    TreeCount total = {0, 0};
    tree_write_but(main_archive, "text", folder, ".txt", ".txt",
        text_write, mission_text, &total);
    /* patch.res has one more text: what the game says when the server
     * does not answer, and the like. */
    if (patch != NULL) {
        tree_write(patch, NULL, folder, ".txt", ".txt", text_write, &total);
    }
    printf("%s\n%d texts written, %d left out\n", folder, total.written,
        total.failed);
    return total.failed;
}

int import_videos(const ResArchive *video, const char *content_folder)
{
    char folder[1000];
    if (video == NULL) {
        return no_archive("videos", "video.res");
    }
    if (!step_folder("videos", content_folder, "videos", folder,
            sizeof folder)) {
        return -1;
    }
    TreeCount total = {0, 0};
    TreeCount fades = {0, 0};
    tree_write(video, NULL, folder, ".smk", ".smk", tree_copy, &total);
    tree_write(video, NULL, folder, ".reg", ".toml", reg_write, &fades);
    printf("%s\n%d videos written, %d left out\n", folder, total.written,
        total.failed + fades.failed);
    return total.failed + fades.failed;
}

/* Writes the registry at `path` in the archive as rules/<name>. */
static bool rule_write(const ResArchive *archive, const char *path,
    const char *folder, const char *name)
{
    ResEntry entry;
    char to[1100];
    int written = snprintf(to, sizeof to, "%s/%s", folder, name);
    return written > 0 && (size_t)written < sizeof to
        && res_find(archive, path, &entry) && !entry.is_folder
        && reg_write(archive->data + entry.offset, entry.size, to);
}

int import_rules(const ResArchive *world, const ResArchive *scenario,
    const char *content_folder)
{
    char folder[1000];
    if (!step_folder("rules", content_folder, "rules", folder,
            sizeof folder)) {
        return -1;
    }
    int written = 0;
    int failed = 0;
    if (rule_write(world, "data/ai.reg", folder, "ai.toml")) {
        written++;
    } else {
        failed++;
    }
    if (rule_write(world, "data/map.reg", folder, "map.toml")) {
        written++;
    } else {
        failed++;
    }
    if (scenario == NULL) {
        no_archive("rules", "scenario.res");
        failed++;
    } else if (rule_write(scenario, "npc.reg", folder, "npc.toml")) {
        written++;
    } else {
        failed++;
    }
    printf("%s\n%d files of rules written, %d left out\n", folder, written,
        failed);
    return failed;
}
