# Units: `units.reg` and the frames of a unit

A unit is described in two places. The registry `units/units.reg` in
`graphics.res` has its numbers, and a `.256` sprite file has its frames.
This page says how the two fit together. The registry format itself is in
[reg.md](reg.md) and the sprite format in [256.md](256.md).

Measured on all 61 units of the GOG release.

Read by `tools/import/units.c` and `tools/import/unit_layout.c`.

## The sections of units.reg

63 sections: `Global`, `Files`, and `Unit0` to `Unit60` with no gaps.

- `Global`: `UnitCount` 61, `FileCount` 60.
- `Files`: `File0` to `File59`, texts like `humans\swordsman\swordsman`.
- `UnitN`: one unit. The number in the section name means nothing. The
  unit's number is its `ID`, which is what maps use. The IDs run from 1 to
  107 and are all different.

Keys of a unit, and how many of the 61 units state each:

```
DescText 61  ID 61  File 61  Sound 61  InfoPicture 60
AttackAnimTime 60  AttackAnimFrame 60  AttackDelay 59  InMapEditor 59
Dying 56  AttackPhases 52  Palette 50  DyingPhases 47
MoveAnimTime 46  MoveAnimFrame 46  Index 45  MovePhases 45
MoveBeginPhases 45  Width 45  Height 45  CenterX 45  CenterY 45
SelectionX1 45  SelectionX2 45  SelectionY1 45  SelectionY2 45
BonePhases 42  ShootOffset 25  ShootDelay 23  Projectile 18  Parent 16
TileSize 10  Flip 10  IdlePhases 5  IdleAnimTime 5  IdleAnimFrame 5  Z 3
```

Example, `Unit0`:

```
DescText "Human Swordsman"   ID 3   File 0   Index 0
MovePhases 8   MoveBeginPhases 2   AttackPhases 7
Width 128   Height 128   CenterX 64   CenterY 78
Dying 4   Palette 0   AttackDelay 4
AttackAnimTime  [2, 2, 1, 1, 1, 2, 2]
AttackAnimFrame [0, 1, 2, 3, 4, 5, 6]
```

## Parent

`Parent` is the `ID` of another unit. A value the unit does not state is
taken from its parent. 16 units have a parent. No parent has a parent
itself, and every parent exists.

## Sprite file

