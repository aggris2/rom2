# Objects: `objects.reg` and the frames of an object

An object is a piece of scenery: a tree, a bush, a rock, a signpost. The
registry `objects/objects.reg` in `graphics.res` lists them, and `.256`
sprite files hold their frames. The registry format is in
[reg.md](reg.md) and the sprite format in [256.md](256.md).

Measured on all 192 objects of the GOG release.

Read by `tools/import/objects.c`, with `tools/import/things.c` for what
objects share with units.

## The sections of objects.reg

194 sections: `Global`, `Files`, and `Object0` to `Object191`.

- `Global`: `ObjectCount` 192, `FileCount` 142.
- `Files`: `File0` to `File141`, texts like `pine1\sprites`.
- `ObjectN`: one object. Its number is its `ID`, which here runs from 0 to
  191 with none missing.

Keys of an object, and how many of the 192 state each:

```
ID 192  File 192  DescText 185  InMapEditor 185  Index 164  Phases 153
Width 153  Height 153  CenterX 153  CenterY 153  DeadObject 87
Parent 39  IconID 33  AnimationTime 31  AnimationFrame 31  FireObject 14
```

`Parent` works as for units: a value the object does not state is taken
from its parent. One object's parent has a parent itself. With parents
followed, every object has `Index`, `Phases`, `Width`, `Height`,
`CenterX` and `CenterY`.

## Sprite file and frames

`File` is an index into `Files`, and the text there is the path of the
sprite inside `objects/`, without its ending: `pine1\sprites` is
`objects/pine1/sprites.256`.

Objects share sprite files. 118 files belong to one object each, and 17
are shared by 2 to 9. `Index` is the first frame in the file that is the
object's, and `Phases` is how many it has. The rocks are frames 3, 4 and
5 of `objects/stones/sprites.256`. No frame of any file is left unused.

`Width`, `Height`, `CenterX` and `CenterY` are the box and the centre, as
for units, and a frame is the middle of the box. Most object frames are
exactly the size of the box: 350 of 405. Because a file is shared, its
other frames may be larger than this object's box.

`AnimationFrame` and `AnimationTime` are two lists of the same length:
which of the object's frames to show, counted from 0, and for how long.
45 objects have them, with parents followed.

Every sprite file has its `b` file with the soft edges.

## Dead and burning objects

`DeadObject` is the `ID` of the object this one turns into when it is
destroyed, or -1. Every one named exists.

`FireObject` is -2 on the 21 dead trees and bushes, and -1 on 7 objects
that have no name and are not in the map editor. Those 7 come right
after a tree in the registry (1 after Pine 1, 4 after Vallenwood 1) and
have 17 frames and a long animation, so they are the burning trees. What
ties a tree to its burning object is not stated in the registry.

## What does not fit

- **The 7 burning trees have no sprites.** They name files like
  `objects/pine1/fire/fpine.256`, which are not in the archive. ROM2
  leaves them out.
- **18 dead trees state 7 frames and have 1.** Their `Phases` is 7, as
  the living tree's, and their file has one frame. They have no
  animation lists. ROM2 gives them the one frame.

## What ROM2 writes for an object

`import_objects` writes a folder for every object, named after its ID
and its name: `000_pine_1`. In it are `object.toml`, described at the
top of `tools/import/object_toml.h`, and two pictures with the same
cells: `sprites.png`, the object's frames in one row, and `edges.png`,
their soft edges. An object gets only its own frames, also when its
file is shared, and the values of its parent written out.

185 of the 192 objects are written.

## Not known

- How the game finds the burning object of a tree.
- The meaning of `IconID`.
- Where the box is drawn relative to the object's place on the map, as
  for units.
