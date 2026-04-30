#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>
#include "bun.h"

/**
 * Example helper: convert 4 bytes in `buf`, positioned at `offset`,
 * into a little-endian u32.
 */

static u16 read_u16_le(const u8 *buf, size_t offset) {
  return (u16)buf[offset] | (u16)buf[offset + 1] << 8;
}

static u32 read_u32_le(const u8 *buf, size_t offset) {
  return (u32)buf[offset] | (u32)buf[offset + 1] << 8 |
         (u32)buf[offset + 2] << 16 | (u32)buf[offset + 3] << 24;
}

static u64 read_u64_le(const u8 *buf, size_t offset) {
  return (u64)buf[offset] | (u64)buf[offset + 1] << 8 |
         (u64)buf[offset + 2] << 16 | (u64)buf[offset + 3] << 24 |
         (u64)buf[offset + 4] << 32 | (u64)buf[offset + 5] << 40 |
         (u64)buf[offset + 6] << 48 | (u64)buf[offset + 7] << 56;
}
//
// API implementation
//

static int sections_overlap(u64 offset_a,u64 size_a,u64 offset_b,u64 size_b){
  if (!(offset_a + size_a <= offset_b ||
  offset_b + size_b <= offset_a)) {
    return 1;
  }
  return 0;
}
//
// Basic helper function to handle error logging to tempfile.
//
int bun_log_error(BunParseContext *ctx, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(ctx->errors, fmt, args);
    va_end(args);
    fprintf(ctx->errors, "\n");
    return 0;
}

bun_result_t bun_open(const char *path, BunParseContext *ctx) {
  // we open the file; seek to the end, to get the size; then jump back to the
  // beginning, ready to start parsing.
  ctx->errors = stderr;
  ctx->file = fopen(path, "rb");
  if (!ctx->file) {
    return BUN_ERR_IO;
  }

  if (fseek(ctx->file, 0, SEEK_END) != 0) {
    fclose(ctx->file);
    return BUN_ERR_IO;
  }
  ctx->file_size = ftell(ctx->file);
  if (ctx->file_size < 0) {
    fclose(ctx->file);
    return BUN_ERR_IO;
  }
  rewind(ctx->file);

  return BUN_OK;
}

