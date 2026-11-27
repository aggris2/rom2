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
| 8 | a size | 25,672 in every map of 80 by 80, 83,016 in every one of 144 by 144, 262,216 in every one of 256 by 256; of what is not known |
| 12 | sections | 13 |
| 16 | version | 1300 in 41 missions; 1600 in 5 missions and in all 12 loose maps |

## Section header, 20 bytes

| Offset | Field | Notes |
|---|---|---|
| 0 | type | 7 or 5 |
| 4 | header size | 20 |
| 8 | data size | in bytes |
| 12 | id | which section this is, 0 to 12 |
| 16 | version | one of two numbers, the same through a map |

The data size of section 0 is 644 in every map, and its data is 660
bytes: the 16 bytes over are the end of its last field. The other
sections follow each other with nothing between.

The sections come in the same order in every map, by their ids: 0, 1,
2, 3, 5, 11, 4, 9, 8, 6, 7, 10, 12.

## The sections

| Id | Size | What it is |
|---|---|---|
| 0 | 660 | the map as a whole |
| 1 | width × height × 2 | the ground, a 16-bit number for each square, which also says what kind of ground it is: see [terrain.md](terrain.md) |
| 2 | width × height | the height at each square, a byte from 0 to 127: see below |
| 3 | width × height | the object on each square, a byte: 0 for none |
| 4 | 20 for each structure, 28 for a bridge | the structures |
| 5 | 76 for each player | the players |
| 6 | 48 for each unit | the units |
| 7 | differs | the script of the map |
| 8 | differs | the sacks |
| 9 | differs, 4 when empty | the effects: traps and magic that is in place |
| 10 | 16 for each | the groups of units |
| 11 | 12, 84 and 12 for each of three kinds | what the inns, the shops and the signposts hold |
| 12 | 28 for each, and 28 more | the places where a tune plays |

The missions are 144 by 144 squares (32 of them), 80 by 80 (13) or 256
by 256 (1). The loose maps are 144 by 144 (6), 80 by 80 (3) or 256 by
256 (3). Every size in the table that is given as a rule holds in all
58 maps.

## Section 0, the map as a whole

| Offset | Field | Notes |
|---|---|---|
| 0 | width, height | in squares |
| 8 | angle of the sun | a decimal number of 32 bits: 0.785 in 33 maps, 0.768 in 21 |
| 12 | time of day | 360 in 55 maps |
| 16 | darkness | 0 to 23; 21 in 33 maps |
| 20 | contrast | 48 to 64; 63 in 46 maps |
| 24 | which ground is used | bits; 8191 in 27 maps |
| 28 | number of players | |
| 32 | number of structures | |
| 36 | number of units | |
| 40 | seven more counts | see below |
| 68 | name | 64 bytes of text, empty in 27 maps |
| 132 | players the map is meant for | 1 in every mission; 4, 8 or 16 in the loose maps |
| 136 | level | 1 to 4 |
| 140 | two numbers | not known |
| 148 | author | 512 bytes of text |

The seven counts at 40, in order:

| Count | What it counts |
|---|---|
| 1 | in 49 maps the script's actions, conditions and triggers together; in 9 not |
| 2 | the sacks of section 8 |
| 3 | the groups of section 10 |
| 4 | the inns of section 11 |
| 5 | the shops of section 11 |
| 6 | the signposts of section 11 |
| 7 | the places of section 12, after its first entry |

## Section 4, the structures

A structure is 20 bytes:

| Offset | Field | Notes |
|---|---|---|
| 0 | x, y | 32 bits each: the square times 256, plus 128 |
| 8 | type | the structure's `ID` in `structures.reg`, in the lower 16 bits |
| 12 | health | 16 bits; 100 in 3,015 of the 3,303 |
| 14 | player | 32 bits, counted from 1 |
| 18 | number | 16 bits; its number in the map |

A bridge of variable size, type 33, has 8 bytes more: its width and its
height in squares. In maps of version 1600 its type also has the bit
`0x01000000` set; in maps of version 1300 it does not, and the type
alone says the 8 bytes are there. The maps have 43 such bridges.

