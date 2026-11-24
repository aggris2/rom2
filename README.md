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

```bash
cmake -B build
cmake --build build
ctest --test-dir build
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for how the code is organised.

## Licence

GPL-3.0. See [LICENSE](LICENSE).