bun_result_t bun_parse_header(BunParseContext *ctx, BunHeader *header) {
  u8 buf[BUN_HEADER_SIZE];
  bun_result_t exit_code = BUN_OK;

  // our file is far too short, and cannot be valid!
  // (query: how do we let `main` know that "file was too short"
  // was the exact problem? Where can we put details about the
  // exact validation problem that occurred?)

  // File entered is truncated, with stated size of the file being larger than the
  // actual file given.
  
  if (ctx->file_size < (long)BUN_HEADER_SIZE) {
    bun_log_error(ctx, "Malformed: file too small to contain header (file_size: %llu, header_size: %llu)",
        ctx->file_size,
        BUN_HEADER_SIZE);
    return BUN_MALFORMED;
  }

  // slurp the header into `buf`
  if (fread(buf, 1, BUN_HEADER_SIZE, ctx->file) != BUN_HEADER_SIZE) {
    bun_log_error(ctx, "Critical: file-read of bun-file failed.");
    return BUN_ERR_IO;
  }
  
  // printf("%02x %02x %02x %02x %02x %02x %02x %02x\n",
  //      buf[0], buf[1], buf[2], buf[3],
  //      buf[4], buf[5], buf[6], buf[7]);

  // printf("%02x %02x %02x %02x %02x %02x %02x %02x\n",
  //      buf[0], buf[1], buf[2], buf[3],
  //      buf[4], buf[5], buf[6], buf[7]);

  // TODO: populate `header` from `buf`.

  header->magic = read_u32_le(buf, 0);
  header->version_major = read_u16_le(buf, 4);
  header->version_minor = read_u16_le(buf, 6);
  header->asset_count = read_u32_le(buf, 8);
  ctx->asset_count = header->asset_count;
  header->asset_table_offset = read_u64_le(buf, 12);
  header->string_table_offset = read_u64_le(buf, 20);
  header->string_table_size = read_u64_le(buf, 28);
  header->data_section_offset = read_u64_le(buf, 36);
  header->data_section_size = read_u64_le(buf, 44);

  // TODO: validate fields and return BUN_MALFORMED or BUN_UNSUPPORTED
  // as required by the spec. The magic check is a good place to start.

  // Notes 4.1,  5: BUN Magic Field must match exactly
  if (header->magic != BUN_MAGIC) {
    bun_log_error(ctx, "Malformed: magic number does not match (expected: 0x%08X, got: 0x%08X)",
      BUN_MAGIC,
      header->magic);
    return BUN_MALFORMED;
  }
  // Notes 4.1,  7: version_major and version_minor must be 1 and 0
  // respectively, other versions are NOT supported
  if (header->version_major != BUN_VERSION_MAJOR || header->version_minor != BUN_VERSION_MINOR) {
    bun_log_error(ctx, "Unsupported: version not supported (expected: 1.0, got: %u.%u)",
      header->version_major,
      header->version_minor);
    return BUN_UNSUPPORTED;
  }

  // Notes 4.1,  3: The three offsets and two sizes must be divisible by 4

  if (header->asset_table_offset % 4 != 0) {
    bun_log_error(ctx, "Malformed: asset_table_offset not divisible by 4 (got: %llu)",
      header->asset_table_offset);
    exit_code = BUN_MALFORMED;
  }
  if (header->string_table_offset % 4 != 0) {
    bun_log_error(ctx, "Malformed: string_table_offset not divisible by 4 (got: %llu)",
      header->string_table_offset);
    exit_code = BUN_MALFORMED;
  }
  if (header->data_section_offset % 4 != 0) {
    bun_log_error(ctx, "Malformed: data_section_offset not divisible by 4 (got: %llu)",
      header->data_section_offset);
    exit_code = BUN_MALFORMED;
  }
  if (header->string_table_size % 4 != 0) {
    bun_log_error(ctx, "Malformed: string_table_size not divisible by 4 (got: %llu)",
      header->string_table_size);
    exit_code = BUN_MALFORMED;
  }
  if (header->data_section_size % 4 != 0) {
    bun_log_error(ctx, "Malformed: data_section_size not divisible by 4 (got: %llu)",
      header->data_section_size);
    exit_code = BUN_MALFORMED;
  }

  u64 file_size = (u64)ctx->file_size;
  u64 asset_table_size = (u64)header->asset_count * 48;

  // Notes 9,  3:All sections must in their entirety fall within the bounds of the file they are
  // contained in. If the declared start or end of a section falls outside the bounds
  // of the containing file, that is a parse error.

  if (sections_overlap(header->asset_table_offset, asset_table_size,
  header->data_section_offset, header->data_section_size)){
    bun_log_error(ctx, "Malformed file: asset/data section overlaps (asset: %llu-%llu, data: %llu-%llu)",
    header->asset_table_offset,
    header->asset_table_offset + asset_table_size,
    header->data_section_offset,
    header->data_section_offset + header->data_section_size);
    exit_code = BUN_MALFORMED;
  }

  if (sections_overlap(header->asset_table_offset, asset_table_size,
  header->string_table_offset, header->string_table_size)){
    bun_log_error(ctx, "Malformed file: asset/string section overlaps (asset: %llu-%llu, string: %llu-%llu)",
    header->asset_table_offset,
    header->asset_table_offset + asset_table_size,
    header->string_table_offset,
    header->string_table_offset+header->string_table_size);
    exit_code = BUN_MALFORMED;
  }

  if (sections_overlap(header->data_section_offset, header->data_section_size,
  header->string_table_offset, header->string_table_size)) {
    bun_log_error(ctx, "Malformed file: string/data section overlaps (string: %llu-%llu, data: %llu-%llu)",
    header->string_table_offset,
    header->string_table_offset+header->string_table_size,
    header->data_section_offset,
    header->data_section_offset+header->data_section_size);
    exit_code = BUN_MALFORMED;
  }  
  // Notes 9,  2: All sections must, in their entirety, remain within the bounds
  // of the file. If the declared start or end of a section falls outside the bounds
  // of the containing file, that is a parse error.

  if (header->asset_table_offset + asset_table_size > file_size  ||
  header->string_table_offset + header->string_table_size > file_size ||
  header->data_section_offset + header->data_section_size > file_size){
  bun_log_error(ctx,"Malformed file: section exceeds file bounds (asset: %llu-%llu, string: %llu-%llu, data: %llu-%llu, file_size: %llu)",
    header->asset_table_offset,
    (header->asset_table_offset + asset_table_size),
    header->string_table_offset,
    (header->string_table_offset + header->string_table_size),
    header->data_section_offset,
    (header->data_section_offset + header->data_section_size),
    file_size);
    exit_code = BUN_MALFORMED;
  }
  return exit_code;
}

