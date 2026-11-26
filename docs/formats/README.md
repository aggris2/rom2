# The original game's file formats

These pages describe the files of Rage of Mages II as ROM2's importer
reads them. They were worked out by measuring the files of the GOG
release. A page says what was measured, on how many files, and what is
still not known.

Nothing here is copied from the game. The pages hold layouts and counts,
and a few lines of values as examples.

## Described

| Format | Files | What it is | Page |
|---|---|---|---|
| `.res` | 10 | archives | [res.md](res.md) |
| `.reg` | 17 | registries of named values | [reg.md](reg.md) |
| `.256` | 1,929 | sprites in 256 colours | [256.md](256.md) |
| `.pal` | 159 | palettes for colour variants | [units.md](units.md) |
| | | how a unit's values and frames fit together | [units.md](units.md) |

Described, and not read by ROM2 yet:

| Format | Files | What it is | Page |
|---|---|---|---|
| `data.bin` | 1 | kinds of units, items, spells and buildings | [databin.md](databin.md) |

## Not described yet

| Format | Files | What it is |
|---|---|---|
| `.16a` | 623 | sprites with see-through pixels: inventory, interface |
| `.16` | 3 | fonts |
| `.alm` | 46 | maps |
| `.dat`, `itemname.*` | 8 | small tables |

## Standard formats

| Format | Files | What it is |
|---|---|---|
| `.bmp` | 1,515 | pictures: interface, terrain, menus |
| `.wav` | 1,599 | sound effects, speech and music |
| `.txt` | 68 | texts |
| `.smk` | 8 | Smacker video |

The counts are over the ten archives. `res_list` prints the files of an
archive.
