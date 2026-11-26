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

## Licence

GPL-3.0. See [LICENSE](LICENSE).
