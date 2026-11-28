# Architecture

This document describes how the ROM2 source is organised. It covers what
exists today and grows with the code.

## The layers

The source is five layers, in this order:

| # | Folder | What it is |
|---|---|---|
| 1 | `src/base` | bytes, files, PNG, TOML |
| 2 | `src/data` | the content as the game holds it: maps and definitions, read into structs |
| 3 | `src/mechanics` | how the game works: units, movement, combat, one tick |
| 4 | `src/view` | what is seen: the state of the game and the camera in, a list of things to draw out |
| 5 | `src/game` | the window, input, the renderer, sound, the loop |

Each layer may use only the layers above it in the table. Code in
`src/mechanics` never touches a file, a clock or the screen, whatever layer
those are in.

Three things follow from that order. Where something belongs is the lowest
layer that has what it needs. Only `src/game` uses SDL, so everything with
logic in it is plain code that unit tests can reach, and `src/game` is kept
thin: it calls the others and talks to the graphics card. And single player
and multiplayer run the same code of `src/mechanics`: a server, when there
is one, is a second program on top of layers 1 to 3.

A frame goes down the same order: input, then the ticks, then the view
writes the list of what to draw, then the renderer draws it.

`src/mechanics` has its clock, its dice and its list of units so far. The
table below says where what is still to come will go, so that the place is
decided before the code is written.

| Thing | Place |
|---|---|
| Units, movement, path finding, combat | `src/mechanics` |
| Making live units from a map file | `src/mechanics`, from what `src/data` read: the file's records are not the live state |
| Commands, such as "move these units there" | types in `src/mechanics`, filled in by `src/game` and checked where they are defined, the same for a local player and for the network |
| The camera, what is on screen, the order of drawing | `src/view` |
| Ground with heights and light, and the fog | a second kind of record in `src/view`, drawn by the renderer |
| The clock, and the last two states for smooth movement | `src/game` |
| Sounds | `src/game`, played from what a tick reports; drawing never starts a sound |
| The main menu | a second file beside the map's in `src/game`, and a switch in `main.c` |
| Saving a game | the format in `src/mechanics`, the writing of the file in `src/game` |

## Layout

