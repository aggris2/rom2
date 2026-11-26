#include "import/projectiles.h"

#include <stdio.h>
#include <string.h>

/* A projectile that is mirrored has the directions from straight ahead
 * to straight back: half the circle and both its ends. */
#define MIRRORED_STEPS (PROJECTILE_STEPS / 2 + 1)
#define DEFAULT_BOX 64

Things projectiles_things(const Reg *reg)
{
    Things t = {reg, "Count", "Projectile"};
    return t;
}

bool projectiles_sprite_path(const Reg *reg, int32_t id, char *out,
    size_t size, bool *out_is_16a)
{
    Things t = projectiles_things(reg);
    char path[256];
    const char *text = NULL;
    int32_t a16 = 0;
    if (!things_text(&t, id, "File", &text)) {
        return false;
    }
    things_int(&t, id, "A16", &a16);
    int written = snprintf(path, sizeof path, "projectiles/%s.%s", text,
        a16 != 0 ? "16a" : "256");
    if (written < 0 || (size_t)written >= sizeof path
        || (size_t)written >= size) {
        return false;
    }
    /* The registry writes some folders with a capital. The archive has
     * every name in lower case. */
    for (char *p = path; *p != '\0'; p++) {
        if (*p == '\\') {
            *p = '/';
        } else if (*p >= 'A' && *p <= 'Z') {
            *p = (char)(*p - 'A' + 'a');
        }
    }
    memcpy(out, path, (size_t)written + 1);
    *out_is_16a = a16 != 0;
    return true;
}

bool projectiles_layout(const Reg *reg, int32_t id, uint32_t frame_count,
    ProjectileLayout *out)
{
    Things t = projectiles_things(reg);
    char section[THINGS_SECTION_SIZE];
    int32_t phases = 0;
    int32_t flip = 0;
    int32_t width = DEFAULT_BOX;
    int32_t height = DEFAULT_BOX;
    if (!things_section(&t, id, section)
        || !things_int(&t, id, "Phases", &phases)
        || phases <= 0 || (uint32_t)phases > PROJECTILE_MAX_PHASES) {
        return false;
    }
    things_int(&t, id, "Flip", &flip);
    things_int(&t, id, "Width", &width);
    things_int(&t, id, "Height", &height);

    int32_t stored = flip != 0 ? (int32_t)MIRRORED_STEPS
        : (int32_t)PROJECTILE_STEPS;
    things_int(&t, id, "RotationPhases", &stored);
    if (width <= 0 || height <= 0
        || (stored != 1 && stored != (int32_t)MIRRORED_STEPS
            && stored != (int32_t)PROJECTILE_STEPS)) {
        return false;
    }
    uint32_t needed = (uint32_t)phases * (uint32_t)stored;
    if (frame_count < needed) {
        return false;
    }

    ProjectileLayout layout;
    layout.phases = (uint32_t)phases;
    layout.stored_steps = (uint32_t)stored;
    layout.steps = stored == 1 ? 1 : PROJECTILE_STEPS;
    layout.width = (uint32_t)width;
    layout.height = (uint32_t)height;
    layout.spare_frames = frame_count - needed;
    *out = layout;
    return true;
}

bool projectiles_frame(const ProjectileLayout *layout, uint32_t step,
    uint32_t phase, uint32_t *out_frame, bool *out_mirror)
{
    if (step >= layout->steps || phase >= layout->phases) {
        return false;
    }
    /* A direction past the last stored one is the mirror image of the
     * one just as far from the start of the circle on the other side. */
    bool mirror = step >= layout->stored_steps;
    uint32_t stored = mirror ? PROJECTILE_STEPS - step : step;
    *out_frame = stored * layout->phases + phase;
    *out_mirror = mirror;
    return true;
}
