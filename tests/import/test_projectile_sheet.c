#include "import/projectile_sheet.h"

#include <string.h>

#include "base/buffer.h"
#include "make_sprite.h"
#include "utest.h"

/* Where the pixel at `x`, `y` of the cell at `column`, `row` is in the
 * sheet's two blocks. */
static size_t at(const ProjectileSheet *s, uint32_t column, uint32_t row,
    uint32_t x, uint32_t y)
{
    size_t px = (size_t)column * s->cell_width + x;
    size_t py = (size_t)row * s->cell_height + y;
    return py * projectile_sheet_width(s) + px;
}

/* ---- a .16a file built by hand ---- */

static void put_u16(ByteBuffer *b, uint32_t value)
{
    buffer_push(b, (uint8_t)(value & 0xff));
    buffer_push(b, (uint8_t)((value >> 8) & 0xff));
}

/* Adds a frame of 2 by 1: two pixels, each a colour and a solidness of
 * 0 to 15. */
static void add_16a_frame(ByteBuffer *b, uint32_t colour0, uint32_t alpha0,
    uint32_t colour1, uint32_t alpha1)
{
    buffer_u32_le(b, 2);
    buffer_u32_le(b, 1);
    buffer_u32_le(b, 6);
    put_u16(b, 2);
    put_u16(b, (alpha0 << 9) | (colour0 << 1));
    put_u16(b, (alpha1 << 9) | (colour1 << 1));
}

/* Ends the file with its trailer and opens it. */
static bool finish_16a(ByteBuffer *b, uint32_t frames, Sprite256 *out)
{
    buffer_u32_le(b, frames);
    return sprite256_open(b->data, b->size, out);
}

UTEST(projectile_sheet, build_16a_gives_colour_and_solidness)
{
    /* One direction, two pictures, in a box of 4 by 3. */
    ByteBuffer file = {0};
    add_16a_frame(&file, 10, 15, 20, 1);
    add_16a_frame(&file, 30, 8, 0, 15);
    Sprite256 sprite;
    ASSERT_TRUE(finish_16a(&file, 2, &sprite));
    ProjectileLayout layout = {2, 1, 1, 4, 3, 0};
    ProjectileSheet s;

    ASSERT_TRUE(projectile_sheet_build_16a(&layout, &sprite, &s));

    ASSERT_EQ(4u, s.cell_width);
    ASSERT_EQ(3u, s.cell_height);
    ASSERT_EQ(2u, s.columns);
    ASSERT_EQ(1u, s.rows);
    ASSERT_EQ(8u, projectile_sheet_width(&s));
    ASSERT_EQ(3u, projectile_sheet_height(&s));
    /* The frame is the middle of the box: at 1, 1. Around it nothing. */
    ASSERT_EQ(0, s.solid[at(&s, 0, 0, 0, 1)]);
    ASSERT_EQ(0, s.solid[at(&s, 0, 0, 1, 0)]);
    /* The file's 15 levels of solidness are spread up to 255. */
    ASSERT_EQ(10, s.pixels[at(&s, 0, 0, 1, 1)]);
    ASSERT_EQ(255, s.solid[at(&s, 0, 0, 1, 1)]);
    ASSERT_EQ(20, s.pixels[at(&s, 0, 0, 2, 1)]);
    ASSERT_EQ(17, s.solid[at(&s, 0, 0, 2, 1)]);
    ASSERT_EQ(30, s.pixels[at(&s, 1, 0, 1, 1)]);
    ASSERT_EQ(136, s.solid[at(&s, 1, 0, 1, 1)]);
    /* Colour 0 is a colour like any other in a .16a: the pixel is
     * there, and solid. */
    ASSERT_EQ(0, s.pixels[at(&s, 1, 0, 2, 1)]);
    ASSERT_EQ(255, s.solid[at(&s, 1, 0, 2, 1)]);
    projectile_sheet_free(&s);
    ASSERT_TRUE(s.pixels == NULL);
    ASSERT_TRUE(s.solid == NULL);
    buffer_free(&file);
}