bun_result_t bun_parse_assets(BunParseContext *ctx, const BunHeader *header) {
  u8 buf[BUN_ASSET_RECORD_SIZE];
  bun_result_t exit_code = BUN_OK;

  // go to start of asset table
  if (fseek(ctx->file, header->asset_table_offset, SEEK_SET) != 0) {
    return BUN_ERR_IO;
  }
  // saving all the asset records in ctx
  ctx->assets = malloc((size_t)header->asset_count * sizeof(BunAssetRecord)); 
  if (ctx->assets == NULL) {
    exit_code = BUN_ERR_IO;
  }
  ctx->asset_count = header->asset_count;
  //loop through all assets
  for (u32 i = 0; i < header->asset_count; i++) {
    u64 record_pos = header->asset_table_offset + ((u64)i * BUN_ASSET_RECORD_SIZE);

    if (fseek(ctx->file, (long)record_pos, SEEK_SET) != 0) {
        return BUN_ERR_IO;
    }
    if (fread(buf, 1, BUN_ASSET_RECORD_SIZE, ctx->file) != BUN_ASSET_RECORD_SIZE) {
        return BUN_ERR_IO;
    }
    ctx->assets[i].name_offset = read_u32_le(buf, 0);
    ctx->assets[i].name_length = read_u32_le(buf, 4);
    ctx->assets[i].data_offset = read_u64_le(buf, 8);
    ctx->assets[i].data_size = read_u64_le(buf, 16);
    ctx->assets[i].uncompressed_size = read_u64_le(buf, 24);
    ctx->assets[i].compression = read_u32_le(buf, 32);
    ctx->assets[i].type = read_u32_le(buf, 36);
    ctx->assets[i].checksum = read_u32_le(buf, 40);
    ctx->assets[i].flags = read_u32_le(buf, 44);
    ctx->assets[i].name_valid = 1;
    ctx->assets[i].data_valid = 1;
    //basic asset validation
    BunAssetRecord *asset = &ctx->assets[i];
    //name_length ≠ 0 → asset must have a name
    if (asset->name_length == 0) {
        bun_log_error(ctx, "Malformed: asset %u has zero-length name", i);
        asset->name_valid = 0;
        exit_code = BUN_MALFORMED;
    }
    //name within string table → name_offset + name_length must stay inside string section
    if ((u64)asset->name_offset + (u64)asset->name_length > header->string_table_size) {
        bun_log_error(ctx, "Malformed: asset %u name outside string table", i);
        exit_code =  BUN_MALFORMED;
    }
    //data within data section → data_offset + data_size must stay inside data section
    if (asset->data_offset + asset->data_size > header->data_section_size) {
        bun_log_error(ctx, "Malformed: asset %u data outside data section", i);
        asset->name_valid = 0;
        exit_code = BUN_MALFORMED;
    }
    // valid ASCII 
    //temporary buffer for full name
      char *name_buf = malloc(asset->name_length);
      if (name_buf == NULL) {
          return BUN_ERR_IO;
    }
    // seek to actual name position
    u64 name_pos = header->string_table_offset + asset->name_offset;
    if (fseek(ctx->file, (long)name_pos, SEEK_SET) != 0) {
        free(name_buf);
        return BUN_MALFORMED;
    }
    // Read full name
    if (fread(name_buf, 1, asset->name_length, ctx->file) != asset->name_length) {
        free(name_buf);
        return BUN_MALFORMED;
    }
    // Validate ASCII
    for (u32 k = 0; k < asset->name_length; k++) {
      unsigned char c = (unsigned char)name_buf[k];
      if (!isprint(c)) {
          bun_log_error(ctx, "Malformed: asset %u has non-printable ASCII in name", i);
          exit_code = BUN_MALFORMED;
      }
    }
    free(name_buf);
    //data within data section → data_offset + data_size must stay inside data section
    if (asset->data_offset + asset->data_size > header->data_section_size) {
        bun_log_error(ctx, "Malformed: asset %u data outside data section", i);
        exit_code = BUN_MALFORMED;
    }
    //only allowed values (e.g. 0 = none, 1 = RLE)
    if (asset->compression > 2 || asset->compression < 0) {
        bun_log_error(ctx, "Unsupported: asset %u has unknown compression %u", i, asset->compression);
        exit_code = BUN_UNSUPPORTED;
    }
    //compression unsupported (zlib) → reject if compression = 2
    if (asset->compression == 2) {
        bun_log_error(ctx, "Unsupported: asset %u uses zlib compression", i);
        exit_code = BUN_UNSUPPORTED;
    }
    //uncompressed_size rule → must be 0 if not compressed
    if (asset->compression == 0 && asset->uncompressed_size != 0) {
        bun_log_error(ctx, "Malformed: asset %u uncompressed_size must be 0 when not compressed", i);
        exit_code = BUN_MALFORMED;
    }
    //checksum rule → reject if non-zero (unsupported feature)
    if (asset->checksum != 0) {
        bun_log_error(ctx, "Unsupported: asset %u uses checksum validation", i);
        exit_code =  BUN_UNSUPPORTED;
    }
    //flags valid → only allowed bits (e.g. encrypted/executable)
    if (asset->flags & ~(BUN_FLAG_ENCRYPTED | BUN_FLAG_EXECUTABLE)) {
        bun_log_error(ctx, "Unsupported: asset %u has unsupported flags 0x%08x", i, asset->flags);
        exit_code = BUN_UNSUPPORTED;
    }
  // TODO: validation 

  // Loop through each asset record and parse / validate data
    // Process each asset

    u64 data_read = 0;

    BunAssetRecord current_asset = ctx->assets[i];
    // Note: fseek() 'off' parameter takes a long int.
    // If we had a UINT64_MAX bit offset, would indicate a ~ 4gb file, which is
    // super large But don't think the file size should be that big?
    if (fseek(ctx->file,
              header->data_section_offset + current_asset.data_offset,
              SEEK_SET) != 0) {
      bun_log_error(ctx, "Error when trying to move FP in data section");
      exit_code = BUN_ERR_IO;
    }
    // RLE Compression Checks
    if (current_asset.compression == 1) {
      u64 uncompressed_read = 0;

      // Notes 8.1: If data size isn't an even number of bytes, then the data is
      // invalid Since (count, pair) is an even number of bytes
      if (current_asset.data_size % 2 != 0) {
        exit_code = BUN_MALFORMED;
        bun_log_error(
            ctx, "Malformed: RLE Data Size is not an even number (got %llu)",
            current_asset.data_size);
      }
      BunRlePair current_pair;

      while (data_read < current_asset.data_size) {
        // Assuming that each RLE pair is a (u8 count, u8 value) pair.
        if (fread(&current_pair, sizeof(current_pair), 1, ctx->file) != 1) {
          exit_code = BUN_ERR_IO;
          bun_log_error(ctx, "IO Error: Could not read BUN file successfully");
          return exit_code;
        };

        data_read += 2;

        // Notes 8.2: Count must not be 0
        if (current_pair.count == 0) {
          exit_code = BUN_MALFORMED;
          bun_log_error(ctx, "Malformed: RLE count field must not be 0");
          continue;
        }

        // Check for if the count > uncomp size
        if (current_pair.count > current_asset.uncompressed_size) {
          exit_code = BUN_MALFORMED;
          bun_log_error(ctx,
                        "Malformed: RLE count field exceeds the uncompressed "
                        "size (got: %u)",
                        current_pair.count);
          continue;
        }

        // Check to ensure we dont exceed the specified uncompressed size
        if (uncompressed_read >
            current_asset.uncompressed_size - current_pair.count) {
          exit_code = BUN_MALFORMED;
          bun_log_error(ctx,
                        "Malformed: Uncompressed size does not match specified "
                        "size (got %llu)",
                        current_asset.uncompressed_size);
          continue;
        }

        uncompressed_read += current_pair.count;

        // TODO: Do we expand and then write, or store these pairs somewhere ;
        // if its valid then we write up to 60 bytes.
      }
      // Check for truncation
      if (uncompressed_read < current_asset.uncompressed_size) {
        exit_code = BUN_MALFORMED;
        bun_log_error(ctx,
                      "Uncompressed size is less than the expected size. "
                      "Expected: %lu | Got: %lu",
                      current_asset.uncompressed_size, uncompressed_read);
      }
    }
  }

  return exit_code;
}

bun_result_t bun_close(BunParseContext *ctx) {
  assert(ctx->file);
  free(ctx->assets);
  ctx->assets = NULL;
  int res = fclose(ctx->file);
  if (res) {
    return BUN_ERR_IO;
  } else {
    ctx->file = NULL;
    return BUN_OK;
  }
}
