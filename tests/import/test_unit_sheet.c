#include "import/unit_sheet.h"

#include <string.h>

#include "utest.h"

/* A sprite file built by hand, without a palette: frames one after the
 * other and a trailer. */
typedef struct MakeSprite {
    uint8_t bytes[8192];
    size_t size;
    uint32_t frames;
} MakeSprite;

static void put_u32(uint8_t *at, uint32_t value)
{
    at[0] = (uint8_t)(value & 0xff);
    at[1] = (uint8_t)((value >> 8) & 0xff);
    at[2] = (uint8_t)((value >> 16) & 0xff);
    at[3] = (uint8_t)((value >> 24) & 0xff);
}

static void make_sprite_start(MakeSprite *m)
{
    memset(m, 0, sizeof *m);
}

/* Adds a frame of `width` by `height` pixels. Each row is written as one
 * run of pixels, so `width` is at most 63. */
static void make_sprite_frame(MakeSprite *m, uint32_t width, uint32_t height,
    const uint8_t *pixels)
{
    uint8_t *header = m->bytes + m->size;
    m->size += 12;
    size_t codes_start = m->size;
    for (uint32_t y = 0; y < height; y++) {
        m->bytes[m->size++] = (uint8_t)width;
        memcpy(m->bytes + m->size, pixels + (size_t)y * width, width);
        m->size += width;
    }
    put_u32(header, width);
    put_u32(header + 4, height);
    put_u32(header + 8, (uint32_t)(m->size - codes_start));
    m->frames++;
}

/* Adds a frame filled with one value. */
static void make_sprite_solid(MakeSprite *m, uint32_t width, uint32_t height,
    uint8_t value)
{
    uint8_t pixels[64];
    memset(pixels, value, sizeof pixels);
    make_sprite_frame(m, width, height, pixels);
}

static bool make_sprite_finish(MakeSprite *m, Sprite256 *out)
{
    put_u32(m->bytes + m->size, m->frames);
    m->size += 4;
    return sprite256_open(m->bytes, m->size, out);
}

/* The layout of a unit that is not flipped, with `move` moving frames for
 * each direction and nothing else: 16 + move * 8 frames. */
static UnitLayout plain_layout(uint32_t move)
{
    UnitLayout l;
    memset(&l, 0, sizeof l);
    l.phases[UNIT_ANIM_MOVE] = move;
    l.own_dying = true;
    for (int anim = 0; anim < UNIT_ANIM_COUNT; anim++) {
        l.first[anim] = 16;
    }
    for (int anim = UNIT_ANIM_ATTACK; anim < UNIT_ANIM_COUNT; anim++) {
        l.first[anim] = 16 + move * 8;
    }
    l.frame_count = 16 + move * 8;
    return l;
}

/* The same for a flipped unit: 9 + move * 5 frames. */
static UnitLayout flipped_layout(uint32_t move)
{
    UnitLayout l = plain_layout(move);
    l.flip = true;
    l.first[UNIT_ANIM_MOVE] = 9;
    for (int anim = UNIT_ANIM_ATTACK; anim < UNIT_ANIM_COUNT; anim++) {
        l.first[anim] = 9 + move * 5;
    }
    l.frame_count = 9 + move * 5;
    return l;
}

/* The pixel at `x`, `y` inside the cell at `column`, `row`. */
static uint8_t cell_at(const UnitSheet *u, uint32_t column, uint32_t row,
    uint32_t x, uint32_t y)
{
    const Sheet *s = &u->sheet;
    size_t px = (size_t)column * s->cell_width + x;
    size_t py = (size_t)row * s->cell_height + y;
    return s->pixels[py * sheet_width(s) + px];
}

/* ---- unit_sheet_build ---- */

