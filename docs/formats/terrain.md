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

So picture 17 is `tile2-01.bmp`. Checked on all 1,152,512 squares of
the 58 maps: all but 5 name a tile that exists. The top bits are 2,
which is bit 13 alone, in 1,927 squares.

## What kind of ground a tile is

The kind of ground is worked out from the number itself. The pictures
come in families of four, so the bits read again as:

| Bits | Meaning |
|---|---|
| 0 to 3 | the tile in its picture, 0 to 13 |
| 4 to 5 | the picture in its family, 0 to 3 |
| 6 to 9 | the family, 0 to 12 |
| 13 | this square cannot be walked on, whatever its ground |

There are ten kinds of ground, with the cost of walking on each as
`data/map.reg` in `world.res` gives it:

| Kind | Cost | Kind | Cost |
|---|---|---|---|
| 1 land | 8 | 6 stones | 12 |
| 2 grass | 8 | 7 savanna | 11 |
| 3 flowers | 9 | 8 mountain | 16 |
| 4 sand | 14 | 9 water | 8 |
| 5 cracked | 6 | 10 road | 6 |

A family is two kinds of ground meeting, a first and a second:

| Family | First, second | Family | First, second |
|---|---|---|---|
| 0 | grass, land | 5 | cracked, stones |
| 1 | cracked, land | 6 | flowers, savanna |
| 2 | sand, land | 7 | mountain, stones |
| 3 | savanna, land | 8 to 11 | water |
| 4 | stones, land | 12 | road, land |

How much of a tile is the first kind is a number from 1, none of it, to
5, all of it. It depends on the picture in the family and the tile in
the picture:

```
tile       0  1  2  3  4  5  6  7  8  9 10 11 12 13
picture 0  2  3  2  4  3  4  2  2  2  2  4  4  4  4
picture 1  3  5  3  3  1  3  2  4  2  2  4  2  4  4
picture 2  2  3  2  4  3  4  2  4  2  2  4  2  4  4
picture 3  5  5  5  5  5  5  2  2  2  2  4  4  4  4
```

The square counts as the first kind if that number is above 2, and as
the second if not. Its cost is the two costs mixed by it: the second's
alone for 1, three parts second to one of first for 2, half and half
for 3, three parts first for 4, the first's alone for 5.

In the families of water a tile is water, but for tile 4 of picture 1,
which is land.

A square cannot be walked on if bit 13 is set, if it counts as
mountain, if it is of a family of water, or if an object stands on it.
The 8 squares along each edge of a map cannot be walked on either.

These rules and the two tables are the original program's, taken from
earlier work on it, and were then checked on the 58 maps:

- Of the 7,665 units the maps place, 423 stand on water or mountain.
  421 of them are of the five kinds that fly: bee, bat, dragon,
  succubus and ghost. All but 10 units stand inside the edge of 8
  squares.
- Of the 3,303 structures, 58 bridges have the first square of their
  base on water, and 18 others that are larger than one square. No
  structure stands in the edge.
- Drawn over mission 10, the water lies on its river and the mountain
  on its rock, with a way through to the tower.

## What ROM2 writes

The importer writes, in `content/terrain`:

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

- Whether the game goes by the cost of a square alone, or also by what
  it is, when a unit picks its way.
- What `dirt.bmp` is used for.
- The 5 squares that name a tile that is not there.