UTEST(projectile_sheet, build_16a_writes_out_the_mirrored_directions)
{
    /* 9 directions of one picture. Frame n has colour n + 1 on the left
     * and n + 101 on the right. */
    ByteBuffer file = {0};
    for (uint32_t i = 0; i < 9; i++) {
        add_16a_frame(&file, i + 1, 15, i + 101, 15);
    }
    Sprite256 sprite;
    ASSERT_TRUE(finish_16a(&file, 9, &sprite));
    ProjectileLayout layout = {1, 9, 16, 2, 1, 0};
    ProjectileSheet s;

    ASSERT_TRUE(projectile_sheet_build_16a(&layout, &sprite, &s));

    ASSERT_EQ(16u, s.rows);
    ASSERT_EQ(1u, s.columns);
    /* Direction 8 as stored. */
    ASSERT_EQ(9, s.pixels[at(&s, 0, 8, 0, 0)]);
    ASSERT_EQ(109, s.pixels[at(&s, 0, 8, 1, 0)]);
    /* Direction 9 is frame 7 mirrored: its right side is on the left. */
    ASSERT_EQ(108, s.pixels[at(&s, 0, 9, 0, 0)]);
    ASSERT_EQ(8, s.pixels[at(&s, 0, 9, 1, 0)]);
    /* Direction 15 is frame 1 mirrored. */
    ASSERT_EQ(102, s.pixels[at(&s, 0, 15, 0, 0)]);
    ASSERT_EQ(2, s.pixels[at(&s, 0, 15, 1, 0)]);
    ASSERT_EQ(255, s.solid[at(&s, 0, 15, 1, 0)]);
    projectile_sheet_free(&s);
    buffer_free(&file);
}

UTEST(projectile_sheet, build_16a_mirrors_about_the_middle_of_the_box)
{
    /* A frame 2 wide in a box 5 wide cannot be centred exactly: it sits
     * at 1, with 1 pixel free on its left and 2 on its right. Mirrored
     * it sits at 2. */
    ByteBuffer file = {0};
    for (uint32_t i = 0; i < 9; i++) {
        add_16a_frame(&file, i + 1, 15, i + 101, 15);
    }
    Sprite256 sprite;
    ASSERT_TRUE(finish_16a(&file, 9, &sprite));
    ProjectileLayout layout = {1, 9, 16, 5, 1, 0};
    ProjectileSheet s;

    ASSERT_TRUE(projectile_sheet_build_16a(&layout, &sprite, &s));

    /* Direction 1 as stored. */
    ASSERT_EQ(0, s.solid[at(&s, 0, 1, 0, 0)]);
    ASSERT_EQ(2, s.pixels[at(&s, 0, 1, 1, 0)]);
    ASSERT_EQ(102, s.pixels[at(&s, 0, 1, 2, 0)]);
    ASSERT_EQ(0, s.solid[at(&s, 0, 1, 3, 0)]);
    /* Direction 15, the same frame mirrored. */
    ASSERT_EQ(0, s.solid[at(&s, 0, 15, 1, 0)]);
    ASSERT_EQ(102, s.pixels[at(&s, 0, 15, 2, 0)]);
    ASSERT_EQ(2, s.pixels[at(&s, 0, 15, 3, 0)]);
    ASSERT_EQ(0, s.solid[at(&s, 0, 15, 4, 0)]);
    projectile_sheet_free(&s);
    buffer_free(&file);
}

UTEST(projectile_sheet, build_16a_refuses_what_does_not_fit)
{
    ByteBuffer file = {0};
    add_16a_frame(&file, 1, 15, 2, 15);
    Sprite256 sprite;
    ASSERT_TRUE(finish_16a(&file, 1, &sprite));
    ProjectileSheet s;
    memset(&s, 0, sizeof s);
    s.columns = 12345;

    /* Two pictures wanted and one in the file. */
    ProjectileLayout two = {2, 1, 1, 4, 3, 0};
    ASSERT_FALSE(projectile_sheet_build_16a(&two, &sprite, &s));
    /* A box narrower than the frame, of no size, and too large. */
    ProjectileLayout narrow = {1, 1, 1, 1, 3, 0};
    ASSERT_FALSE(projectile_sheet_build_16a(&narrow, &sprite, &s));
    ProjectileLayout none = {1, 1, 1, 0, 3, 0};
    ASSERT_FALSE(projectile_sheet_build_16a(&none, &sprite, &s));
    ProjectileLayout huge = {1, 1, 1, 1025, 3, 0};
    ASSERT_FALSE(projectile_sheet_build_16a(&huge, &sprite, &s));
    ASSERT_EQ(12345u, s.columns);
    ASSERT_TRUE(s.pixels == NULL);
    /* A box of exactly the frame's size is fine. */
    ProjectileLayout exact = {1, 1, 1, 2, 1, 0};
    ASSERT_TRUE(projectile_sheet_build_16a(&exact, &sprite, &s));
    projectile_sheet_free(&s);
    buffer_free(&file);
}

/* ---- .256 projectiles ---- */

