# BUN Parser

A binary file parser and validator for the **BUN (Binary UNified assets)** format — a container format for storing multiple named assets (textures, audio, scripts, etc.) in a single binary file.

---

## Overview

This parser reads `.bun` files, validates them against the BUN 1.0 specification, and reports any structural or semantic errors found. It supports:

- Full header validation (magic, version, offsets, sizes)
- Asset record parsing and validation
- String table name validation (printable ASCII)
- RLE decompression and validation
- Section bounds and overlap checking
- Human-readable asset and data output

---

### Output

```
Header:
  magic: 0x304e5542
  version: 1.0
  asset count: 2
  asset table offset: 64
  string table offset: 160
  string table size: 16
  data section offset: 176
  data section size: 48

Assets:
Asset 0:
  Name: texture_01
  name_offset: 0
  name_length: 10
  data_offset: 0
  data_size: 24
  compression: 0
  type: 1
  checksum: 0
  flags: 0
  Data: ...
```

### Exit codes

| Code | Meaning |
|------|---------|
| 0 | `BUN_OK` — file is valid |
| 1 | `BUN_MALFORMED` — file is structurally invalid |
| 2 | `BUN_UNSUPPORTED` — file uses an unsupported feature (e.g. zlib, checksums) |
| 3 | `BUN_ERR_IO` — file could not be read |

---

## Running Tests

```bash
make test
```

---

## File Format Summary

A `.bun` file consists of four sections:

```
+----------------------+
| Header               |  60 bytes, always first
+----------------------+
| Asset Entry Table    |  48 bytes × asset_count
+----------------------+
| String Table         |  variable length
+----------------------+
| Data Section         |  variable length
+----------------------+
```

Sections after the header may appear in any order, must not overlap, and must fit entirely within the file. All offsets and sizes must be divisible by 4.

### Header fields

| Field | Size | Description |
|-------|------|-------------|
| `magic` | u32 | Must be `0x304E5542` ("BUN0") |
| `version_major` | u16 | Must be `1` |
| `version_minor` | u16 | Must be `0` |
| `asset_count` | u32 | Number of asset records |
| `asset_table_offset` | u64 | Offset to asset entry table |
| `string_table_offset` | u64 | Offset to string table |
| `string_table_size` | u64 | Size of string table in bytes |
| `data_section_offset` | u64 | Offset to data section |
| `data_section_size` | u64 | Size of data section in bytes |
| `reserved` | u64 | Ignored |

### Asset record fields

| Field | Size | Description |
|-------|------|-------------|
| `name_offset` | u32 | Offset into string table |
| `name_length` | u32 | Length of name (no null terminator) |
| `data_offset` | u64 | Offset into data section |
| `data_size` | u64 | On-disk size of asset data |
| `uncompressed_size` | u64 | Expected uncompressed size (0 if unused) |
| `compression` | u32 | 0=none, 1=RLE, 2=zlib (unsupported) |
| `type` | u32 | User-defined asset type |
| `checksum` | u32 | CRC-32 checksum (0=unused, non-zero unsupported) |
| `flags` | u32 | Bit flags: `0x1`=encrypted, `0x2`=executable |

---

## Validation Rules

The parser checks all of the following, reporting errors to stderr:

- Magic number matches `BUN_MAGIC`
- Version is exactly 1.0
- All offsets and sizes are divisible by 4
- No section extends past the end of the file
- No two sections overlap (including the header)
- Each asset name is non-empty and contains only printable ASCII (0x20–0x7E)
- Asset names lie within the string table
- Asset data lies within the data section
- Compression values are valid (0 or 1; 2 returns unsupported)
- `uncompressed_size` is 0 when compression is 0
- RLE data size is even
- RLE count fields are non-zero
- RLE expanded size matches declared `uncompressed_size`
- Checksums are unsupported (non-zero returns unsupported)
- Unknown flag bits return unsupported

---

## Project Structure

```
.
├── bun.h               # Type definitions, structs, constants
├── bun_parse.c         # Parser implementation
├── main.c              # Entry point and output formatting
├── Makefile
├── setup.sh            # Use to install dependencies for using tests
└── tests/
    ├── test_bun.c      # Test suite (uses libcheck)
    └── fixtures/
        ├── valid/      # Valid BUN files for positive tests
        └── invalid/    # Malformed BUN files for negative tests
```