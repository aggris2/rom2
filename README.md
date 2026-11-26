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
them into ROM2's own content folder, once. So far it imports the units,
the objects, the structures, the projectiles, the spells and the items.
With the original game's files in a folder called `game`:

```bash
./build/import_units game/graphics.res game/world.res content
./build/import_objects game/graphics.res content
./build/import_structures game/graphics.res game/world.res content
./build/import_projectiles game/graphics.res content
./build/import_spells game/world.res content
./build/import_items game/graphics.res game/world.res content
```

This writes a folder for each unit, object, structure, projectile, spell
and item into a folder of its kind, such as `content/units`, with its
definition and its pictures. The `content` folder is yours and is not
part of this repository.

## Licence

GPL-3.0. See [LICENSE](LICENSE).
