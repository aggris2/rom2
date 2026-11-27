#version 450

/* A corner of a square of ground: where it goes on the screen, which
 * point of the sheet of tiles is there, and how bright it is. The
 * inputs are whole numbers, as GroundCorner in view/frame.h has them. */

layout(set = 1, binding = 0) uniform Screen {
    vec2 size;          /* of the screen, in pixels */
    float light_whole;  /* the level of light at which a tile is as its
                           own colours: FRAME_LIGHT_WHOLE */
} screen;

layout(location = 0) in ivec2 place;    /* on the screen */
layout(location = 1) in uvec2 point;    /* in the sheet */
layout(location = 2) in uvec2 tile;     /* the tile's corner in the sheet */
layout(location = 3) in uvec2 light;    /* the level, and a number unused */

layout(location = 0) out vec2 at;
layout(location = 1) flat out vec2 from;
layout(location = 2) out float bright;

void main()
{
    at = vec2(point);
    from = vec2(tile);
    bright = float(light.x) / screen.light_whole;
    /* The screen runs from -1 to 1, with 1 at the top. */
    vec2 spot = vec2(place) / screen.size * 2.0 - 1.0;
    gl_Position = vec4(spot.x, -spot.y, 0.0, 1.0);
}