UTEST(unit_sheet, build_puts_every_frame_in_its_row_and_column)
{
    /* 24 frames of 2 by 2, each filled with its own number from 1 up.
     * The box is 8 by 6, so every frame sits at 3, 2 in it. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 24; i++) {
        make_sprite_solid(&m, 2, 2, (uint8_t)(i + 1));
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = plain_layout(1);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, 8, 6, 4, 5, &u));

    /* The cell is the part of the box the frames use: 2 by 2. */
    ASSERT_EQ(2u, u.sheet.cell_width);
    ASSERT_EQ(2u, u.sheet.cell_height);
    ASSERT_EQ(16u, u.sheet.columns);
    ASSERT_EQ(9u, u.sheet.rows);
    /* The centre 4, 5 of the box, counted from the cell's corner 3, 2. */
    ASSERT_EQ(1, u.anchor_x);
    ASSERT_EQ(3, u.anchor_y);
    ASSERT_EQ(0u, u.column[UNIT_ANIM_MOVE]);
    ASSERT_EQ(1u, u.columns[UNIT_ANIM_MOVE]);
    ASSERT_EQ(0u, u.columns[UNIT_ANIM_ATTACK]);

    /* Row 0: the 16 standing frames, 1 to 16. */
    for (uint32_t step = 0; step < 16; step++) {
        ASSERT_EQ(step + 1, cell_at(&u, step, 0, 0, 0));
        ASSERT_EQ(step + 1, cell_at(&u, step, 0, 1, 1));
    }
    /* Rows 1 to 8: one moving frame for each direction, 17 to 24. */
    for (uint32_t d = 0; d < 8; d++) {
        ASSERT_EQ(17 + d, cell_at(&u, 0, 1 + d, 0, 0));
        ASSERT_EQ(0, cell_at(&u, 1, 1 + d, 0, 0));
    }
    unit_sheet_free(&u);
}

UTEST(unit_sheet, build_keeps_frames_of_different_sizes_in_place)
{
    /* Frame 0 is 6 by 2 and the rest 2 by 4, in a box of 8 by 8. The
     * wide one sits at 1, 3 in the box and the tall ones at 3, 2, so the
     * cell is 6 wide and 4 high, starting at 1, 2. */
    static MakeSprite m;
    make_sprite_start(&m);
    make_sprite_solid(&m, 6, 2, 9);
    for (uint8_t i = 1; i < 16; i++) {
        make_sprite_solid(&m, 2, 4, 5);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = plain_layout(0);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, 8, 8, 4, 6, &u));

    ASSERT_EQ(6u, u.sheet.cell_width);
    ASSERT_EQ(4u, u.sheet.cell_height);
    ASSERT_EQ(3, u.anchor_x);
    ASSERT_EQ(4, u.anchor_y);
    /* The wide frame: rows 1 and 2 of its cell, all 6 columns. */
    ASSERT_EQ(0, cell_at(&u, 0, 0, 0, 0));
    ASSERT_EQ(9, cell_at(&u, 0, 0, 0, 1));
    ASSERT_EQ(9, cell_at(&u, 0, 0, 5, 2));
    ASSERT_EQ(0, cell_at(&u, 0, 0, 5, 3));
    /* A tall frame: columns 2 and 3 of its cell, all 4 rows. */
    ASSERT_EQ(0, cell_at(&u, 1, 0, 1, 0));
    ASSERT_EQ(5, cell_at(&u, 1, 0, 2, 0));
    ASSERT_EQ(5, cell_at(&u, 1, 0, 3, 3));
    ASSERT_EQ(0, cell_at(&u, 1, 0, 4, 3));
    unit_sheet_free(&u);
}

