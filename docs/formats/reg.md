# `.reg` registry files

A registry is a small tree of named values, like an `.ini` file in binary:
sections, and in each section names with a value. The game describes its
units, objects, structures and projectiles in registries.

Measured on all 17 `.reg` files of the GOG release (5 in graphics.res, 1
each in scenario.res and sfx.res, 8 in video.res, 2 in world.res). Every
statement below held for every file. All numbers are 32-bit, low byte
first.

Read by `tools/import/reg.c`.

## Layout

```
offset 0                  header, 24 bytes
offset 24                 entry table, entry_count * 32 bytes
offset 24 + 32 * count    values_size, 4 bytes
after that                the values area, values_size bytes, to the end
```

This is the same family as the `.res` archive (same signature, same
32-byte entries), with two differences: the table comes directly after
the header, and the fifth header field is the number of entries, not an
offset.

## Header, 24 bytes

| Offset | Field | Notes |
|---|---|---|
| 0 | signature | `0x31415926` |
| 4 | root_first | index of the first root entry; 0 in every file |
| 8 | root_count | number of root entries |
| 12 | flags | 17 in every file |
| 16 | entry_count | number of entries in the table |
| 20 | unknown | 0 in every file |

`24 + 32 * entry_count + 4 + values_size == file size` in all 17 files.

## Entry, 32 bytes

| Offset | Field |
|---|---|
| 0 | unknown, 0 in every entry |
| 4 | `a`: meaning depends on the type |
| 8 | `b`: meaning depends on the type |
| 12 | type |
| 16 | name, 16 bytes, zero-terminated (longest seen: 15) |

| Type | Kind | `a` | `b` |
|---|---|---|---|
| 0 | text | offset in the values area | length in bytes, closing zero included |
| 1 | section | index of its first entry | number of entries |
| 2 | integer | the value itself, signed | 0 |
| 4 | float | the value itself, IEEE 32-bit | not always 0 (see below) |
| 6 | integer list | offset in the values area | length in bytes, always a multiple of 4 |

No other type occurs. Types 3 and 5 are never used.

Measured:

- Every text ends with exactly one zero byte and has none inside.
  The ones in units.reg are plain ASCII. Paths in them use `\`.
- The values area is used completely and nothing in it is used twice:
  the texts and lists tile it with no gaps and no overlaps.
- The tree is always exactly two levels: root sections, and values inside
  them. No section contains a section.
- Every entry is reached exactly once from the root.
- Floats occur only in the video registries. In 9 of their float entries
  `b` is not 0; its meaning is unknown.

## Not known

- The meaning of `flags`, of the first field of an entry, and of `b` in a
  float entry.

The registry of units is described in [units.md](units.md).
