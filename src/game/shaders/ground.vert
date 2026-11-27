#version 450

/* A corner of a square of ground: where it goes on the screen, which
 * point of the sheet of tiles is there, and how bright it is. */

layout(set = 1, binding = 0) uniform Screen {
    vec2 size;      /* of the screen, in pixels */
} screen;

layout(location = 0) in vec2 place;     /* on the screen */
layout(location = 1) in vec2 point;     /* in the sheet */
layout(location = 2) in vec2 tile;      /* the tile's corner in the sheet */
layout(location = 3) in float light;    /* 1 is as the tile's colours */

layout(location = 0) out vec2 at;
layout(location = 1) flat out vec2 from;
layout(location = 2) out float bright;

void main()
{
    at = point;
    from = tile;
    bright = light;
    /* The screen runs from -1 to 1, with 1 at the top. */
    vec2 spot = place / screen.size * 2.0 - 1.0;
    gl_Position = vec4(spot.x, -spot.y, 0.0, 1.0);
}
