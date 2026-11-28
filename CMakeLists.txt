cmake_minimum_required(VERSION 3.20)
project(rom2 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Writes build/compile_commands.json, which tells an editor where every
# header is, the downloaded libraries' too.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

# SDL opens the window and reads the keys and the mouse. Like zlib, its
# source is downloaded at an exact version checked by its hash and built
# along with ROM2, unless ROM2_SYSTEM_SDL asks for the system's own. Only
# the game uses it: ROM2_BUILD_GAME=OFF builds the importer and the tests
# alone, with no SDL at all.
option(ROM2_BUILD_GAME "Build rom2, the program a player runs" ON)
option(ROM2_SYSTEM_SDL "Use the SDL3 installed on the system" OFF)
if(ROM2_BUILD_GAME)
    if(ROM2_SYSTEM_SDL)
        find_package(SDL3 REQUIRED CONFIG)
    else()
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
        # Faking key presses for other programs, which needs a package
        # many systems do not have.
        set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(SDL3
            URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.20/SDL3-3.4.20.tar.gz
            URL_HASH SHA256=f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f
            EXCLUDE_FROM_ALL
        )
        FetchContent_MakeAvailable(SDL3)
    endif()
endif()

# rom2_core: code with no dependency on a window, a network or the clock.
add_library(rom2_core STATIC
    src/base/buffer.c
    src/base/file.c
    src/base/pack.c
    src/base/png.c
    src/base/reader.c
    src/base/toml.c
    src/data/cursor.c
    src/data/font.c
    src/data/map.c
    src/data/sprite_sheet.c
    src/data/structure_kind.c
    src/data/unit_animation.c
    src/data/unit_kind.c
    src/mechanics/clock.c
    src/mechanics/idle.c
    src/mechanics/random.c
    src/mechanics/unit.c
    src/mechanics/units.c
    src/view/bars.c
    src/view/camera.c
    src/view/ground.c
    src/view/light.c
    src/view/objects.c
    src/view/pointer.c
    src/view/sprite.c
    src/view/structures.c
    src/view/text.c
    src/view/units.c
)
target_include_directories(rom2_core PUBLIC src)
target_link_libraries(rom2_core PRIVATE ${ROM2_ZLIB})
if(NOT MSVC AND NOT APPLE)
    # The mathematics of the C library, which is apart on some systems.
    target_link_libraries(rom2_core PUBLIC m)
endif()
rom2_set_warnings(rom2_core)

# rom2_formats: readers for the original game's file formats. Only the
# importer uses them; the game itself never reads an original file.
add_library(rom2_formats STATIC
    tools/import/bmp.c
    tools/import/databin.c
    tools/import/frames.c
    tools/import/item.c
    tools/import/cursor.c
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

# rom2: the game, the program a player runs, from src/game.
if(ROM2_BUILD_GAME)
    add_executable(rom2
        src/game/main.c
        src/game/map_screen.c
        src/game/renderer.c
        src/game/shader.c
        src/game/sheets.c
        src/game/shaders/data.c
    )
    target_link_libraries(rom2 PRIVATE rom2_core SDL3::SDL3)
    rom2_set_warnings(rom2)
    # Only a build with the sanitizers has this file.
    if(ROM2_SANITIZE AND NOT MSVC)
        target_sources(rom2 PRIVATE src/game/sanitize.c)
    endif()
endif()

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
    tests/data/test_cursor.c
    tests/data/test_font.c
    tests/data/test_sprite_sheet.c
    tests/data/test_structure_kind.c
    tests/data/test_unit_animation.c
    tests/data/test_unit_kind.c
    tests/mechanics/test_clock.c
    tests/mechanics/test_idle.c
    tests/mechanics/test_random.c
    tests/mechanics/test_unit.c
    tests/mechanics/test_units.c
    tests/view/test_bars.c
    tests/view/test_camera.c
    tests/view/test_ground.c
    tests/view/test_light.c
    tests/view/test_objects.c
    tests/view/test_pointer.c
    tests/view/test_sprite.c
    tests/view/test_structures.c
    tests/view/test_text.c
    tests/view/test_units.c
    tests/import/test_bmp.c
    tests/import/test_cursor.c
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
    tests/data/test_map.c
)
# tests/utest.h is the test framework (github.com/sheredom/utest.h, public
# domain). SYSTEM: it is not held to our warning settings.
target_include_directories(rom2_tests SYSTEM PRIVATE tests)
target_include_directories(rom2_tests PRIVATE tests/import)
target_link_libraries(rom2_tests PRIVATE rom2_core rom2_formats)
rom2_set_warnings(rom2_tests)
add_test(NAME rom2_tests COMMAND rom2_tests)
