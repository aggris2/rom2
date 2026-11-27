# Pictures and sprites of the interface

The pictures of the interface, of the menus and of the shops are
ordinary `.bmp` files. There are 1,462 of them besides the 53 of the
ground, which [terrain.md](terrain.md) describes.

Measured on the GOG release.

Read by `tools/import/bmp.c`.

## Where they are

| Archive | Folder | Pictures | What they are |
|---|---|---|---|
| `graphics.res` | `interface/` | 1,076 | the windows, buttons and panels |
| `graphics.res` | `infowindow/` | 185 | the large pictures of units and buildings |
| `graphics.res` | `global.map/` | 11 | the map of the campaign |
| `main.res` | `graphics/` | 34 | the menus and the halls |
| `movies.res` | `shop_druid/`, `shop_kaarg/`, `shopanim/` | 156 | the frames of the shopkeepers |

## The files

All are BMP pictures that are not compressed, with their rows stored
from the bottom up and each row filled out to a multiple of 4 bytes.
None is larger than 640 by 480.

| Bits for each pixel | Pictures | Notes |
|---|---|---|
| 24 | 1,403 | blue, green, red for each pixel |
| 8 | 59 | each pixel is an index into the picture's own palette |

A BMP picture has no see-through pixels. How the game draws the ones
that are not rectangles, such as the buttons, is not measured yet.

## What the importer writes

The importer writes them all into `content/interface`, each as a PNG
under the path it has in its archive below the folder named here:

| Archive | Folder | Goes to |
|---|---|---|
| `graphics.res` | `interface/` | `interface/` |
| `graphics.res` | `infowindow/` | `interface/infowindow/` |
| `graphics.res` | `global.map/` | `interface/global.map/` |
| `main.res` | `graphics/` | `interface/` |
| `movies.res` | all of it | `interface/` |

So `interface/bookopened.bmp` in `graphics.res` becomes
`interface/bookopened.png`. The original splits some folders over two
archives, such as `shop_kaarg`, and they come together here. No two
pictures come to the same place.

A picture of 24 bits becomes a PNG in full colour, one of 8 bits an
8-bit indexed PNG with the same palette and the same indices. Every
pixel of all 1,462 was compared with its original and is the same.

## The sprites

Next to the pictures the interface has 139 sprite files, `.256` and
`.16a`, which no registry describes: the cursors, the frames of the
windows, and the things that move in the town and the inn.

| Folder of `graphics.res` | Sprites | What they are |
|---|---|---|
| `interface/` | 85 | window frames, buttons, the people and carts of the town |
| `cursors/` | 46 | the cursors: 23 `.16a`, and 23 older `.256` |
| `global.map/` | 6 | the flags on the map of the campaign |
| `backpack/` | 2 | the backpack that lies on the ground, and its soft edges |

The frames of one file are mostly all of one size. Two files have
frames of several sizes, `interface/lm.256` and `interface/radiob.256`.
The largest is `interface/town_druid/bug/sprites.16a`, 183 frames of
640 by 268.

Four of the `.256` cursors are empty files: `cast`, `defend`, `move`
and `patrol`. Two more, `attack.256` and `pickup.256`, are broken
files. Their pixels are three bytes of colour each, where a `.256` has
one byte that is an index, but a frame is still counted out as one byte
for a pixel: so a frame of 32 by 32 holds 1,024 bytes, which is the top
third of its picture, and reads as noise. Laid out 96 bytes to a row,
the first frame of `attack.256` shows the top of a sword. Each of the
six has a `.16a` cursor of the same name beside it, which is the one the
game shows.

The importer writes a sprite as three files named after it, here for
`cursors/wait/sprites.16a`:

| File | What it is |
|---|---|
| `sprites.toml` | how many frames, the size of a cell, how many in a row |
| `sprites_sheet.png` | the frames in a grid, row by row, as indices with the sprite's palette |
| `sprites_alpha.png` | for a `.16a`: how solid each pixel is |

The folders are those of the pictures, with `cursors/` and `backpack/`
going to `interface/cursors/` and `interface/backpack/`. The sheet has
`_sheet` in its name because the original has pictures and sprites of
one name, such as `shopframe.bmp` and `shopframe.256`. A row holds as
many frames as fit in 4,096 pixels. Where the frames are not all of one
size, each lies in the top left corner of its cell and the definition
lists their sizes. The two broken cursors are left out.
