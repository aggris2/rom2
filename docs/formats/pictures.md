# Plain pictures

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
