/* The maps, sounds, texts and videos steps of rom2_import: the files of
 * the original game that ROM2 takes as they are.
 *
 *     maps      scenario.res  *.alm          to  maps/
 *     sounds    sfx.res       *.wav          to  sounds/effects/
 *               music.res     *.wav          to  sounds/music/
 *               speech.res    *.wav          to  sounds/speech/
 *     texts     main.res      text/ *.txt    to  texts/
 *     videos    video.res     *.smk          to  videos/
 *
 * Each file keeps its name and the folders it is in. The maps, the
 * sounds and the videos are copied byte for byte. The texts are written
 * as UTF-8 with plain line ends: the original has them in the Windows
 * code page 1251, with a carriage return before each line feed.
 */
#include "rom2_import/steps.h"

#include <stdio.h>

#include "base/buffer.h"
#include "base/file.h"
#include "import/text.h"
#include "rom2_import/tree.h"

/* Writes a text of the original as UTF-8. */
static bool text_write(const uint8_t *data, size_t size, const char *path)
{
    ByteBuffer text = {0};
    bool ok = text_cp1251_to_utf8(data, size, &text)
        && file_write_all(path, text.data, text.size);
    buffer_free(&text);
    return ok;
}

/* Makes the folder `name` of the content folder and writes its path to
 * `out`, which holds `size` characters. Prints why if it cannot. */
static bool step_folder(const char *step, const char *content_folder,
    const char *name, char *out, size_t size)
{
    int written = snprintf(out, size, "%s/%s", content_folder, name);
    if (written < 0 || (size_t)written >= size
        || !file_make_folder(content_folder) || !file_make_folder(out)) {
        fprintf(stderr, "%s: cannot make the folder %s/%s\n", step,
            content_folder, name);
        return false;
    }
    return true;
}

/* Says that an archive a step needs is not there. */
static int no_archive(const char *step, const char *archive)
{
    fprintf(stderr, "%s: %s is not in the game folder\n", step, archive);
    return -1;
}

int import_maps(const ResArchive *scenario, const char *content_folder)
{
    char folder[1000];
    if (scenario == NULL) {
        return no_archive("maps", "scenario.res");
    }
    if (!step_folder("maps", content_folder, "maps", folder,
            sizeof folder)) {
        return -1;
    }
    TreeCount total = {0, 0};
    tree_write(scenario, NULL, folder, ".alm", ".alm", tree_copy, &total);
    printf("%s\n%d maps written, %d left out\n", folder, total.written,
        total.failed);
    return total.failed;
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
    printf("%d sounds written, %d left out\n", total.written,
        total.failed);
    return missing == 3 ? -1 : total.failed + missing;
}

int import_texts(const ResArchive *main_archive, const char *content_folder)
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
    tree_write(main_archive, "text", folder, ".txt", ".txt", text_write,
        &total);
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
    tree_write(video, NULL, folder, ".smk", ".smk", tree_copy, &total);
    printf("%s\n%d videos written, %d left out\n", folder, total.written,
        total.failed);
    return total.failed;
}
