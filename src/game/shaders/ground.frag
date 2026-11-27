#version 450

/* A pixel of ground: the colour of its tile, made as bright as the
 * light there says. The light runs smoothly between the corners of a
 * square, so a slope shades from one side to the other. */

/* The same three as a sprite's, so that one sheet serves both: how
 * solid a pixel is does not matter to the ground, which has no holes. */
layout(set = 2, binding = 0) uniform sampler2D indices;
layout(set = 2, binding = 1) uniform sampler2D solid;
layout(set = 2, binding = 2) uniform sampler2D palette;

layout(location = 0) in vec2 at;
layout(location = 1) flat in vec2 from;
layout(location = 2) in float bright;

layout(location = 0) out vec4 pixel;

void main()
{
    /* Never a pixel of the tile next to this one in the sheet, however
     * a slope stretches the square. A tile is 32 pixels a side:
     * MAP_SQUARE in the C code. */
    ivec2 first = ivec2(from);
    ivec2 spot = clamp(ivec2(at), first, first + 31);
    float part = texelFetch(indices, spot, 0).r;
    int index = int(part * 255.0 + 0.5);
    vec3 colour = texelFetch(palette, ivec2(index, 0), 0).rgb;
    pixel = vec4(min(colour * bright, vec3(1.0)), 1.0);
}
