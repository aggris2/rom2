# `.alm` map files

A map is one file: its ground, what stands on it, who plays on it, and
what happens when. The 46 maps of the campaign's missions are in
`scenario.res`, named by number. The maps for several players lie loose
in the game's folder, named in words: the GOG release has 12.

Measured on those 58 maps. Each was read to its exact end. All numbers
are 32-bit, low byte first, unless said.

ROM2 does not change maps. The importer copies them, and the game is to
read them as they are. A mission's map goes to
`campaign/missions/010/map.alm`, next to `mission.txt`, the text with
the same number from `main.res`, `text/mission10.txt`. A loose map goes
to `maps/` with its name in small letters.

## Layout

```
header, 20 bytes
13 sections, each a header of 20 bytes and its data
```

## Header, 20 bytes

| Offset | Field | Notes |
|---|---|---|
| 0 | signature | `0x0052374D`, the bytes `M7R` and a zero |
| 4 | header size | 20 |
| 8 | unknown | 25,672 in every map of 80 by 80, 83,016 in every one of 144 by 144, 262,216 in every one of 256 by 256 |
| 12 | sections | 13 |
| 16 | version | 1300 in 41 missions; 1600 in 5 missions and in all 12 loose maps |

## Section header, 20 bytes

| Offset | Field | Notes |
|---|---|---|
| 0 | unknown | 7 or 5 |
| 4 | header size | 20 |
| 8 | data size | in bytes |
| 12 | id | which section this is, 0 to 12 |
| 16 | unknown | one of two numbers, the same through a map |

After the data of section 0 come 16 bytes more, in every map. The other
sections follow each other with nothing between.

The sections come in the same order in every map, by their ids: 0, 1,
2, 3, 5, 11, 4, 9, 8, 6, 7, 10, 12.

## The sections

| Id | Size | What it is |
|---|---|---|
| 0 | 644 | the map as a whole: its width and height in squares first |
| 1 | width × height × 2 | the ground, a 16-bit number for each square: see [terrain.md](terrain.md) |
| 2 | width × height | the height of each square, a byte from 0 to 127 |
| 3 | width × height | the object on each square, a byte: 0 for none |
| 4 to 12 | differ | players, units, structures, triggers and more |

The missions are 144 by 144 squares (32 of them), 80 by 80 (13) or 256
by 256 (1). The loose maps are 144 by 144 (6), 80 by 80 (3) or 256 by
256 (3). The sizes of sections 1, 2 and 3 are as the table says in all
58.

## Missions and loose maps

The two kinds are one format and differ in what they hold:

| Section | Missions | Loose maps |
|---|---|---|
| 7 | large: 5,156 to 94,256 bytes | small: 808 to 3,564 bytes |
| 8 | 20 to 140 bytes in 25, empty in 21 | empty in all |
| 10 | empty in all | 224 to 3,808 bytes in all |

What sections 7, 8 and 10 are is not known yet. What follows about
objects was measured on the 46 missions.

A byte of section 3 that is not 0 is taken to be an object's `ID` from
`objects.reg`, plus 1. Drawing a map that way puts trees and rocks
where a wood and a road's edge are. 735,895 of the 812,288 squares
have no object.

Section 10 is empty in every map.

## Not known

- Sections 4 to 12, beyond their sizes.
- The rest of section 0 after the width and the height.
- The unknown fields of the two headers and the 16 bytes after
  section 0.
- Byte values in section 3 above 192, which is more than there are
  objects.