UTEST(unit_sheet, build_writes_out_the_mirrored_half_of_a_flipped_unit)
{
    /* Nine standing frames and five directions of one moving frame. Each
     * frame is 2 by 1 with its number on the left and 100 more on the
     * right, so a mirrored one can be told from a plain one. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 14; i++) {
        const uint8_t pixels[2] = {(uint8_t)(i + 1), (uint8_t)(i + 101)};
        make_sprite_frame(&m, 2, 1, pixels);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = flipped_layout(1);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, 4, 4, 2, 2, &u));

    ASSERT_EQ(2u, u.sheet.cell_width);
    /* Steps 0 to 8 as stored. */
    ASSERT_EQ(1, cell_at(&u, 0, 0, 0, 0));
    ASSERT_EQ(101, cell_at(&u, 0, 0, 1, 0));
    ASSERT_EQ(9, cell_at(&u, 8, 0, 0, 0));
    /* Step 9 is frame 7 mirrored: its number 8 is now on the right. */
    ASSERT_EQ(108, cell_at(&u, 9, 0, 0, 0));
    ASSERT_EQ(8, cell_at(&u, 9, 0, 1, 0));
    /* Step 15 is frame 1 mirrored. */
    ASSERT_EQ(102, cell_at(&u, 15, 0, 0, 0));
    ASSERT_EQ(2, cell_at(&u, 15, 0, 1, 0));
    /* Direction 4 is the last stored one, frame 9 + 4. */
    ASSERT_EQ(14, cell_at(&u, 0, 1 + 4, 0, 0));
    /* Direction 5 is direction 3 mirrored, frame 9 + 3. */
    ASSERT_EQ(113, cell_at(&u, 0, 1 + 5, 0, 0));
    ASSERT_EQ(13, cell_at(&u, 0, 1 + 5, 1, 0));
    /* Direction 7 is direction 1 mirrored, frame 9 + 1. */
    ASSERT_EQ(111, cell_at(&u, 0, 1 + 7, 0, 0));
    ASSERT_EQ(11, cell_at(&u, 0, 1 + 7, 1, 0));
    unit_sheet_free(&u);
}

UTEST(unit_sheet, build_mirrors_about_the_middle_of_the_box)
{
    /* A box 5 wide and a frame 2 wide cannot be centred exactly: the
     * frame sits at 1, with 1 pixel free on its left and 2 on its right.
     * Mirrored it sits at 2. The cell must hold both: 3 wide, from 1. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 9; i++) {
        const uint8_t pixels[2] = {(uint8_t)(i + 1), (uint8_t)(i + 101)};
        make_sprite_frame(&m, 2, 1, pixels);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = flipped_layout(0);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, 5, 1, 2, 0, &u));

    ASSERT_EQ(3u, u.sheet.cell_width);
    ASSERT_EQ(1, u.anchor_x);
    /* Step 1 as stored: at the left of the cell. */
    ASSERT_EQ(2, cell_at(&u, 1, 0, 0, 0));
    ASSERT_EQ(102, cell_at(&u, 1, 0, 1, 0));
    ASSERT_EQ(0, cell_at(&u, 1, 0, 2, 0));
    /* Step 15, the same frame mirrored: at the right of the cell. */
    ASSERT_EQ(0, cell_at(&u, 15, 0, 0, 0));
    ASSERT_EQ(102, cell_at(&u, 15, 0, 1, 0));
    ASSERT_EQ(2, cell_at(&u, 15, 0, 2, 0));
    unit_sheet_free(&u);
}

UTEST(unit_sheet, build_lays_the_animations_side_by_side)
{
    /* 2 moving and 3 attack frames for each direction: 16 + 5 * 8. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 56; i++) {
        make_sprite_solid(&m, 1, 1, (uint8_t)(i + 1));
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = plain_layout(2);
    layout.phases[UNIT_ANIM_ATTACK] = 3;
    layout.first[UNIT_ANIM_ATTACK] = 16 + 2 * 8;
    layout.frame_count = 56;
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, 1, 1, 0, 0, &u));

    ASSERT_EQ(0u, u.column[UNIT_ANIM_MOVE]);
    ASSERT_EQ(2u, u.columns[UNIT_ANIM_MOVE]);
    ASSERT_EQ(2u, u.column[UNIT_ANIM_ATTACK]);
    ASSERT_EQ(3u, u.columns[UNIT_ANIM_ATTACK]);
    /* Never fewer than the 16 columns the standing row needs. */
    ASSERT_EQ(16u, u.sheet.columns);
    /* Direction 3: moving frames 16 + 3 * 2 + 0 and + 1, then attack
     * frames 32 + 3 * 3 + 0, + 1, + 2. Frame n holds the number n + 1. */
    ASSERT_EQ(23, cell_at(&u, 0, 1 + 3, 0, 0));
    ASSERT_EQ(24, cell_at(&u, 1, 1 + 3, 0, 0));
    ASSERT_EQ(42, cell_at(&u, 2, 1 + 3, 0, 0));
    ASSERT_EQ(43, cell_at(&u, 3, 1 + 3, 0, 0));
    ASSERT_EQ(44, cell_at(&u, 4, 1 + 3, 0, 0));
    ASSERT_EQ(0, cell_at(&u, 5, 1 + 3, 0, 0));
    unit_sheet_free(&u);
}

