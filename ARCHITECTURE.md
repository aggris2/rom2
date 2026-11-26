# Architecture

This document describes how the ROM2 source is organised. It covers what
exists today and grows with the code.

## The core and the shell

The game rules will be a library that runs one fixed tick at a time. It has
no window, no network, no files and no clock. Everything else is a shell
around it: the shell feeds commands in and shows what comes out.

Two things follow from that split. The rules can be tested without a window
or a network. And single player and multiplayer run the same rules code.

## Layout

```
src/
  base/      small building blocks with no dependencies
tools/
  import/    readers for the original game's file formats
tests/
  utest.h    the test framework, one header
  main.c     the test runner
  base/      tests for src/base
  import/    tests for tools/import
```

`src/base/reader` is the checked binary reader. Every parser reads through
it instead of touching a pointer. A read past the end fails, and the
failure sticks, so a parser checks once at the end.

`src/base/buffer` is a block of bytes that grows as bytes are added. A
file to be written is built in one first.

`src/base/file` reads a whole file from disk into memory and writes one
back. It is the only code in `base` that touches the file system.

`src/base/pack` packs and unpacks data in the zlib format, the lossless
compression inside a PNG, and has the CRC that goes with it. The work is
done by the zlib library, and this is the only file that calls it.

`tools/import` reads the original game's files: so far the `.res`
archives, the `.reg` registries and the `.256` sprites. It is the only
code that knows those formats. The game itself never opens an original
file. It also writes what the
import produces, which so far is 8-bit indexed PNG pictures: a unit's
frames are laid out as sprite sheets, grids of cells of one size. A unit
has two, a main sheet and one with its dying frames.

Two small programs let you look at what the importer reads. `res_list`
prints the files in an archive, and `unit_sheet` writes the sprite sheets
of one unit.

## Libraries

`rom2_core` is a static library built from `src/base`. It depends on the C
standard library and on zlib.

zlib is the one outside library so far. The build downloads its source at
an exact version, checked by its hash, and builds it with ROM2, so every
build uses the same zlib. Nothing of zlib is kept in this repository.

`rom2_import` is a static library built from `tools/import`. It depends on
`rom2_core` and nothing else.

## Naming

- Files are lowercase: `base/reader.c`, `base/reader.h`.
- Functions are `snake_case` and start with the name of their file or
  module: `reader_u8`.
- Types are `CamelCase`: `Reader`.
- Functions used in one file are `static`.
- Headers sit next to their sources and are included with their folder:
  `#include "base/reader.h"`.
- Lines are at most 80 characters. A parameter list that does not fit
  breaks between parameters, and the next line is indented four spaces.

## Tests

Tests mirror the source folders: `tests/base/test_reader.c` tests
`src/base/reader.c`. All test files are built into one program,
`rom2_tests`, using the utest.h framework. A test is named as a statement
that is true when it passes.

New code comes with tests. A parser comes with tests for input that is too
short.

Tests never read a file of the original game. A test that needs an archive
or a sprite builds a small one from bytes written in the test.