UTEST(projectile_sheet, build_256_puts_the_figure_solid_and_its_edge_soft)
{
    /* One picture, not turning. The figure is 1 by 1 of colour 7 and its
     * edge 3 by 1 of colour 9, in a box of 3 by 1. */
    static MakeSprite m;
    static MakeSprite e;
    make_sprite_start(&m);
    make_sprite_start(&e);
    make_sprite_solid(&m, 1, 1, 7);
    make_sprite_solid(&e, 3, 1, 9);
    Sprite256 sprite;
    Sprite256 edges;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ASSERT_TRUE(make_sprite_finish(&e, &edges));
    ProjectileLayout layout = {1, 1, 1, 3, 1, 0};
    ProjectileSheet s;

    ASSERT_TRUE(projectile_sheet_build_256(&layout, &sprite, &edges, &s));

    ASSERT_EQ(9, s.pixels[at(&s, 0, 0, 0, 0)]);
    ASSERT_EQ(PROJECTILE_SHEET_EDGE, s.solid[at(&s, 0, 0, 0, 0)]);
    /* Where both have a pixel, the figure is what shows. */
    ASSERT_EQ(7, s.pixels[at(&s, 0, 0, 1, 0)]);
    ASSERT_EQ(255, s.solid[at(&s, 0, 0, 1, 0)]);
    ASSERT_EQ(9, s.pixels[at(&s, 0, 0, 2, 0)]);
    ASSERT_EQ(PROJECTILE_SHEET_EDGE, s.solid[at(&s, 0, 0, 2, 0)]);
    projectile_sheet_free(&s);

    /* Without edges only the figure is there. */
    ASSERT_TRUE(projectile_sheet_build_256(&layout, &sprite, NULL, &s));
    ASSERT_EQ(0, s.solid[at(&s, 0, 0, 0, 0)]);
    ASSERT_EQ(7, s.pixels[at(&s, 0, 0, 1, 0)]);
    ASSERT_EQ(255, s.solid[at(&s, 0, 0, 1, 0)]);
    projectile_sheet_free(&s);
}

UTEST(projectile_sheet, build_256_leaves_index_0_empty)
{
    static MakeSprite m;
    const uint8_t pixels[2] = {0, 5};
    make_sprite_start(&m);
    make_sprite_frame(&m, 2, 1, pixels);
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ProjectileLayout layout = {1, 1, 1, 2, 1, 0};
    ProjectileSheet s;

    ASSERT_TRUE(projectile_sheet_build_256(&layout, &sprite, NULL, &s));

    ASSERT_EQ(0, s.solid[at(&s, 0, 0, 0, 0)]);
    ASSERT_EQ(5, s.pixels[at(&s, 0, 0, 1, 0)]);
    ASSERT_EQ(255, s.solid[at(&s, 0, 0, 1, 0)]);
    projectile_sheet_free(&s);
}

UTEST(projectile_sheet, build_256_lays_directions_in_rows)
{
    /* An arrow: 16 directions of one picture, each of its own colour. */
    static MakeSprite m;
    make_sprite_start(&m);
    for (uint8_t i = 0; i < 16; i++) {
        make_sprite_solid(&m, 1, 1, (uint8_t)(i + 1));
    }
    Sprite256 sprite;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ProjectileLayout layout = {1, 16, 16, 1, 1, 0};
    ProjectileSheet s;

    ASSERT_TRUE(projectile_sheet_build_256(&layout, &sprite, NULL, &s));

    ASSERT_EQ(16u, s.rows);
    for (uint32_t step = 0; step < 16; step++) {
        ASSERT_EQ(step + 1, s.pixels[at(&s, 0, step, 0, 0)]);
    }
    projectile_sheet_free(&s);
}

UTEST(projectile_sheet, build_256_refuses_what_does_not_fit)
{
    static MakeSprite m;
    static MakeSprite e;
    make_sprite_start(&m);
    make_sprite_start(&e);
    make_sprite_solid(&m, 2, 2, 7);
    make_sprite_solid(&e, 2, 2, 9);
    make_sprite_solid(&e, 2, 2, 9);
    Sprite256 sprite;
    Sprite256 edges;
    ASSERT_TRUE(make_sprite_finish(&m, &sprite));
    ASSERT_TRUE(make_sprite_finish(&e, &edges));
    ProjectileSheet s;
    memset(&s, 0, sizeof s);
    s.columns = 12345;

    /* Edges with another number of frames, too few frames, and a box
     * smaller than the frame. */
    ProjectileLayout fine = {1, 1, 1, 2, 2, 0};
    ASSERT_FALSE(projectile_sheet_build_256(&fine, &sprite, &edges, &s));
    ProjectileLayout two = {2, 1, 1, 2, 2, 0};
    ASSERT_FALSE(projectile_sheet_build_256(&two, &sprite, NULL, &s));
    ProjectileLayout low = {1, 1, 1, 2, 1, 0};
    ASSERT_FALSE(projectile_sheet_build_256(&low, &sprite, NULL, &s));
    ASSERT_EQ(12345u, s.columns);
    ASSERT_TRUE(s.pixels == NULL);
    ASSERT_TRUE(projectile_sheet_build_256(&fine, &sprite, NULL, &s));
    projectile_sheet_free(&s);
}