UTEST(unit_sheet, build_with_more_than_16_columns_of_animation)
{
    /* 20 moving frames for each direction: the sheet is 20 wide. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (int i = 0; i < 16 + 20 * 8; i++) {
        make_sprite_solid(&m, 1, 1, (uint8_t)(i % 200 + 1));
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = plain_layout(20);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, 1, 1, 0, 0, &u));

    ASSERT_EQ(20u, u.sheet.columns);
    /* The last frame: direction 7, frame 19, which is frame 175. */
    ASSERT_EQ(176, cell_at(&u, 19, 8, 0, 0));
    /* The standing row is empty past its 16 steps. */
    ASSERT_EQ(0, cell_at(&u, 16, 0, 0, 0));
    unit_sheet_free(&u);
}

UTEST(unit_sheet, build_refuses_a_sprite_with_the_wrong_number_of_frames)
{
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 23; i++) {
        make_sprite_solid(&m, 2, 2, 1);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = plain_layout(1); /* wants 24 */
    UnitSheet u;
    memset(&u, 0, sizeof u);
    u.anchor_x = 12345;

    ASSERT_FALSE(unit_sheet_build(&layout, &sprite, 8, 6, 4, 5, &u));

    ASSERT_EQ(12345, u.anchor_x);
    ASSERT_TRUE(u.sheet.pixels == NULL);
}

UTEST(unit_sheet, build_refuses_a_frame_larger_than_the_box)
{
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 16; i++) {
        make_sprite_solid(&m, 2, 2, 1);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = plain_layout(0);
    UnitSheet u;
    memset(&u, 0, sizeof u);
    u.anchor_x = 12345;

    /* A box of exactly the frame's size is fine. */
    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, 2, 2, 1, 1, &u));
    unit_sheet_free(&u);

    u.anchor_x = 12345;
    ASSERT_FALSE(unit_sheet_build(&layout, &sprite, 1, 2, 0, 0, &u));
    ASSERT_FALSE(unit_sheet_build(&layout, &sprite, 2, 1, 0, 0, &u));
    /* A box of no size, or beyond the limit. */
    ASSERT_FALSE(unit_sheet_build(&layout, &sprite, 0, 2, 0, 0, &u));
    ASSERT_FALSE(unit_sheet_build(&layout, &sprite, 2, 0, 0, 0, &u));
    ASSERT_FALSE(unit_sheet_build(&layout, &sprite, 2049, 2, 0, 0, &u));
    ASSERT_FALSE(unit_sheet_build(&layout, &sprite, 2, 2049, 0, 0, &u));
    ASSERT_EQ(12345, u.anchor_x);
}

