/* The shaders of the game, compiled. Made by build.sh from the shaders
 * in this folder: do not edit, run the script.
 *
 * A shader is kept in two forms: SPIR-V, which Vulkan takes, and MSL,
 * the text Metal takes.
 */
#ifndef ROM2_GAME_SHADERS_DATA_H
#define ROM2_GAME_SHADERS_DATA_H

#include <stddef.h>
#include <stdint.h>

typedef struct ShaderData {
    const uint8_t *spirv;
    size_t spirv_size;
    const uint8_t *msl;
    size_t msl_size;
} ShaderData;

/* picture.frag */
extern const ShaderData shader_picture_frag;
/* picture.vert */
extern const ShaderData shader_picture_vert;

#endif
