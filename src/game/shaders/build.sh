#!/bin/sh
# Compiles the shaders in this folder into the two forms the game needs,
# and writes them as C into data.c and data.h beside them.
#
# Run it after changing a shader, from anywhere, and commit what it
# writes. The build itself needs none of these tools.
#
# Needs glslangValidator and spirv-cross: on Ubuntu,
#     sudo apt install glslang-tools spirv-cross
set -eu

source_dir=$(dirname "$0")
out_c=$source_dir/data.c
out_h=$source_dir/data.h
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# Writes a file as a C array of bytes, 12 to a line.
bytes() {
    od -An -v -tx1 "$1" | tr -s ' \n' '\n\n' | grep . | awk '
        { line = line (n % 12 ? " " : "    ") "0x" $1 ","; n++ }
        n % 12 == 0 { print line; line = "" }
        END { if (line != "") print line }'
}

cat > "$out_h" <<'HEADER'
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

HEADER

cat > "$out_c" <<'HEADER'
/* Made by build.sh from the shaders in this folder: do not edit, run
 * the script. */
#include "game/shaders/data.h"
HEADER

for source in $(ls "$source_dir"/*.vert "$source_dir"/*.frag | sort); do
    file=$(basename "$source")
    name=$(echo "$file" | tr '.' '_')
    stage=${file##*.}
    glslangValidator -V --quiet -S "$stage" -o "$work/$file.spv" "$source"
    spirv-cross --msl --stage "$stage" --output "$work/$file.msl" \
        "$work/$file.spv"
    {
        echo
        echo "static const uint8_t ${name}_spirv[] = {"
        bytes "$work/$file.spv"
        echo "};"
        echo
        echo "static const uint8_t ${name}_msl[] = {"
        bytes "$work/$file.msl"
        echo "};"
        echo
        echo "const ShaderData shader_${name} = {"
        echo "    ${name}_spirv, sizeof ${name}_spirv,"
        echo "    ${name}_msl, sizeof ${name}_msl"
        echo "};"
    } >> "$out_c"
    echo "/* $file */" >> "$out_h"
    echo "extern const ShaderData shader_${name};" >> "$out_h"
    echo "$file"
done

{
    echo
    echo "#endif"
} >> "$out_h"
