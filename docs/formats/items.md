# Items: shapes, materials, classes and their pictures

An item is something a character carries or wears: a sword, a helm, a
scroll. Their numbers are in six tables of `data.bin`, described in
[databin.md](databin.md), and their pictures for the inventory are
[.16a](16a.md) files in `graphics.res`.

Measured on the GOG release.

Read by `tools/import/item.c`.

## An item is a shape in a material and a class

The original has no row for an iron long sword. It has:

- **shapes**: the rows of the tables of weapons (27), shields (9) and
  armour (30). A dagger, a long sword, a plate cuirass.
- **materials**: 16 rows, from iron and bronze to dragon leather and
  crystal.
- **classes**: 7 rows: common, uncommon, rare, very rare, elven, bad and
  good. The file calls this table the shapes, which it is not.

A row of the materials or the classes is 9 fractions. The first two are
0 in every row. The other seven are taken to be what a shape's price,
weight, damage, chance to hit, defence and absorption are multiplied
by, and one more number to do with magic. That reading is from the
table's column names and was not checked in the game.

## Which variants exist

Not every material goes with every class. A shape's row ends in 8 short
numbers, one for each class from 0 to 7, and in each of them bit `m` is
set if the shape exists in that class in material `m`. The dagger's are
3, 6, 104, 208, 224, 0, 0, 0: a common dagger is of iron or bronze, an
uncommon one of bronze or steel, a rare one of silver, mithril or
adamantium.

One pair of a material and a class is a variant of the shape. The 66
shapes have 395 variants, 14 at most.

## A shape's row

After its name a row has 17 numbers and the 8 short ones. The columns
are, by the file's own names: `Shape`, `Material`, `Price`, `weight`,
`Slot`, `@.attackType`, `@.physicalMin`, `@.physicalMax`, `@.toHit`,
`#.deIrnce`, `#.absorbtion`, `@.range`, `@.charge`, `@.relax`,
`2 handed`, `sutableFor` and `Other Parameter`.

`Slot` is where the item is worn:

| Slot | | Slot | |
|---|---|---|---|
| 1 | weapon | 8 | cuirass, cloak |
| 2 | shield | 9 | bracers |
| 4 | ring | 10 | gauntlets, gloves |
| 5 | amulet | 12 | boots, shoes |
| 6 | helm, hat | 14 | magic item |
| 7 | mail, robe | | |

One row of the weapons, called `rem`, is -1 in every number: a gap. Five
weapons have no variants: bare hands, and four that only monsters and
structures use, such as the dragon's flame thrower.

## The pictures for the inventory

`inventory/` has 491 `.16a` files, each one frame of 80 by 80 pixels.
A file is named after its item in seven digits:

```
MM SS C PP     material, slot, class, place
00 01 0 02     an iron common dagger: 0001002.16a
```

`place` is the shape's place in its table, counted from 1. Armour is
one table for all its slots: the ring is 1, the amulet 2, the plate
boots 30.

The masks and the files agree completely: each of the 395 variants has
its file, and no file is of a variant the masks do not allow.

The other 96 files are the magic items, as material 0 and class 0 in
slot 14: `0014001.16a` to `0014096.16a`.

The pictures of one shape are not one picture in other colours. An iron
dagger and a gold one differ in their pixels and in their palettes.

## The pictures of items worn

In the inventory the character is drawn with what it wears. The
pictures for that are `.256` files in `equipment/`, 1,104 of them:

```
equipment/mfighter/1.256 ... 54.256         the characters themselves
equipment/mfighter/primary/0001002.256      an item, as worn
equipment/mfighter/secondary/0009020.256    its second part
```

There are four kinds of character: `mfighter`, `ffighter`, `mmage` and
`fmage`, a man or a woman who fights or does magic, with 54, 22, 20 and
20 characters to choose from.

Every file is one frame of 160 by 240 pixels, the size of the whole
character, with its own palette. An item's picture has the item at the
place it is worn and nothing else, so the pictures are laid over the
character one on the other. None has a `b` file.

An item's files are named as its picture for the inventory is. Measured
on the 395 variants of the 60 shapes that have any:

- Every variant has a worn picture for at least one kind of character.
- Which kinds is the same for all variants of a shape. 47 shapes are
  drawn on the two fighters, 11 on the two mages, and 2 on all four.
- 10 shapes also have a file in `secondary` for each of their kinds:
  the ring, the bracers, the gauntlets and gloves, and the cloaks. It is
  the item's second part. Drawn on a character, the first picture of
  bracers, of gloves or of a ring is on one hand and the second on the
  other, and a cloak's second picture is its shoulders.
- The picture for a man and for a woman is the same file byte for byte
  for 66 of the 335 fighter items.

This was checked by laying the pictures of a helm, a cuirass, bracers,
boots, a sword and a shield over a fighter: they sit where they belong.

## What does not fit

- **Two amulets are drawn as helms.** The pictures of the common amulet
  in iron and in bronze, `0005002.16a` and `0105002.16a`, are the same
  files byte for byte as those of the common helm in iron and in bronze,
  `0006006.16a` and `0106006.16a`. The other 12 amulets are amulets.
  ROM2 writes them as they are.
  Their worn pictures for the fighters, four files, are empty: 0 bytes.
  So these two are not real items, and nothing wears them.
- **Magic items share pictures.** Every scroll of one kind of magic has
  the same picture, and so have the potions of one kind: 10 groups of 4
  to 10 files that are the same byte for byte.

## Magic items

