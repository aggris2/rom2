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

if(ROM2_SANITIZE AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# rom2_core: code with no dependency on a window, a network or the clock.
add_library(rom2_core STATIC
    src/base/reader.c
    src/base/file.c
)
target_include_directories(rom2_core PUBLIC src)
rom2_set_warnings(rom2_core)

# rom2_import: readers for the original game's file formats. Only the
# importer uses them; the game itself never reads an original file.
add_library(rom2_import STATIC
    tools/import/res.c
)
target_include_directories(rom2_import PUBLIC tools)
target_link_libraries(rom2_import PUBLIC rom2_core)
rom2_set_warnings(rom2_import)

# Tests: one program, rom2_tests, built from every test file.
enable_testing()

add_executable(rom2_tests
    tests/main.c
    tests/base/test_file.c
    tests/base/test_reader.c
    tests/import/test_res.c
)
# tests/utest.h is the test framework (github.com/sheredom/utest.h, public
# domain). SYSTEM: it is not held to our warning settings.
target_include_directories(rom2_tests SYSTEM PRIVATE tests)
target_link_libraries(rom2_tests PRIVATE rom2_core rom2_import)
rom2_set_warnings(rom2_tests)
add_test(NAME rom2_tests COMMAND rom2_tests)
