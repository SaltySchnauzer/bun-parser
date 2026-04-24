#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "bun.h"

/**
 * Example helper: convert 4 bytes in `buf`, positioned at `offset`,
 * into a little-endian u32.
 */
static u16 read_u16_le(const u8 *buf, size_t offset) {
  return (u16)buf[offset]
     |  (u16)buf[offset + 1] << 8;
}

static u32 read_u32_le(const u8 *buf, size_t offset) {
  return (u32)buf[offset]
     | (u32)buf[offset + 1] << 8
     | (u32)buf[offset + 2] << 16
     | (u32)buf[offset + 3] << 24;
}

static u64 read_u64_le(const u8 *buf, size_t offset) {
  return (u64)buf[offset]
     | (u64)buf[offset + 1] << 8
     | (u64)buf[offset + 2] << 16
     | (u64)buf[offset + 3] << 24
     | (u64)buf[offset + 4] << 32
     | (u64)buf[offset + 5] << 40
     | (u64)buf[offset + 6] << 48
     | (u64)buf[offset + 7] << 56;
}
//
// API implementation
//

//
// Basic helper function to handle error logging to tempfile.
//
int bun_log_error(BunParseContext *ctx, char *message) {
  fprintf(ctx->errors, "%s\n", message);
  return 0;
}

bun_result_t bun_open(const char *path, BunParseContext *ctx) {
  // we open the file; seek to the end, to get the size; then jump back to the
  // beginning, ready to start parsing.

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

  // our file is far too short, and cannot be valid!
  // (query: how do we let `main` know that "file was too short"
  // was the exact problem? Where can we put details about the
  // exact validation problem that occurred?)
  if (ctx->file_size < (long)BUN_HEADER_SIZE) {
    return BUN_MALFORMED;
  }

  // slurp the header into `buf`
  if (fread(buf, 1, BUN_HEADER_SIZE, ctx->file) != BUN_HEADER_SIZE) {
    return BUN_ERR_IO;
  }

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
    return BUN_MALFORMED;
  }
  // Notes 4.1,  7: version_major and version_minor must be 1 and 0
  // respectively, other versions are NOT supported
  if (header->version_major != 1 || header->version_minor != 0) {
    return BUN_UNSUPPORTED;
  }

  // Notes 4.1,  3: The three offsets and two sizes must be divisible by 4

  // Checking table offsets
  // For specific error handling (Like if we wanna say that a specific table
  // offset is invalid) feel free to split this up
  if (header->string_table_offset % 4 != 0 ||
      header->asset_table_offset % 4 != 0 ||
      header->data_section_offset % 4 != 0) {
    return BUN_MALFORMED;
  }
  // Checking for string and data sizes
  // Again, feel free to split this up if we wanna specify if a certain section
  // is malformed
  if (header->string_table_size % 4 != 0 ||
      header->data_section_size % 4 != 0) {
    return BUN_MALFORMED;
  }

  return BUN_OK;
}

bun_result_t bun_parse_assets(BunParseContext *ctx, const BunHeader *header) {
  u8 buf[BUN_ASSET_RECORD_SIZE];
  //go to start of asset table
  if (fseek(ctx->file, header->asset_table_offset, SEEK_SET) != 0) {
      return BUN_ERR_IO;
  }
  //saving all the asset records in ctx 
  ctx->assets = malloc(header->asset_count * BUN_ASSET_RECORD_SIZE);
  if (ctx->assets == NULL) {
    return BUN_ERR_IO;
  }
  //loop through all assets
  for (u32 i = 0; i < header->asset_count; i++) {
    if (fread(buf, 1, BUN_ASSET_RECORD_SIZE, ctx->file) != BUN_ASSET_RECORD_SIZE) {
        return BUN_ERR_IO;
    }
    ctx->assets[i].name_offset = read_u32_le(buf, 0);
    ctx->assets[i].name_length = read_u32_le(buf, 4);
    ctx->assets[i].data_offset = read_u64_le(buf, 8);
    ctx->assets[i].data_size   = read_u64_le(buf, 16);
    ctx->assets[i].uncompressed_size = read_u64_le(buf,24);
    ctx->assets[i].compression = read_u32_le(buf, 32);
    ctx->assets[i].type = read_u32_le(buf, 36);
    ctx->assets[i].checksum = read_u32_le(buf, 40);
    ctx->assets[i].flags = read_u32_le(buf, 44);
  }
  // to do: validation 

  return BUN_OK;
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