Every one of the 3,303 structures has a type that `structures.reg`
has, a player the map has, and a place inside the map.

The square is the top left one of the structure's base. Drawn so, with
the bottom of the picture on the bottom of the base, the bridge of
mission 10 lies across its river and the two sphinxes on either side of
the road to it.

## Section 5, the players

A player is 76 bytes: a colour, flags and money, 32 bits each, a name
of 32 bytes, and 16 numbers of 16 bits, one for each other player. The
16 numbers are taken to be how the player stands to each other one, by
bits: 1 is at war, 2 is allied, and 16 is seeing what the other sees. A
player's number for itself is 18, allied and seeing, in 354 of the 359.
This reading fits the names: in mission 10 the player `self` has 1 for
`Enemy` and 0 for `Neutral`. It was not checked against the game. A
mission's first player is named `self`.

## Section 6, the units

A unit is 48 bytes:

| Offset | Field | Notes |
|---|---|---|
| 0 | x, y | 32 bits each: the square times 256, plus 128 |
| 8 | type | 16 bits: the unit's `ID` in `units.reg` |
| 10 | face | 16 bits, 0 to 7 |
| 12 | flags | 32 bits; 0 for 6,594, 16 for 1,056 |
| 16 | quest flags | 32 bits; 0 for 7,138 |
| 20 | kind | 32 bits: the `serverID` of a kind in `data.bin` |
| 24 | player | 32 bits, counted from 1 |
| 28 | sack | 32 bits: the number of its sack in section 8, counted from 1; 0 for none |
| 32 | angle | 32 bits, 0 to 15: the standing step it faces |
| 36 | health, and the most it can have | 16 bits each; both -1 for 6,250, which is taken to mean "as its kind has" |
| 40 | number | 16 bits: the unit's number in the map, each used once |
| 42 | not used | 16 bits of anything |
| 44 | group | 32 bits: the `id` of a group in section 10 |

Every one of the 7,665 units has a type that `units.reg` has, a kind
that `data.bin` has, a player the map has, and a place inside the map.
For 95 of them the kind belongs to another unit than the type names: 86
have type 4 with a kind of unit 5, 9, 10, 12 or 15, and 9 a kind of
unit 1 with type 5, 10 or 14. Which of the two the game goes by is not
measured.

## Section 7, the script

Three lists, each with its count before it:

```
count, then that many actions, 796 bytes each
count, then that many conditions, 796 bytes each
count, then that many triggers, 184 bytes each
```

An action and a condition have one layout:

| Offset | Field | Notes |
|---|---|---|
| 0 | name | 64 bytes of text, as the map's maker called it |
| 64 | type | which kind of action or condition |
| 68 | index | the number triggers know it by |
| 72 | once | not 0 if it is to run once |
| 76 | 10 values | 32 bits each |
| 116 | 10 types of value | 32 bits each |
| 156 | 10 names of value | 64 bytes of text each, `<None>` for one not used |

The kinds are listed in two text files that come with the game's map
editor, with the meaning of each value: `Description Instants.ini` for
the actions and `Description Checks.ini` for the conditions. Every one
of the 1,353 actions and 986 conditions in the 58 maps has a type that
its file lists. Type 65538 is in both: as an action it is
`Drop Location`, where a player enters the map, and its first two
values are the square.

A trigger is:

| Offset | Field | Notes |
|---|---|---|
| 0 | description | 128 bytes of text |
| 128 | 6 conditions | the index of each, or 0 |
| 152 | 4 actions | the index of each, or 0 |
| 168 | 3 operators | how the conditions are compared, two by two; 0 to 5 |
| 180 | once | not 0 if it is to fire once |

Of the 4,314 conditions the triggers name, one is not there; all the
2,876 actions are. What the operators' numbers mean was not measured.

Read so, mission 10 starts with a trigger `Start` that sends message 1,
and has one `Come to ogre first time`.

## Section 8, the sacks

For each sack: the number of things in it, the number of the unit that
has it, x and y as a unit has them, its gold, and then 10 bytes for
each thing: the item's number of 16 bits as in
[items.md](items.md), in 32 bits; flags of 16 bits; and the number of
an effect in section 9, or 0.