`File` is an index into `Files`. The text there is the path of the sprite
inside `units/`, without its ending and with `\` between folders:
`humans\swordsman\swordsman` is `units/humans/swordsman/swordsman.256`.
All 60 files exist.

## Directions and mirroring

A unit either stores every direction, or stores half of them and is
mirrored for the other half. The second kind has `Flip` set.

| | Not flipped | Flipped |
|---|---|---|
| Standing frames, a full turn in 16 steps | 16 | 9 |
| Directions of each animation, a full turn in 8 | 8 | 5 |

For a flipped unit, standing step `s` above 8 is the mirror image of step
`16 - s`, and direction `d` above 4 is the mirror image of direction
`8 - d`. The first and the middle direction are never mirrored.

This was checked by rendering: the flipped Unarmed Fighter, mirrored this
way, turns step for step like the Swordsman, who is not flipped.

## Frame order

The frames in the file come in this order. `D` is 8, or 5 for a flipped
unit.

1. Standing: 16, or 9.
2. Moving: (`MoveBeginPhases` + `MovePhases`) × `D`, the begin frames
   first.
3. Attack: `AttackPhases` × `D`.
4. Dying: `DyingPhases` × `D`.
5. Bones: `BonePhases` × `D`.
6. Idle: `IdlePhases` × `D`. Only 5 units state this.

In each block the frames of one direction are together: all of direction
0, then all of direction 1, and so on.

## Dying frames in another unit's file

`Dying` is the `ID` of the unit whose sprite file holds the dying and
bones frames. A unit's own file has them only when `Dying` is its own
`ID`, or is not stated. That is 41 units. The other 20 use another unit's:
the Swordsman's are in the file of the Swordsman with shield.

For those 20, blocks 4 and 5 are missing from the unit's own file.

Compared with the unit they take the frames from:

- All 20 have the same `Width` and `Height`.
- 13 are humans, and have the same palette.
- 7 are skeletons (IDs 90 to 96, from 89), and their palettes do not
  match. The frames only look right with the palette of the file they are
  in.
- 5 differ in `Flip`. The frames follow the `Flip` of the file they are
  in.

## Frame count

With `R` = 9 and `D` = 5 for a flipped unit, 16 and 8 otherwise, a file
holds

```
R + (MoveBeginPhases + MovePhases + AttackPhases + IdlePhases) * D
```

frames, plus `(DyingPhases + BonePhases) * D` when the dying frames are
its own. This gives the exact number of frames for 59 of the 61 units.

The two that do not fit:

- **ID 2, "Unarmed Fighter with Shield".** It names the same file as ID 1
  but, unlike ID 1, does not set `Flip`. The file has 129 frames, which
  fits ID 1.
- **ID 107, "WarSheep".** The file has 144 frames and the numbers give
  136.

ROM2's importer refuses these two with a message.

## Where a frame sits

A frame is stored cut close around the figure, and frames of one unit have
different sizes. The Swordsman's attack frames go from 32 to 88 pixels
wide. `Width` and `Height` are the size of the box the unit was drawn in,
128 by 128 for the Swordsman, and each frame is the middle of that box.

This was checked by rendering: with every frame placed in the middle of
the box, the body stays in one place through the attack while the frame
grows for the sword swing, and `CenterX`, `CenterY` (64, 78) lands at the
feet.

## Palettes

A sprite file has one palette inside it. The other colourings of a unit
are in `.pal` files, read by `tools/import/pal.c`. Measured on the 159
`.pal` files of `graphics.res`:

- 156 are ordinary 8-bit BMP pictures. The palette is the picture's
  colour table, and the pixels do not matter.
- A monster's folder has `palette.pal`, `palette2.pal`, `palette3.pal`
  and so on. For the three units compared (bat, bee, catapult),
  `palette.pal` has the same colours as the palette inside `sprites.256`,
  and the numbered ones are other colourings.
- `Palette` is how many of them the unit uses: 5 for the goblin, with
  `palette.pal` to `palette5.pal`, and 1 for a unit with one colouring.
  No unit states more than its folder has.
- Humans state `Palette` 0 and have no `.pal` in their folders. They use
  the `human.pal` one folder up, `units/humans/human.pal`, which is not a
  BMP: 16,384 bytes, 16 palettes of 256 entries of 4 bytes, one after the
  other. The 16 differ from each other in 55 entries, and every human
  sprite's own palette is the same as them in the other 201. Rendered,
  the 55 are the cloth that shows the colour of the unit's player.
- Which of a monster's colourings is used is not in these files. It is
  the `face` of the monster's kind in `data.bin`, with 1 for
  `palette.pal`. See [databin.md](databin.md).

## What ROM2 writes for a unit

`import_units` writes a folder for every unit, named after its ID and
its name: `003_human_swordsman`. In it are the unit's definition,
`unit.toml`, two sheets, `sprites.png` and `dying.png`, and the unit's
colourings, `palettes.png`. The sheets are 8-bit indexed PNGs, and each
is a grid of cells of one size.

- **The main sheet.** Row 0 has the 16 standing steps. Rows 1 to 8 are
  the 8 directions, and each has the moving, attack and idle frames one
  after the other.
- **The dying sheet.** 8 rows, one for each direction, with the dying
  frames and then the bones frames. It is a picture of its own because
  these frames may come from another unit's file, with another palette.
  They are copied, so a unit does not depend on another unit's pictures.

Mirrored frames are written out, so every sheet has all 16 steps and all
8 directions.

A cell is the smallest part of the box that holds every frame on that
sheet, kept the same distance from the left and the right edge of the box
so that a mirrored frame fits. The anchor of a sheet is `CenterX`,
`CenterY` counted from the corner of the cell. For the Swordsman the main
sheet has cells of 88 by 72 with the anchor at 44, 50.

`palettes.png` is 256 pixels wide, and every row is one palette: 16
rows for a human, one for each player colour, and for a monster as many
as its `Palette` says. A pixel of a sheet is a number from 0 to 255, and
a row gives the colour of each. The sheets themselves carry the first
row, so they look right in a picture viewer. A unit whose dying frames
come from a unit with other palettes also gets `dying_palettes.png`.

`unit.toml` has the unit's values from the registry, with those of its
parent written out, so a folder does not depend on another unit. The
file is described at the top of `tools/import/unit_toml.h`. Places inside
the unit's box, which are the selection box and the points a shot leaves
from, are written counted from `CenterX`, `CenterY`.

After that the file lists the unit's kinds, the rows of `data.bin` that
name this unit, each with its numbers: a weak and a strong goblin are
two kinds of the one goblin unit. The 59 units have 458 kinds.

59 of the 61 units are written. The two whose frame count does not fit
are left out with a message.

## Not known

- Where the box is drawn relative to the unit's place on the map. That
  needs a comparison with the original game.
- What the `palette_.pal` files are for.
- `TileSize` is 1, 2 or 3 and is stated by large or unusual units (2 for
  the ogre and the catapult, 3 for the dragon). It is probably the number
  of map tiles the unit takes. Not checked.
- `Z` is 96 for the bat, the dragon and the succubus and not stated
  otherwise. It is probably the height a flying unit is drawn at. Not
  checked.
- `Index` is 0 wherever a unit states it.
- Why the WarSheep has 8 more frames than its numbers give.