The table of magic items has 96 rows: books, scrolls, potions, and
things for quests. A row is a price and a weight, and 84 of them have a
text of what the item does: `castSpell=Fire_Arrow:50`. A magic item's
number is the place of its row, counted from 1.

## Modifiers

The 50 rows of the modifiers are what magic can change about whoever
carries an item: `body`, `healthMax`, `toHit`, `damageEarth`. A row is
28 numbers: `cost.MP`, `affectMin`, `affectMax`, `UsableBy`, and then
24 named after places on the body, 12 for a fighter and 12 for a mage:
how much of the change an item in that place can hold.

## How a unit names its items

A kind of unit in `data.bin` names what it carries as text: the class,
if it is not common, the material, the shape, and in braces what magic
it has. `Elven Magic Wood Long Bow{toHit=50}`. Reading these texts is
not done yet.

## What ROM2 writes

The importer writes, in `content/items`:

- A folder for every shape, in `weapons`, `shields` or `armour`, named
  after its place and its name: `weapons/002_dagger`. In it is
  `item.toml`, with the shape's numbers and the list of its variants,
  and a folder for each variant, named after its material and its
  class: `iron_common`. A variant's folder has its picture for the
  inventory: `icon.png`, an 8-bit indexed PNG with its own palette, and
  `icon_alpha.png`, which says how solid each pixel is. It also has a
  folder `worn` with the variant's pictures as worn, one for each kind
  of character that wears it, named after the character:
  `worn/fighter_man.png`, and `worn/fighter_man_second.png` for the item's
  second part. These are 8-bit indexed PNGs with their own palette.
- A folder for every magic item in `magic`, with `item.toml`,
  `icon.png` and `icon_alpha.png`.
- `materials.toml`, `classes.toml` and `modifiers.toml`: the three
  tables, an entry for each row.

A variant has pictures of its own because it has colours of its own,
and a PNG holds one palette: put side by side in one picture, only the
first would look right in a viewer.

The characters themselves go to `content/characters`, a folder for each of
the four kinds, under ROM2's names for them: `fighter_man`,
`fighter_woman`, `mage_man` and `mage_woman`. So the first fighter is
`characters/fighter_man/001.png`.

The files are described at the top of `tools/import/item.h`.

65 shapes and 96 magic items are written, with all 491 pictures for the
inventory, 984 pictures of items worn, and 116 characters.

## The names shown

The name a player sees for an item is not in `data.bin`. Three files
give it:

| File | What it is |
|---|---|
| `data/itemname.bin` in `world.res` | 491 numbers of 16 bits, low byte first, one for each item |
| `text/itemserv.txt` in `main.res` | 491 lines: the name the server knows each by |
| `text/itemname.txt` in `main.res` | 491 lines: the name shown |

The three are in one order. A number is the item's material in its top
4 bits, then 4 bits of its slot, 3 of its class and 5 of its place in
its table: `0x1914` is material 1, slot 9, class 0, place 20, the common
bronze bracers, which the server knows as `Common Bronze Bracers` and
the player sees as `Bronze Bracers`.

395 of the numbers are the 395 variants of the shapes, each exactly
once. The other 96 have slot 14 and are the magic items: the server's
name of each is the name its row has in `data.bin`, such as `Book Air`,
shown as `Book`.

ROM2 writes the name shown as `display_name`, in each variant of a
shape's `item.toml` and at the top of a magic item's.

`text/itemname.pat` has the same names in another form, lines of
`Common Bronze Bracers=Bronze Bracers`: 521 of them, and 5 lines without
a `=`. All 491 pairs of the two lists above are in it, word for word. The
other 30 are names of things for quests and of five scrolls, which no
table has an item for. The importer writes the file as
`texts/itemname_pat.txt`.

## The numbers worked out in advance

`data/itemname.pkt` in `world.res` has, for each of the 491 items in
the order of `itemname.bin`, what the game shows of it:

```
3 bytes of 0, the count of items in 32 bits, the size of the rest in 16
for each item:
    its number, 16 bits        as in itemname.bin
    how many, 16 bits          1 in all
    a kind, 1 byte             2 for most of what is worn or held
    the count of values, 1 byte
    their size in bytes, 1 byte
    the values                 a tag of 1 byte and then the value
```

A value is one byte, but for tag 1, the price, which is 32 bits. The
tags that were matched: 1 the price, 12 the bonus to hit, 13 the least
damage, 14 how much more the most damage is, 15 the defence, 16 the
absorption. The file reads to its exact end so.

This is the game's own arithmetic, and it checks ours. For each of the
395 variants the numbers were worked out from the shape's row and the
multipliers of its material and class in `data.bin`:

| Value | Worked out as | Agrees |
|---|---|---|
| price | shape × class × material, rounded | 395 of 395 |
| defence | shape × class × material, rounded | 395 of 395 |
| absorption | shape × class × material, cut off | 395 of 395 |
| least damage | shape × class × material, rounded | 154 of 154 weapons |
| most damage, as what it adds | the same for the most, less the least, rounded | 154 of 154 |
| bonus to hit | shape × class × material, rounded | 154 of 154 |

"Rounded" is half added and the fraction dropped. The importer does not
read this file: it holds nothing that is not worked out from the
tables.

## Not known

- In what order the worn pictures are laid over each other.
- How a character's number here goes with the `face` of a kind of
  human in `data.bin`.

- What the 9 fractions of a material or a class are, beyond the reading
  above, and how exactly a variant's numbers are worked out from them.
- The columns `Shape`, `Material`, `sutableFor` and `Other Parameter` of
  a shape.
- The small languages of a magic item's text and of the braces in a
  unit's item.
