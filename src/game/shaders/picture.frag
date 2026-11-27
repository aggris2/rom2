#version 450

/* A pixel of a picture with a palette: the picture gives a number, and
 * the palette gives the colour that number stands for, with how solid
 * it is. */

layout(set = 2, binding = 0) uniform sampler2D indices;
layout(set = 2, binding = 1) uniform sampler2D palette;

layout(location = 0) in vec2 at;
layout(location = 0) out vec4 pixel;

void main()
{
    float part = texelFetch(indices, ivec2(at), 0).r;
    int index = int(part * 255.0 + 0.5);
    pixel = texelFetch(palette, ivec2(index, 0), 0);
}
