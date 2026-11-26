# `.res` archives

The original game keeps its files in ten archives: `graphics.res`,
`main.res`, `sfx.res` and so on. An archive is a tree of folders and files
with no compression.

Everything below was measured on all ten archives of the GOG release, and
held for every one. All numbers are unsigned 32-bit, low byte first.

Read by `tools/import/res.c`.

## Layout

```
offset 0      header, 24 bytes
offset 24     file data, one file after another
...
table_offset  entry table, entry_count * 32 bytes, runs exactly to the end
              of the archive
```

## Header, 24 bytes

| Offset | Field | Notes |
|---|---|---|
| 0 | signature | `0x31415926` (bytes `26 59 41 31`) |
| 4 | root_first | index of the first entry of the root folder |
| 8 | root_count | number of entries in the root folder |
| 12 | flags | 1 or 17 seen; meaning unknown |
| 16 | table_offset | byte offset of the entry table |
| 20 | entry_count | number of entries in the table |

`table_offset + entry_count * 32 == archive size` in all ten archives.
graphics.res: root_first 4663, root_count 16, table_offset 106815610,
entry_count 4679, size 106965338.

## Entry, 32 bytes

| Offset | Field | Notes |
|---|---|---|
| 0 | unknown | 0 in every entry of every archive |
| 4 | offset | file: byte offset in the archive. folder: index of its first child entry |
| 8 | size | file: size in bytes. folder: number of children |
| 12 | type | 0 = file, 1 = folder; no other value seen |
| 16 | name | 16 bytes, zero-terminated |

Names: longest seen is 15 characters, so a zero byte was always present.
Stop at the first zero: in main.res, patch.res, scenario.res and video.res
the bytes after the zero are leftover junk, not zeros. No name contains a
space.

A folder's children are the entries `offset` to `offset + size - 1`.
For every folder `offset + size <= entry_count`. The root folder is
described by the header and has no entry of its own. It is not always the
last entries of the table (movies.res, sfx.res, world.res).

Entry counts: graphics 4679 (4049 files, 630 folders), main 112,
movies 162, music 21, patch 1, scenario 47, sfx 546, speech 1147, video 23,
world 6.

## Ranges and the tree

- Every file's bytes lie between the end of the header and the start of
  the table: `24 <= offset` and `offset + size <= table_offset`.
- Empty files exist (8 in graphics.res), with ordinary offsets. Empty
  folders exist (3 in graphics.res).
- Walking from the root, every entry is reached exactly once. No folder
  contains itself, no entry is shared, none is unreachable.
- Deepest nesting: 5 folders (graphics.res). The others are 1 to 4.

## Names

Measured on all 5,978 file paths of the ten archives.

- Every name is lower case. The characters used are `a-z`, `0-9`, `_`,
  `.`, `-`, `(` and `)`. No spaces, no upper case, no `/` or `\`.
- No two entries in one folder share a name: every full path is unique.

## Not known

- The meaning of `flags`, and of the first field of an entry.

In a damaged archive a folder could contain itself. A reader that walks
the tree needs a limit on depth.
