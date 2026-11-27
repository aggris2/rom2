# ROM2

An open-source reimplementation of **Rage of Mages II: Necromancer**
(Allods 2, 1999).

ROM2 is an engine only. This repository contains no art, sound, maps or text
from the original game. To play you will need your own copy of the game, for
example from GOG.

This is a fan project. It is not affiliated with or endorsed by Nival or any
holder of the Rage of Mages or Allods trademarks.

## Building

You need a C11 compiler and CMake 3.20 or newer.

ROM2 uses the zlib library. The first `cmake` command downloads zlib's
source, at a fixed version, and builds it with ROM2, so that step needs an
internet connection once. To use the zlib installed on your system instead,
add `-DROM2_SYSTEM_ZLIB=ON` to that command.

```bash
cmake -B build
cmake --build build
ctest --test-dir build
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for how the code is organised, and
[docs/formats](docs/formats/README.md) for the original game's file
formats.

## Importing the game's files

ROM2 does not read the original game's files directly. An importer turns
them into ROM2's own content folder, once. With the original game's
archives, `graphics.res`, `world.res` and the others, in a folder called
`game`:

```bash
./build/rom2_import game content
```

This runs every step of the import in turn. Each step writes one kind of
thing into a folder of `content`:

| Folder | What is in it |
|---|---|
| `units`, `objects`, `structures`, `projectiles`, `spells` | a folder for each thing, with its definition and its pictures |
| `heroes` | a folder for each look of the player's hero, in armour and without |
| `effects` | smoke, a small explosion and other sprites the game shows by itself |
| `items`, `characters` | the items with their pictures, and the characters they are drawn on |
| `terrain` | the ground tiles |
| `interface` | the pictures and sprites of the interface, the menus, the shops and the cursors |
| `fonts` | a folder for each font, with its glyphs in one picture |
| `campaign` | a folder for each mission, with its map and its text |
| `maps` | the maps for several players, which lie loose in the game folder |
| `sounds`, `videos` | the original's files, as they are |
| `texts` | the other texts, as UTF-8 |
| `rules` | small tables of settings: the cost of each kind of ground, and others |

At the end it says how many of the original's files it read, how many
it left out on purpose, such as a spare palette, and how many it has
not read yet. The last two are listed in `content/not_imported.txt`,
each with the reason.

A step can be run alone, and most can write one thing alone, by its ID:

```bash
./build/rom2_import game content units
./build/rom2_import game content units 12
```

The `content` folder is yours and is not part of this repository.

## Licence

GPL-3.0. See [LICENSE](LICENSE).
