# Projectiles: `projectiles.reg` and their frames

A projectile is what flies from an attacker to its target, and what
shows where a spell lands: an arrow, a catapult's stone, a fire bolt, a
healing sparkle. The registry `projectiles/projectiles.reg` in
`graphics.res` lists them. The registry format is in [reg.md](reg.md).

Measured on all 31 projectiles of the GOG release.

Read by `tools/import/projectiles.c`.

## The sections of projectiles.reg

`Global`, with `Count` 31, and `Projectile0` to `Projectile30`. A
projectile's number is its `ID`, from 1 to 64 with gaps. No projectile
has a name or a parent.

Keys, and how many of the 31 state each:

```
ID 31  File 31  Phases 31  Palette 28  A16 24  SFX 23  Width 22
Height 22  RotationPhases 22  Homing 14  Flip 3
```

## Sprite file

`File` is a text, the path of the sprite inside `projectiles/` without
its ending: `archer\arrow`. Three are written with a capital and the
archive has every name in lower case.

A projectile with `A16` has a [.16a](16a.md) file, with pixels that are
solid to different degrees: all the spells, 24 of them. The other 7,
the arrows and the catapults' stones, have a [.256](256.md) file with
its `b` file of soft edges.

Two projectiles, IDs 6 and 7, share a file.

## Palette

`Palette` is 1 for 28 projectiles and missing for 3: the arrows of the
archer, the crossbowman and the orc. Those 3 have sprite files without a
palette of their own. That is half of the 6 `.256` files without one,
and the other half are their `b` files.

Next to the registry are two palettes, `projectiles.pal` and
`projectile_.pal`, both BMP pictures as [units.md](units.md) describes.
ROM2 draws the three arrows with `projectiles.pal`. That choice is by
the name and by how the arrows look with each. It was not checked in
the original game.

So `Palette` here does not count colourings, as it does for units. No
projectile has more than one.

## Frames

`Phases` is the number of pictures of the projectile's animation. A
projectile may also turn to face where it flies, and then the file has
its pictures for each direction, all of the first direction, then all
of the next.

The number of directions in the file:

- `RotationPhases`, if stated. It is 1 on all 22 that state it: these
  do not turn.
- Otherwise 9 for a projectile with `Flip`, and 16 for one without.

A projectile with `Flip` is mirrored for the other 7 directions, as a
unit is: direction `s` above 8 is the mirror image of direction
`16 - s`. This was checked by drawing the fire bolt's 36 frames: 9
directions of 4 pictures each, turning from straight up to straight
down.

`Phases` times the directions is the number of frames of the file for
30 of the 31.

The box a projectile is drawn in is `Width` by `Height`, and 64 by 64
for the 9 that do not state them. A frame is the middle of the box. The
registry has no centre: it is the middle too.

## What does not fit

- **The healing sparkle, ID 56,** has 7 phases and 8 frames in its
  file. ROM2 writes the first 7.
- **5 sprite files are named by no projectile:** `diamonddust`,
  `freeze`, `smallxpl`, `smoke0` and `smoke1`. The game must use them
  directly. ROM2 does not write them yet.

## What ROM2 writes for a projectile

`import_projectiles` writes a folder for every projectile, named after
its ID and its file: `010_firebolt_sprites`. In it are
`projectile.toml`, described at the top of
`tools/import/projectile_toml.h`, and three pictures:

- `sprites.png`: the colour of each pixel as a number, an 8-bit indexed
  PNG as for a unit. It has a row for each direction, 16 or 1, with the
  mirrored ones written out, and in each row the pictures of the
  animation. A cell is the whole box.
- `alpha.png`: a picture in shades of grey with the same cells, which
  says how solid each pixel is, from black for not there to white for
  solid.
- `palettes.png`: the colours of the numbers, one palette in a row. The
  original has one for each projectile. Another row is another
  colouring, so a projectile can be recoloured as a unit can.

A `.16a` pixel is as solid as its file says, its 15 levels spread evenly
up to white. A `.256` projectile has its figure solid and its soft edge
half solid, both on the one sheet.

In a `.16a` file colour 0 is a colour like any other, so a pixel is
there if `alpha.png` says so, whatever its number. 809 pixels of the 31
sheets are of colour 0 and are there. A picture viewer shows
`sprites.png` without them and with hard outlines: the look comes from
the two pictures together.

All 31 projectiles are written.

## Not known

- How long each picture of an animation is shown. The registry has no
  times.
- The meaning of `SFX` and `Homing` beyond their names.
- What `projectile_.pal` is for.
