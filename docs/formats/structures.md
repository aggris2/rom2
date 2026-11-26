# Structures: `structures.reg` and the tiles of a structure

A structure is a building: a hut, a windmill, a tower, a bridge. The
registry `structures/structures.reg` in `graphics.res` lists them,
`.256` sprite files hold their pictures, and `data.bin` has their
numbers. The registry format is in [reg.md](reg.md), the sprite format in
[256.md](256.md) and `data.bin` in [databin.md](databin.md).

Measured on all 179 structures of the GOG release.

Read by `tools/import/structures.c`.

## The sections of structures.reg

180 sections: `Global`, with `Count` 179, and `Structure0` to
`Structure178`. A structure's number is its `ID`, from 1 to 179 with
none missing. No structure has a parent.

Every structure states these:

```
DescText  ID  File  TileWidth  TileHeight  FullHeight  Phases
SelectionX1  SelectionX2  SelectionY1  SelectionY2  ShadowY
AnimMask  AnimFrame  AnimTime  Picture
```

and some state these:

```
Indestructible 81  IconID 34  Usable 32  Flat 12  LightRadius 5
LightPulse 4  VariableSize 2
```

`File` is a text, the path of the sprite inside `structures/` without
its ending: `hut1\house` is `structures/hut1/house.256`. A few are
written with a capital, `Inn3\house`, and the archive has every name in
lower case. 163 files serve the 179 structures: 16 are used by two.

## A structure is tiles

A structure is not one picture. Its sprite file holds tiles of 32 by 32
pixels, like the ground: all 4,886 frames of all structure files are
exactly that size.

`TileWidth` and `TileHeight` are the tiles the structure stands on.
`FullHeight` is how many tiles high its picture is, which is never less
than `TileHeight`: a tower stands on a few tiles and rises over the ones
behind it. The picture is `TileWidth` by `FullHeight` tiles, stored row
after row.

## Animation and ruin

`Phases` is the number of pictures of the structure's animation, and
`AnimMask` is a text with one mark for each tile of the picture: `+` for
a tile that changes during the animation, `-` for one that does not. The
church has 20 tiles and the mask `--------++----------`: only its bell
moves.

The file holds, in this order:

1. The tiles of the first picture: `TileWidth * FullHeight`.
2. For each further picture, only the tiles marked `+`, in the order
   they have in the picture.
3. For a structure that can be destroyed, all the tiles of its ruin.

So a file has `W * H + P * (Phases - 1)` tiles, where `P` is the number
of `+` marks, and `W * H` more if there is a ruin. A structure with
more than one phase and an empty mask changes all its tiles.

This gives the number of tiles of 175 of the 179 files: 69 without a
ruin and 106 with one. It was checked by putting the tiles of the
windmill and the church together this way: the pictures are whole, the
sails and the bell move, and the last set is the building in ruins.

`AnimFrame` and `AnimTime` are lists of the same length: which picture
to show and for how long. A structure without them has an empty text in
their place. 33 structures have the lists.

## What does not fit

- **Bridges.** The 2 structures with `VariableSize` are 1 tile by 1 and
  have 9 and 14 tiles in their files. They are pieces to be laid out by
  the game, not a picture.
- **Plagat Inn 3**, IDs 45 and 69, which share a file. `Phases` is 2 and
  the file has the tiles of one picture.
- **11 structures have 2 pictures and no lists**: the other inns, the
  guard towers, the training schools, the mage's tower and the castle.
  When the second picture is shown is not known.

## The numbers in data.bin

The buildings table of `data.bin` has 179 rows, and row `n`, counted
from 1, is the structure with `ID` `n`. The names agree, and so do the
sizes: `sizeX` and `sizeY` are `TileWidth` and `TileHeight` for 177 of
the 179. The cave and the castle differ by one in `sizeY`.

The row's other columns are `scanRange`, `healthMax`, `Passability` and
`BuildingPresent`.

## What ROM2 writes for a structure

The importer writes a folder for every structure, named after
its ID and its name: `005_windmill_1`. In it are `structure.toml`,
described at the top of `tools/import/structure_toml.h`, and these
pictures:

- `sprites.png`: the pictures of the animation in one row, each put
  together whole from its tiles. For a bridge, its pieces.
- `ruin.png`: the ruin, for a structure that has one.
- `edges.png` and `ruin_edges.png`: the soft edges of those two.

`structure.toml` has the registry's values and the numbers of the
structure's row in `data.bin`. Plagat Inn 3 is written with its one
picture.

All 179 structures are written.

## Not known

- The meaning of `ShadowY`, `Picture`, `IconID`, `Flat`, `Usable`,
  `LightRadius` and `LightPulse` beyond their names.
- How `Passability` and `BuildingPresent` are read. They look like one
  bit for each tile the structure stands on: 55 and 63 for a hut of 3
  by 2.
- When a structure with 2 pictures and no lists shows the second.
