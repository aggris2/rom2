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
| `.16a` | 623 | sprites with see-through pixels | [16a.md](16a.md) |
| `.pal` | 159 | palettes for colour variants | [units.md](units.md) |
| `data.bin` | 1 | kinds of units, items, spells, buildings | [databin.md](databin.md) |
| | | how a unit's values and frames fit together | [units.md](units.md) |
| | | how an object's values and frames fit together | [objects.md](objects.md) |
| | | how a structure's values and tiles fit together | [structures.md](structures.md) |
| | | how a projectile's values and frames fit together | [projectiles.md](projectiles.md) |
| | | how items are put together, and their pictures | [items.md](items.md) |
| `.bmp` | 53 | the ground tiles, in `terrain/` | [terrain.md](terrain.md) |
| `.bmp` | 1,462 | plain pictures: interface, menus, shops | [pictures.md](pictures.md) |
| `.alm` | 46 | maps: the layout and the ground | [alm.md](alm.md) |

## Not described yet

| Format | Files | What it is |
|---|---|---|
| `.16` | 3 | fonts |
| `.dat`, `itemname.*` | 8 | small tables |

## Standard formats

| Format | Files | What it is |
|---|---|---|
| `.wav` | 1,599 | sound effects, speech and music |
| `.txt` | 68 | texts |
| `.smk` | 8 | Smacker video |

The sounds are all at 16 bits, one channel for effects and speech and two
for music, at 22,050 samples a second but for three at 44,100. 560 of
them are plain PCM. The other 1,039, most of the speech and 53 effects,
are packed as Microsoft ADPCM, 4 bits for each sample.

The texts are in the Windows code page 1251 and end their lines with a
carriage return and a line feed. They are English and nearly all ASCII:
five have a few Cyrillic letters left in them.

The importer copies the maps, the sounds and the videos as they are, and
writes the texts as UTF-8 with plain line ends.

The counts are over the ten archives. `res_list` prints the files of an
archive.
