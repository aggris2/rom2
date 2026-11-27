cmake_minimum_required(VERSION 3.20)
project(rom2 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(ROM2_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

# Warnings are errors in our own code.
function(rom2_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion
            -Wstrict-prototypes -Werror)
    endif()
endfunction()

# The Windows compiler calls fopen() and its relatives deprecated and
# suggests its own replacements, which no other system has.
if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# -fno-sanitize-recover makes the program stop at the first thing the
# sanitizers find, so a test run cannot pass with a finding in its output.
if(ROM2_SANITIZE AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined
        -fno-sanitize-recover=all -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# zlib does the compression inside PNG pictures. Its source is downloaded
# and built along with ROM2, at an exact version checked by its hash, so
# that every build of ROM2 uses the same zlib and packs data to the same
# bytes. ROM2_SYSTEM_ZLIB is for those who package ROM2 for a system and
# want the system's own copy instead.
option(ROM2_SYSTEM_ZLIB "Use the zlib installed on the system" OFF)
# Give downloaded files the time they were unpacked, as newer CMake asks.
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
include(FetchContent)
if(ROM2_SYSTEM_ZLIB)
    find_package(ZLIB REQUIRED)
    set(ROM2_ZLIB ZLIB::ZLIB)
else()
    set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(zlib
        URL https://github.com/madler/zlib/releases/download/v1.3.2/zlib-1.3.2.tar.gz
        URL_HASH SHA256=bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16
    )
    FetchContent_MakeAvailable(zlib)
    # zlib's headers are not held to our warning settings.
    get_target_property(ROM2_ZLIB_INCLUDES zlibstatic
        INTERFACE_INCLUDE_DIRECTORIES)
    set_target_properties(zlibstatic PROPERTIES
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${ROM2_ZLIB_INCLUDES}")
    set(ROM2_ZLIB ZLIB::ZLIBSTATIC)
endif()

# rom2_core: code with no dependency on a window, a network or the clock.
add_library(rom2_core STATIC
    src/base/buffer.c
    src/base/file.c
    src/base/pack.c
    src/base/png.c
    src/base/reader.c
    src/base/toml.c
    src/map/alm.c
)
target_include_directories(rom2_core PUBLIC src)
target_link_libraries(rom2_core PRIVATE ${ROM2_ZLIB})
rom2_set_warnings(rom2_core)

# rom2_formats: readers for the original game's file formats. Only the
# importer uses them; the game itself never reads an original file.
add_library(rom2_formats STATIC
    tools/import/bmp.c
    tools/import/databin.c
    tools/import/frames.c
    tools/import/item.c
    tools/import/name.c
    tools/import/object.c
    tools/import/pal.c
    tools/import/plain_sheet.c
    tools/import/projectile.c
    tools/import/reg.c
    tools/import/reg_toml.c
    tools/import/res.c
    tools/import/sheet.c
    tools/import/spell.c
    tools/import/sprite16.c
    tools/import/sprite16a.c
    tools/import/sprite256.c
    tools/import/structure.c
    tools/import/text.c
    tools/import/things.c
    tools/import/unit.c
)
target_include_directories(rom2_formats PUBLIC tools)
target_link_libraries(rom2_formats PUBLIC rom2_core)
rom2_set_warnings(rom2_formats)

# import: the program that writes the original game's files as ROM2
# content. Each kind of thing is a step, in a file of its own in
# tools/import/steps. The target is called import_program, and the file
# it builds is import.
add_executable(import_program
    tools/import/main.c
    tools/import/steps/copies.c
    tools/import/steps/fonts.c
    tools/import/steps/interface.c
    tools/import/steps/items.c
    tools/import/steps/objects.c
    tools/import/steps/projectiles.c
    tools/import/steps/shared.c
    tools/import/steps/spells.c
    tools/import/steps/structures.c
    tools/import/steps/terrain.c
    tools/import/steps/units.c
)
set_target_properties(import_program PROPERTIES OUTPUT_NAME import)
target_link_libraries(import_program PRIVATE rom2_formats)
rom2_set_warnings(import_program)

# Tests: one program, rom2_tests, built from every test file.
enable_testing()

add_executable(rom2_tests
    tests/main.c
    tests/base/test_buffer.c
    tests/base/test_file.c
    tests/base/test_pack.c
    tests/base/test_png.c
    tests/base/test_reader.c
    tests/base/test_toml.c
    tests/import/test_bmp.c
    tests/import/test_databin.c
    tests/import/test_item.c
    tests/import/test_name.c
    tests/import/test_object.c
    tests/import/test_pal.c
    tests/import/test_plain_sheet.c
    tests/import/test_projectile.c
    tests/import/test_reg.c
    tests/import/test_reg_toml.c
    tests/import/test_res.c
    tests/import/test_sheet.c
    tests/import/test_spell.c
    tests/import/test_sprite16.c
    tests/import/test_sprite16a.c
    tests/import/test_sprite256.c
    tests/import/test_structure.c
    tests/import/test_text.c
    tests/import/test_things.c
    tests/import/test_unit.c
    tests/map/test_alm.c
)
# tests/utest.h is the test framework (github.com/sheredom/utest.h, public
# domain). SYSTEM: it is not held to our warning settings.
target_include_directories(rom2_tests SYSTEM PRIVATE tests)
target_include_directories(rom2_tests PRIVATE tests/import)
target_link_libraries(rom2_tests PRIVATE rom2_core rom2_formats)
rom2_set_warnings(rom2_tests)
add_test(NAME rom2_tests COMMAND rom2_tests)
