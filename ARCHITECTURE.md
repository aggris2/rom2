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
docs/
  formats/   the original game's file formats, as measured
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

`src/base/file` reads a whole file from disk into memory, writes one
back, and makes folders. It is the only code in `base` that touches the
file system.

`src/base/pack` packs and unpacks data in the zlib format, the lossless
compression inside a PNG, and has the CRC that goes with it. The work is
done by the zlib library, and this is the only file that calls it.

`tools/import` reads the original game's files: so far the `.res` archives,
the `.reg` registries, the `.256` and `.16a` sprites, the `.pal` palettes,
the `.bmp` pictures of the ground and the tables of `data.bin`. It is the
only code that knows those formats. The game itself never opens an original
file. The formats are described in [docs/formats](docs/formats/README.md).
It also writes what the import produces: ROM2's own content, one folder for
each thing. So far that is units, objects, structures, projectiles, spells
and items. A unit's folder has its definition, `unit.toml`, which also
lists the unit's kinds with their numbers, and its frames laid out as two
sprite sheets, grids of cells of one size, in 8-bit indexed PNG pictures: a
main sheet and one with its dying frames. Its colourings are a third
picture, with one palette in each row. An object's folder is the same with
less in it: `object.toml` and one row of frames. A structure's has
`structure.toml` and its pictures, each put together whole from the tiles
the original keeps it in, and its ruin. Next to every sheet is a picture of
its soft edges. A projectile's has `projectile.toml`, a sheet, its palette,
and a picture in shades of grey that says how solid each pixel is, since
most projectiles are fire and light. A spell's has `spell.toml` alone: the
original keeps no picture with a spell's numbers. An item's folder is one
shape, such as a dagger, with `item.toml` and a folder for each of its
variants, which has the variant's picture for the inventory and its
pictures as worn on a character. The materials, the classes and the
modifiers, which have no pictures, are a file each. The ground is one
picture of all its tiles, which a map names by column and row.

A few small programs run it. `res_list` prints the files in an archive.
`import_units`, `import_objects`, `import_structures`,
`import_projectiles`, `import_spells` and `import_items` each write the
folders of all the things of their kind, and most can write one alone.
`import_terrain` writes the ground.

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
