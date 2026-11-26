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
if(ROM2_SYSTEM_ZLIB)
    find_package(ZLIB REQUIRED)
    set(ROM2_ZLIB ZLIB::ZLIB)
else()
    include(FetchContent)
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
    src/base/reader.c
)
target_include_directories(rom2_core PUBLIC src)
target_link_libraries(rom2_core PRIVATE ${ROM2_ZLIB})
rom2_set_warnings(rom2_core)

# rom2_import: readers for the original game's file formats. Only the
# importer uses them; the game itself never reads an original file.
add_library(rom2_import STATIC
    tools/import/name.c
    tools/import/png.c
    tools/import/reg.c
    tools/import/res.c
    tools/import/sheet.c
    tools/import/sprite256.c
    tools/import/unit_layout.c
    tools/import/unit_sheet.c
    tools/import/units.c
)
target_include_directories(rom2_import PUBLIC tools)
target_link_libraries(rom2_import PUBLIC rom2_core)
rom2_set_warnings(rom2_import)

# res_list: prints the contents of a .res archive.
add_executable(res_list tools/res_list/main.c)
target_link_libraries(res_list PRIVATE rom2_import)
rom2_set_warnings(res_list)

# unit_sheet: writes the sprite sheet of one unit as a PNG picture.
add_executable(unit_sheet tools/unit_sheet/main.c)
target_link_libraries(unit_sheet PRIVATE rom2_import)
rom2_set_warnings(unit_sheet)

# Tests: one program, rom2_tests, built from every test file.
enable_testing()

add_executable(rom2_tests
    tests/main.c
    tests/base/test_buffer.c
    tests/base/test_file.c
    tests/base/test_pack.c
    tests/base/test_reader.c
    tests/import/test_name.c
    tests/import/test_png.c
    tests/import/test_reg.c
    tests/import/test_res.c
    tests/import/test_sheet.c
    tests/import/test_sprite256.c
    tests/import/test_unit_layout.c
    tests/import/test_unit_sheet.c
    tests/import/test_units.c
)
# tests/utest.h is the test framework (github.com/sheredom/utest.h, public
# domain). SYSTEM: it is not held to our warning settings.
target_include_directories(rom2_tests SYSTEM PRIVATE tests)
target_include_directories(rom2_tests PRIVATE tests/import)
target_link_libraries(rom2_tests PRIVATE rom2_core rom2_import)
rom2_set_warnings(rom2_tests)
add_test(NAME rom2_tests COMMAND rom2_tests)
