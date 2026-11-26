# Terrain and the map's ground

The ground of a map is tiles of 32 by 32 pixels. The pictures of the
tiles are `.bmp` files in `terrain/` in `graphics.res`, and a map says
for each of its squares which tile lies there. The map file is
described in [alm.md](alm.md).

Measured on the GOG release.

Read by `tools/import/bmp.c`.

## The tile pictures

53 files, all BMP pictures of 8 bits for each pixel, not compressed,
with their rows stored from the bottom up:

```
terrain/tile1-00.bmp ... tile1-15.bmp
terrain/tile2-00.bmp ... tile2-15.bmp
terrain/tile3-00.bmp ... tile3-15.bmp
terrain/tile4-00.bmp ... tile4-03.bmp
terrain/dirt.bmp
```

A tile picture is 32 pixels wide and holds its tiles one under the
other: 14 of them, 448 pixels high, or for the 16 pictures of `tile3`
8 of them, 256 high. That is 632 tiles in the 52 pictures.

The 52 tile pictures have one palette, the same in each. Index 0 is a
colour like the others: the ground has no holes. `dirt.bmp`, 32 by 128,
has a palette of its own.

## How a map names a tile

A square of a map is a number of 16 bits:

| Bits | Meaning |
|---|---|
| 0 to 3 | which tile of the picture, from the top, counted from 0 |
| 4 to 11 | which picture, counted from 0 in the order above |
| 12 to 15 | 0, or 2 for a few squares |

So picture 17 is `tile2-01.bmp`. Checked on all 812,288 squares of the
46 maps: all but 5 name a tile that exists. In 1,654 squares the top
bits are 2, which is taken to mark ground that cannot be walked on.
That reading is from an earlier reader and was not checked in the game.

The pictures of one group are the same ground with other edges, for
where two kinds of ground meet. Which tile is which kind of ground,
and what it costs to walk on, is not in these files.

## What ROM2 writes

`import_terrain` writes, in `content/terrain`:

- `tiles.png`: every tile in one picture, an 8-bit indexed PNG with the
  shared palette and no transparent index. It has a column for each of
  the 52 pictures and in it that picture's tiles from the top down. A
  map's two numbers are the column and the row.
- `dirt.png`: the dirt tiles, as they are.
- `terrain.toml`: the size of a tile, the number of columns, and how
  many tiles each column has.

This was checked by drawing the ground of a map from `tiles.png` and
its objects from `content/objects` on it: the ground is whole, with a
road through a wood.

## Not known

- Which kind of ground each tile is, and what follows from it.
- What `dirt.bmp` is used for.
- The 5 squares that name a tile that is not there.