```
src/
  base/      small building blocks with no dependencies, and ROM2's own
             two file formats, PNG and TOML
  data/      the content read into structs: a map, a definition
  view/      what is seen: the camera, and the list of things to draw
  game/      rom2, the game: the program a player runs
    shaders/ the shaders, the script that compiles them, and its output
tools/
  import/    the importer: readers for the original game's file formats,
             a module for each kind of thing, and main.c, the program
    steps/   one file for each step of the program
docs/
  formats/   the original game's file formats, as measured
tests/
  utest.h    the test framework, one header
  main.c     the test runner
  expect.h   small checks many tests share
  base/      tests for src/base
  data/      tests for src/data
  mechanics/ tests for src/mechanics
  view/      tests for src/view
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

`src/base/png` and `src/base/toml` are ROM2's own two file formats: a
picture as a PNG, and a definition as TOML, a plain text of names and
values. Both are written by the importer and read by the game. The TOML
reader takes what the writer writes and no more of TOML than that, and
says which line it refused.

`src/data/map` reads a map, an `.alm` file. A map is the one file of the
original that ROM2 takes as it is, so the game reads it and not the
importer. It gives the map's size, ground, heights, objects, structures,
players and units. The map's script is not read yet.

`src/data/sprite_sheet` is the first of the readers of definitions, the
`.toml` files the importer wrote: which file a sprite sheet is, which file
has its soft edges, the size of a cell in it, and the point of a cell that
stands on the map. It also joins the two pictures into what a sheet is when
it is drawn, two numbers for each pixel: which colour of the palette, and
how solid, with the soft edge half see-through. In the content the two stay
separate pictures, each of which can be looked at.

`src/data/structure_kind` reads what the game needs of a building's
definition: the squares it stands on, how high its picture is, and its
sprite sheet.

`src/data/unit_kind` reads a kind of unit from a unit's definition: so far
its ID, which colouring it has and how fast it turns.
`src/data/unit_animation` reads one animation of a unit, such as the beating
of a bat's wings: which columns of the sheet's rows it takes, and the frame
it shows in each tick.

`src/data/cursor` reads a cursor's definition: its two pictures, the size of
a frame, and its tip, the pixel that goes where the mouse is. The tips are
not in any file of the original, so the importer writes them from a table of
its own.

`src/data/font` reads a font's definition: its picture of glyphs in cells,
which character the first one is, and the width of each.

`src/mechanics/clock` turns time that has passed into ticks, the steps the
game happens in: 16 in a second at the speed a game starts on, and from 8 to
32 at the others, as in the original. It is told how much time has passed
and does not look at a clock itself, so a test runs it as the game does. The
view goes by their count too: a tree or a windmill shows the frame of its
animation for the tick, which its sprite sheet lists.

`src/mechanics/units` holds the units of a game that is being played, made
from a map's records when the game starts: from then on the map is only the
ground they stand on. The hero is not among those records: he enters on
the square the map's script names for it, and is drawn from pictures of his
own, those of the type of unit he is armed as, in armour or without. A
unit is named by its slot in the list and by how many units the slot had
before it, so that a slot can be used again and an old name leads to
nothing. `src/mechanics/random` is the game's dice, which follow from their
seed alone, so that the same game plays out the same
everywhere. `unit` is the record of one unit and how it turns: at its kind's
speed and by the shorter way, as in the original. `idle` is what a unit does
that has nothing to do: it stands, and now and then, when the dice say so,
it turns to look another way. A tick of the game is so far a tick of every
unit.

`src/view` turns what there is into what is drawn. `frame.h` is what the
renderer is handed once a frame: the ground, and the sprites that lie on it.
`sprite.h` is the record for one sprite, a part of a sheet at a place on the
screen. `camera` says which part of a map is on the screen and keeps it
inside the map. `light` works out how bright the ground is at each corner of
a square, from its slopes and the angle of the sun. `ground` writes a record
for each square the camera sees, its four corners raised by the heights of
the map and with that light: a square on a slope is not a rectangle, so the
ground has a record of its own and is not made of sprites. `objects` writes
a sprite for each tree or rock the camera can see, standing on the ground at
its height. `structures` writes each building as strips a square high, one
for each row of its base, because something may stand behind a building's
front and before its back; a bridge, whose size the map gives, it lays
out of its nine pieces, a sprite for each square. `units` writes a sprite
for each unit, the standing frame of the way it faces, in the colour of its
player if it is a human and of its kind if it is a monster, and finds the
unit under a point of the screen by the selection box of its sheet.
`pointer` is the cursor the mouse moves: on the edge of the screen it
scrolls the map and shows an arrow that points the way, and its sprite has
the last turn of all. `text` writes a text as the glyphs of a font, spaced
as the original spaces them, over the map and under the pointer. Every
sprite says where it stands, and `sprite` sorts the list into the order it
is drawn: row after row from the top, and in a row from the right. None of
it knows the graphics card, so all of it has unit tests.

`src/game` is the program `rom2`, and the only code that uses SDL. `main.c`
is the window and the loop. `map_screen` is being in a map, the one place of
the game there is so far: it reads the map and the sheets of what is on it,
moves the camera by the keys, asks the view for the frame and hands it to
the renderer. `sheets` reads a sheet from the content and puts it on the
card. Each place still to come, such as the main menu, is a file like
`map_screen`, with the same three functions: open, one frame, close.

`renderer` is the one module that talks to the graphics card, through SDL's
GPU API. It is handed a frame and draws the ground first, then the sprites in
the order of their list. A sheet is on the card as three small things: for
each pixel which colour of the palette it is, for each pixel how solid it is,
and the 256 colours. A shader looks each colour up as it draws, makes it as
bright as the ground's light or the sprite says, up to three times the sheet's
own colours, and lays it over what is there by how solid it is. So sprites
stay 8 bits a pixel on the card. A sheet may have several palettes, and a
sprite says which it is drawn in: one sheet of a swordsman serves every
player's colour. A square of ground on a slope is drawn by the rule of the
original game, in columns one pixel wide and in whole numbers: `ground.frag`
has the rule.

The shaders, the small programs the graphics card runs, are written in
GLSL in `src/game/shaders`. The card does not take them as text, and each
system wants its own form: SPIR-V for Vulkan, on Linux and Windows, and
MSL for Metal, on macOS. `build.sh` in that folder compiles every shader
into both and writes them as C into `data.c` beside them, which is kept
in the repository. So the build needs no shader tools: only someone who
changes a shader runs the script, and commits what it writes.

`tools/import` reads the original game's files: so far the `.res` archives,
the `.reg` registries, the `.256`, `.16a` and `.16` sprites, the `.pal`
palettes, the `.bmp` pictures, the texts and the tables of `data.bin`. It is
the only code that knows those formats. The game itself never opens an
original file. The formats are described in
[docs/formats](docs/formats/README.md). It also writes what the import
produces: ROM2's own content, one folder for each thing. So far that is units,
objects, structures, projectiles, spells and items. A unit's folder has its
definition, `unit.toml`, which also lists the unit's kinds with their numbers,
and its frames laid out as two sprite sheets, grids of cells of one size, in
8-bit indexed PNG pictures: a main sheet and one with its dying frames. Its
colourings are a third picture, with one palette in each row. The player's
hero has no unit of its own: each way it may be armed is a folder laid out as
a unit's, once in armour and once without. An object's folder is the same with
less in it: `object.toml` and one row of frames. A structure's has
`structure.toml` and its pictures, each put together whole from the tiles the
original keeps it in, and its ruin. Next to every sheet is a picture of its
soft edges. A projectile's has `projectile.toml`, a sheet, its palette, and a
picture in shades of grey that says how solid each pixel is, since most
projectiles are fire and light. A spell's has `spell.toml` alone: the original
keeps no picture with a spell's numbers. An item's folder is one shape, such
as a dagger, with `item.toml` and a folder for each of its variants, which has
the variant's picture for the inventory and its pictures as worn on a
character. The materials, the classes and the modifiers, which have no
pictures, are a file each. The ground is one picture of all its tiles, which a
map names by column and row.

The plain pictures of the interface, the menus and the shops are written as
PNG files under the names the original gives them, and the sprites of the
interface and the cursors each as a sheet of their frames with a small
definition. The maps, the sounds and the videos are copied as they are, and
the texts are written as UTF-8. The original's small registries of settings
are written as TOML under the names they have. A mission of the campaign is a
folder with its map and its text. The maps for several players are apart from
those.

`tools/import` has three sorts of module. One for each of the original's
formats: `res`, `reg`, `databin`, `sprite256`, `sprite16a`, `sprite16`,
`bmp`, `pal` and `text`. One for each kind of thing, with everything
about it: reading its values, laying out its pictures and writing its
definition. Those are `unit`, `object`, `structure`, `projectile`, `item`
and `spell`. And a few the kinds share: `things`, `sheet`, `frames`,
`plain_sheet`, `name` and `reg_toml`.

One program runs it, `import`: `tools/import/main.c`. Each kind of
thing is a step, in a file of its own in `tools/import/steps`: `units.c`,
`objects.c` and so on, with `steps.h` naming them all and `shared.c` holding
what they share. The steps are the part that touches the file system and
prints; the modules beside `main.c` work on bytes in memory, and are what
the tests cover. `main.c` opens the original's archives once, runs the steps
in turn and hands each the archives it needs. An archive notes every file a
step reads, so at the end the program can say which of the original's files
no step has read yet. That list is how we know what is left to import. The
few files that are left out knowingly, such as a spare palette, are named in
`main.c` with the reason, and the program counts them apart. It also writes
a list of every file in the archives, with its size and the step that reads
it.

## Libraries

`rom2_core` is a static library built from `src/base` and `src/data`. It
depends on the C standard library and on zlib.

zlib and SDL are the outside libraries so far. The build downloads the
source of each at an exact version, checked by its hash, and builds it
with ROM2, so every build uses the same zlib and the same SDL. Nothing of
either is kept in this repository. Only the game uses SDL.

`rom2_formats` is a static library built from the modules of
`tools/import`, without the program and its steps. It depends on
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
