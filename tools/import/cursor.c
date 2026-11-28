#include "import/cursor.h"

#include <stddef.h>
#include <string.h>

/* The folder every cursor is in. */
#define CURSORS "/cursors/"

static const struct {
    const char *name;   /* its files in the folder, without an ending */
    int32_t x;
    int32_t y;
} TIPS[] = {
    {"default/sprites", 4, 4},
    {"move/sprites", 2, 3},
    {"swarm/sprites", 2, 3},
    {"attack/sprites", 3, 3},
    {"defend/sprites", 16, 16},
    {"select/sprites", 3, 3},
    {"patrol/sprites", 8, 25},
    {"cast/sprites", 16, 16},
    {"pickup/sprites", 16, 12},
    /* The eight that show which way the map scrolls: from the one
     * pointing up, round by the right. */
    {"arrow0/sprites", 16, 6},
    {"arrow1/sprites", 23, 9},
    {"arrow2/sprites", 26, 16},
    {"arrow3/sprites", 23, 24},
    {"arrow4/sprites", 16, 28},
    {"arrow5/sprites", 8, 24},
    {"arrow6/sprites", 5, 16},
    {"arrow7/sprites", 8, 9},
    {"sdefault/sprites", 1, 1},
    {"smove", 0, 0},
    {"sattack", 0, 0},
    {"sdefend", 0, 0},
    {"spatrol", 0, 0},
    {"scast", 0, 0},
    {"cantput/sprites", 38, 36},
    {"town/sprites", 16, 16},
    {"dice/sprites", 16, 16},
    {"wait/sprites", 16, 16},
    {"backpack/sprites", 16, 16}
};

bool cursor_tip(const char *base, int32_t out[2])
{
    /* The last folder of that name in the path. */
    const char *name = NULL;
    for (const char *at = strstr(base, CURSORS); at != NULL;
        at = strstr(at + 1, CURSORS)) {
        name = at + strlen(CURSORS);
    }
    if (name == NULL) {
        return false;
    }
    for (size_t i = 0; i < sizeof TIPS / sizeof TIPS[0]; i++) {
        if (strcmp(name, TIPS[i].name) == 0) {
            out[0] = TIPS[i].x;
            out[1] = TIPS[i].y;
            return true;
        }
    }
    return false;
}
