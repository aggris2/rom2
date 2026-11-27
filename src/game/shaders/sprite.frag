#version 450

/* A pixel of a sprite. The sheet gives two numbers for it: which colour
 * of the palette, and how solid it is. The colour is made as bright as
 * the sprite is, and laid over what is there by how solid it is. */

layout(set = 2, binding = 0) uniform sampler2D indices;
layout(set = 2, binding = 1) uniform sampler2D solid;
layout(set = 2, binding = 2) uniform sampler2D palette;

layout(location = 0) in vec2 at;
layout(location = 1) in float bright;

layout(location = 0) out vec4 pixel;

void main()
{
    ivec2 spot = ivec2(at);
    int index = int(texelFetch(indices, spot, 0).r * 255.0 + 0.5);
    vec3 colour = texelFetch(palette, ivec2(index, 0), 0).rgb;
    pixel = vec4(min(colour * bright, vec3(1.0)),
        texelFetch(solid, spot, 0).r);
}