UTEST(unit_sheet, build_refuses_a_damaged_frame)
{
    /* The last frame's codes fill only one of its two pixels. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 15; i++) {
        make_sprite_solid(&m, 2, 1, 1);
    }
    uint8_t *broken = m.bytes + m.size;
    make_sprite_solid(&m, 1, 1, 1);
    put_u32(broken, 2);
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = plain_layout(0);
    UnitSheet u;
    memset(&u, 0, sizeof u);

    ASSERT_FALSE(unit_sheet_build(&layout, &sprite, 4, 4, 2, 2, &u));
    ASSERT_TRUE(u.sheet.pixels == NULL);
}

UTEST(unit_sheet, build_of_a_unit_whose_frames_are_all_empty)
{
    /* 16 frames of no size at all. There is nothing to show, and the
     * sheet is one transparent pixel for each cell. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 16; i++) {
        make_sprite_solid(&m, 0, 0, 0);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = plain_layout(0);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, 8, 8, 4, 4, &u));

    ASSERT_EQ(1u, u.sheet.cell_width);
    ASSERT_EQ(1u, u.sheet.cell_height);
    ASSERT_EQ(0, cell_at(&u, 0, 0, 0, 0));
    unit_sheet_free(&u);
}

/* ---- unit_sheet_build_dying ---- */

/* The layout of a unit with every kind of frame in its own file, in the
 * file's order: standing, moving, attack, dying, bones, idle. */
static UnitLayout full_layout(bool flip, uint32_t move, uint32_t attack,
    uint32_t dying, uint32_t bones, uint32_t idle)
{
    UnitLayout l;
    memset(&l, 0, sizeof l);
    l.flip = flip;
    l.own_dying = true;
    l.phases[UNIT_ANIM_MOVE] = move;
    l.phases[UNIT_ANIM_ATTACK] = attack;
    l.phases[UNIT_ANIM_DYING] = dying;
    l.phases[UNIT_ANIM_BONES] = bones;
    l.phases[UNIT_ANIM_IDLE] = idle;
    uint32_t next = flip ? 9 : 16;
    for (int anim = 0; anim < UNIT_ANIM_COUNT; anim++) {
        l.first[anim] = next;
        next += l.phases[anim] * (flip ? 5u : 8u);
    }
    l.frame_count = next;
    return l;
}

UTEST(unit_sheet, build_dying_puts_dying_then_bones_in_each_row)
{
    /* 1 moving, 2 dying and 1 bones frame for each direction:
     * 16 + 8 + 16 + 8 frames, each 1 by 1 and holding its number + 1. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 48; i++) {
        make_sprite_solid(&m, 1, 1, (uint8_t)(i + 1));
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = full_layout(false, 1, 0, 2, 1, 0);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build_dying(&layout, &sprite, 1, 1, 0, 0, &u));

    ASSERT_EQ(3u, u.sheet.columns);
    ASSERT_EQ(8u, u.sheet.rows);
    ASSERT_EQ(0u, u.first_direction_row);
    ASSERT_EQ(0u, u.column[UNIT_ANIM_DYING]);
    ASSERT_EQ(2u, u.columns[UNIT_ANIM_DYING]);
    ASSERT_EQ(2u, u.column[UNIT_ANIM_BONES]);
    ASSERT_EQ(1u, u.columns[UNIT_ANIM_BONES]);
    ASSERT_EQ(0u, u.columns[UNIT_ANIM_MOVE]);
    /* Dying starts at frame 24 and bones at frame 40. */
    for (uint32_t d = 0; d < 8; d++) {
        ASSERT_EQ(25 + d * 2, cell_at(&u, 0, d, 0, 0));
        ASSERT_EQ(26 + d * 2, cell_at(&u, 1, d, 0, 0));
        ASSERT_EQ(41 + d, cell_at(&u, 2, d, 0, 0));
    }
    unit_sheet_free(&u);
}

UTEST(unit_sheet, build_dying_writes_out_the_mirrored_directions)
{
    /* A flipped unit with one dying frame for each of its 5 stored
     * directions: 9 + 5 frames, each with its number on the left and 100
     * more on the right. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 14; i++) {
        const uint8_t pixels[2] = {(uint8_t)(i + 1), (uint8_t)(i + 101)};
        make_sprite_frame(&m, 2, 1, pixels);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = full_layout(true, 0, 0, 1, 0, 0);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build_dying(&layout, &sprite, 4, 4, 2, 2, &u));

    ASSERT_EQ(1u, u.sheet.columns);
    ASSERT_EQ(8u, u.sheet.rows);
    /* Direction 4 as stored, frame 9 + 4. */
    ASSERT_EQ(14, cell_at(&u, 0, 4, 0, 0));
    ASSERT_EQ(114, cell_at(&u, 0, 4, 1, 0));
    /* Direction 5 is direction 3 mirrored, frame 9 + 3. */
    ASSERT_EQ(113, cell_at(&u, 0, 5, 0, 0));
    ASSERT_EQ(13, cell_at(&u, 0, 5, 1, 0));
    /* Direction 7 is direction 1 mirrored, frame 9 + 1. */
    ASSERT_EQ(111, cell_at(&u, 0, 7, 0, 0));
    ASSERT_EQ(11, cell_at(&u, 0, 7, 1, 0));
    unit_sheet_free(&u);
}

UTEST(unit_sheet, each_sheet_is_sized_by_its_own_frames_only)
{
    /* Standing and idle frames are 2 by 2 and filled with 1 and 3. The
     * dying frames, between them in the file, are 6 by 1 and filled with
     * 2. The main sheet must not grow to hold a dying frame, and the
     * dying sheet must not grow to hold a standing one. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (int i = 0; i < 16; i++) {
        make_sprite_solid(&m, 2, 2, 1);
    }
    for (int i = 0; i < 8; i++) {
        make_sprite_solid(&m, 6, 1, 2);
    }
    for (int i = 0; i < 8; i++) {
        make_sprite_solid(&m, 2, 2, 3);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = full_layout(false, 0, 0, 1, 0, 1);
    UnitSheet u;

    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, 8, 8, 4, 4, &u));
    ASSERT_EQ(2u, u.sheet.cell_width);
    ASSERT_EQ(2u, u.sheet.cell_height);
    ASSERT_EQ(0u, u.columns[UNIT_ANIM_DYING]);
    ASSERT_EQ(0u, u.column[UNIT_ANIM_IDLE]);
    ASSERT_EQ(1u, u.columns[UNIT_ANIM_IDLE]);
    /* The idle frames come after the dying ones in the file. */
    ASSERT_EQ(3, cell_at(&u, 0, 1, 0, 0));
    ASSERT_EQ(3, cell_at(&u, 0, 8, 1, 1));
    unit_sheet_free(&u);

    ASSERT_TRUE(unit_sheet_build_dying(&layout, &sprite, 8, 8, 4, 4, &u));
    ASSERT_EQ(6u, u.sheet.cell_width);
    ASSERT_EQ(1u, u.sheet.cell_height);
    /* The frame sits at 1, 3 in the box, and so does the cell. */
    ASSERT_EQ(3, u.anchor_x);
    ASSERT_EQ(1, u.anchor_y);
    ASSERT_EQ(2, cell_at(&u, 0, 0, 0, 0));
    ASSERT_EQ(2, cell_at(&u, 0, 7, 5, 0));
    unit_sheet_free(&u);
}

UTEST(unit_sheet, build_dying_refuses_a_unit_with_frames_in_another_file)
{
    /* The layout of a unit that borrows: its file has no dying frames. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (int i = 0; i < 16; i++) {
        make_sprite_solid(&m, 1, 1, 1);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = plain_layout(0);
    layout.phases[UNIT_ANIM_DYING] = 2;
    layout.own_dying = false;
    UnitSheet u;
    memset(&u, 0, sizeof u);
    u.anchor_x = 12345;

    ASSERT_FALSE(unit_sheet_build_dying(&layout, &sprite, 1, 1, 0, 0, &u));

    ASSERT_EQ(12345, u.anchor_x);
    ASSERT_TRUE(u.sheet.pixels == NULL);
    /* Its main sheet is still fine. */
    ASSERT_TRUE(unit_sheet_build(&layout, &sprite, 1, 1, 0, 0, &u));
    unit_sheet_free(&u);
}

UTEST(unit_sheet, build_dying_refuses_a_unit_without_dying_frames)
{
    static MakeSprite m;
    make_sprite_start(&m);
    for (int i = 0; i < 24; i++) {
        make_sprite_solid(&m, 1, 1, 1);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = plain_layout(1);
    UnitSheet u;
    memset(&u, 0, sizeof u);
    u.anchor_x = 12345;

    ASSERT_FALSE(unit_sheet_build_dying(&layout, &sprite, 1, 1, 0, 0, &u));

    ASSERT_EQ(12345, u.anchor_x);
    ASSERT_TRUE(u.sheet.pixels == NULL);
}

UTEST(unit_sheet, build_dying_refuses_a_sprite_with_too_few_frames)
{
    static MakeSprite m;
    make_sprite_start(&m);
    for (int i = 0; i < 23; i++) {
        make_sprite_solid(&m, 1, 1, 1);
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    UnitLayout layout = full_layout(false, 0, 0, 1, 0, 0); /* wants 24 */
    UnitSheet u;
    memset(&u, 0, sizeof u);

    ASSERT_FALSE(unit_sheet_build_dying(&layout, &sprite, 1, 1, 0, 0, &u));
    ASSERT_TRUE(u.sheet.pixels == NULL);
}