The 58 maps have 51 sacks. 32 belong to a unit: that unit's `sack` is
the sack's place in this list, its number is the sack's unit, and the
two are on one square. The other 19 have unit 0 and lie on the ground.
In one loose map, `hc_an_autumn_1_8`, 25 units name sacks and the map
has none.

## Section 9, the effects

A count, then for each effect 26 bytes and its changes:

| Offset | Field | Notes |
|---|---|---|
| 0 | item | 32 bits |
| 4 | x, y | 32 bits each, in squares |
| 12 | mode | 16 bits: 0, 1, 4 or 5 |
| 14 | least, and spread | 16 bits each |
| 18 | spell | 16 bits: a spell's number |
| 20 | strength | 16 bits |
| 22 | number of changes | 32 bits |
| 26 | the changes | 6 bytes each: a number of 16 bits and one of 32 |

The maps have 217 effects. What the modes and the changes mean was not
measured.

## Section 10, the groups

A group is 16 bytes: its `id`, the time after which its units come back,
flags, and the index of an action or -1. Every one of the 683 groups
is named by at least one unit. The time is 120 for 486 of them. Only
the loose maps have groups.

## Section 11, the inns, shops and signposts

First the inns, 12 bytes each: the number of the structure, flags, and
an item or -1. Then the shops, 84 bytes each: the number of the
structure, and then five lists of four numbers, one for each shelf of
a shop: flags, the least price, the highest price, how many things, and
how many of one kind. Then the signposts, 12 bytes each: the number of
the structure, whether its action is on, and the index of the action.

All 368 inns and all 69 signposts name a structure the map has, and
426 of the 431 shops.

## Section 12, the tunes

Entries of 28 bytes: x, y and a radius in squares, and four numbers of
tunes, -1 for none. The first entry is the tune of the whole map, with
0 for its place. The others, as many as the count in section 0 says,
are places where the tune changes. All 486 places are inside their
maps.

## Missions and loose maps

The two kinds are one format and differ in what they hold:

| Section | Missions | Loose maps |
|---|---|---|
| 7 | large: 5,156 to 94,256 bytes | small: 808 to 3,564 bytes |
| 8 | 20 to 140 bytes in 25, empty in 21 | empty in all |
| 10 | empty in all | 224 to 3,808 bytes in all |

## Section 2, the heights

A byte for each square, 0 to 127 in all 1,152,512 squares of the 58
maps. It is the height of the ground at the top left corner of the
square, in pixels: a corner is drawn that many pixels higher up the
screen than it would be on flat ground. A square's tile is stretched
between its four corners, which are its own and those of the squares
to its right, below it, and below to its right. What stands on the
ground is raised by the height at its place, taken between the four
corners around it.

The slope of the ground also lights it: ground that faces the sun is
drawn brighter and ground that faces away darker, by the sun's angle
in section 0. Flat ground is drawn brighter than its picture, about
1.56 times.

This reading and the rule for the light are the original program's,
from earlier work on it. Heights are not measured against the game
here. Drawn so, mission 10 has its mountains standing up with a lit
and a dark side, its river lying low, and its trees, bridge and tower
sitting on the ground.

## Section 3, the objects

A byte of section 3 that is not 0 is an object's `ID` from
`objects.reg`, plus 1. Drawing a map that way puts trees and rocks
where a wood and a road's edge are.

The 58 maps use 191 different objects. All but one are in the
registry. The one that is not, 208, is on 11 squares of mission 52,
in its second and fourth rows. Five of the objects the maps use are
burning trees, which the registry names and the archive has no
pictures for: they are
on 100 squares, every one of them in the 8 squares along the edge of
its map, where nothing is seen and nothing can go. So is the 208.

## Not known

- What each kind of action and condition does: their names and values
  are in the editor's two files, their working is in the game.
- What the operators of a trigger mean, the modes and changes of an
  effect, and the flags of a unit, a player, a group and an inn.
- The size in the file's header, and the type and version in a
  section's.
- The exact rule by which the game takes a height between four
  corners, and the numbers of its light at each time of day.
