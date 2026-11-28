#version 450

/* A pixel of ground, by the rule of the original game.
 *
 * A square is drawn as columns one pixel wide. Its top and its low
 * edge each go from the corner on the left to the corner on the right
 * in steps of one pixel up or down, and a column runs straight down
 * from the one edge to the other. The column of the tile is squeezed
 * or stretched to fit, and the light runs along the two edges and then
 * down the column. All of it is in whole numbers, as the original
 * works it out, so that a slope looks as it does there. */

/* The same three as a sprite's, so that one sheet serves both: how
 * solid a pixel is does not matter to the ground, which has no holes. */
layout(set = 2, binding = 0) uniform sampler2D indices;
layout(set = 2, binding = 1) uniform sampler2D solid;
layout(set = 2, binding = 2) uniform sampler2D palette;

layout(set = 3, binding = 0) uniform Ground {
    int square;         /* the side of a square, and of a tile */
    int light_whole;    /* the level at which a tile is as its own
                           colours */
    int light_most;     /* the highest level there is */
    int grid;           /* how strongly the grid is drawn, in 255ths:
                           0 for no grid */
} ground;

layout(location = 0) flat in ivec4 corners;  /* y: see ground.vert */
layout(location = 1) flat in ivec3 from;     /* left edge, then the tile */
layout(location = 2) flat in ivec4 levels;
layout(location = 3) flat in int mark;   /* a FRAME_MARK_ of
                                            view/frame.h: 0 for none */

layout(location = 0) out vec4 pixel;

/* Numbers with 16 bits after the point: one, and a half. */
const int ONE = 65536;
const int HALF = 32768;

/* The colours a marked square is drawn towards, a closed one, one
 * a unit holds and one that is seen, and how far: a third of the
 * way. */
const ivec3 MARK_CLOSED = ivec3(255, 40, 40);
const ivec3 MARK_HELD = ivec3(40, 255, 90);
const ivec3 MARK_SEEN = ivec3(255, 230, 60);
const int MARK_STRENGTH = 85;

/* The light is worked out as how dark it is, with 9 bits after the
 * point. */
const int DARK_ONE = 512;

/* An edge that changes by `steps` pixels over the square takes them
 * one at a time, spread evenly: how many it has taken at a column. */
int taken(int column, int steps)
{
    int apart = ground.square * ONE / (steps + 1);
    return clamp(((column + 1) * ONE - HALF - 1) / apart, 0, steps);
}

/* Where the top or the low edge of the square is at a column, going
 * from `left` to `right`. The top edge takes its steps early when it
 * goes down and late when it goes up, and the low edge the other way
 * round, so that a column is never taller than the square's side. */
int edge(int left, int right, int column, bool top)
{
    int steps = abs(right - left);
    bool down = right > left;
    int moved = top == down ? taken(column, steps)
        : steps - taken(ground.square - 1 - column, steps);
    return down ? left + moved : left - moved;
}

void main()
{
    ivec2 here = ivec2(gl_FragCoord.xy);
    int column = here.x - from.x;
    int top = edge(corners.x, corners.y, column, true);
    int tall = edge(corners.z, corners.w, column, false) - top;
    int row = here.y - top;
    if (row < 0 || row >= tall) {
        discard;
    }
    ivec2 spot = from.yz + ivec2(column, row * ground.square / tall);
    int index = int(texelFetch(indices, spot, 0).r * 255.0 + 0.5);
    ivec3 colour = ivec3(texelFetch(palette, ivec2(index, 0), 0).rgb
        * 255.0 + 0.5);

    /* Along each edge the light goes evenly from the left corner to
     * the right. Down the column it goes from the top edge towards the
     * low one in equal steps, each cut to a whole number, and starts
     * half a level in. */
    ivec4 dark = (ground.light_most - levels) * DARK_ONE;
    int along = DARK_ONE / ground.square;
    int dark_top = dark.x + (levels.x - levels.y) * along * column;
    int dark_low = dark.z + (levels.z - levels.w) * along * column;
    int dark_here = dark_top + DARK_ONE / 2
        + (dark_low - dark_top) / tall * row;
    int level = ground.light_most - dark_here / DARK_ONE;
    ivec3 lit = min(colour * level / ground.light_whole, 255);

    /* A mark, for looking at what the game takes the square to be. */
    if (mark != 0) {
        ivec3 towards = mark == 1 ? MARK_CLOSED : mark == 2 ? MARK_HELD
            : MARK_SEEN;
        lit += (towards - lit) * MARK_STRENGTH / 255;
    }

    /* The grid: the square's left column, and its top edge. Where the
     * edge steps by more than a pixel from a column to the one beside
     * it, the line is as many pixels tall, so that it has no gaps. */
    if (ground.grid > 0) {
        int beside = max(
            edge(corners.x, corners.y, column + 1, true),
            edge(corners.x, corners.y, column - 1, true)) - top;
        if (column == 0 || row < max(beside, 1)) {
            lit += (255 - lit) * ground.grid / 255;
        }
    }
    pixel = vec4(vec3(lit) / 255.0, 1.0);
}
