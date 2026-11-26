# `data.bin`: the tables of the game's rules

`data/data.bin` in `world.res` holds the numbers the rules run on: the
kinds of monsters and humans with their stats, the items, the spells and
the buildings. It is a spreadsheet saved in binary, one table after the
other.

Measured on the `data.bin` of the GOG release, 149,971 bytes. Read as
described below, the tables end exactly at the last byte of the file.

ROM2 does not read this file yet.

## Building blocks

- **Number.** 32 bits, signed, low byte first.
- **Short number.** 16 bits, low byte first.
- **Fraction.** 64 bits, an IEEE double.
- **Text.** One byte with the length, then that many characters. No
  closing zero. A text of no characters is the one byte 0.
- **Header.** A short number, then that many texts: the names of a
  table's columns, as the spreadsheet had them. The first is the column
  of the row's name. A reader can skip a header, but it says what the
  numbers of a row are.

## The tables, in file order

| Table | Header | Rows | A row is |
|---|---|---|---|
| Shapes | 11 names, shared with the next | 7 | text, 9 fractions |
| Materials | (the same header) | 16 | text, 9 fractions |
| Modifiers | 30 names | 50 | text, short number 28, 28 numbers |
| Armour | 18 names, shared by three | 30 | text, short number 17, 17 numbers, 8 short numbers |
| Shields | (the same header) | 9 | as armour |
| Weapons | (the same header) | 27 | as armour |
| Magic items | 4 names | 96 | text, short number 2, 2 numbers, a byte, and a text if the byte is not 0 |
| Monsters | 64 names | 241 | see below |
| Humans | 28 names | 289 | see below |
| Buildings | 9 names | 179 | text, short number 6, 6 numbers |
| Spells | 24 names | 33 | see below |

Each table starts with a number, the count of its rows. For the first
three tables it is the count itself. For armour, shields, weapons, magic
items, monsters, humans and buildings it is one more than the number of
rows that follow, and for spells two more.

A row starts with its name, and in most tables a short number that says
how many numbers follow.

## Monsters and humans

A monster row is its name, the short number 62, 62 numbers, and 2 texts
with the items it carries. The 62 numbers are the header's columns 2 to
63, in order: `body`, `reaction`, `mind`, `spirit`, `healthMax` and so on.

A human row is its name, the short number 26, 26 numbers, and 10 texts
with the items the human wears.

An empty row has the short number 0 and no numbers, but still its texts,
all of no characters: 2 bytes for a monster, 10 for a human. 61 monster
rows and 5 human rows are empty, and so is their name.

A row is a **kind** of unit, not a unit. `Goblin_Pike` and
`Goblin_Pike.2` are two kinds, a weak and a stronger goblin. Two columns
tie a kind to what it looks like:

- `typeID` is the `ID` of a unit in `units.reg`, which gives the sprite.
- `face` picks the colouring: 1 for `palette.pal`, 2 for `palette2.pal`
  and so on. For every `typeID` the largest `face` among its monster
  kinds is no more than the unit's `Palette` in `units.reg`. For a human
  kind `face` is something else, a number up to 51.

`serverID` is a number of the kind's own, different for each of the 180
monster kinds and each of the 284 human kinds that are not empty.

Example, the monster `Goblin_Pike`: body 20, healthMax 40, speed 24,
physicalMin 4, physicalMax 8, typeID 64, face 1, XPvalue 60, and the
item `Bronze Pike`.

## Spells

A spell row is its name, the short number 22, 22 numbers, and a text. An
empty row is the name, the short number 0, and 2 more bytes of 0. 4 of
the 33 rows are empty.

## Not known

- What the 9 fractions of a shape or a material are, one by one. Their
  header has 10 columns after the name.
- Why the count of most tables is one or two more than their rows.
- What `face` means for a human kind.
- The two monster columns the header calls `not used`. They are -1 in
  some rows and hold other numbers in most.
- The meaning of most columns beyond their names.
