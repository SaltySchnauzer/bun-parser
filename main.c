#include <stdio.h>
#include <stdlib.h>

#include "bun.h"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <file.bun>\n", argv[0]);
    return BUN_ERR_IO;
  }
  const char *path = argv[1];

  BunParseContext ctx = {0};
  BunHeader header  = {0};

  bun_result_t result = bun_open(path, &ctx);
  if (result != BUN_OK) {
    fprintf(stderr, "Error: could not open '%s'\n", path);
    return result;
  }

  result = bun_parse_header(&ctx, &header);
  if (result != BUN_OK) {
    // bun_parse_header returns a code; printing the specifics is up to
    // you -- you may want to extend the API to return error details
    fprintf(stderr, "Error: header invalid or unsupported (code %d)\n", result);
    bun_close(&ctx);
    return result;
  }

  result = bun_parse_assets(&ctx, &header);
  if (result != BUN_OK) {
    fprintf(stderr, "Error: assets invalid or unsupported (code %d)\n", result);
    bun_close(&ctx);
  return result;
  }
  //on BUN_OK, print human-readable summary to stdout.
  //TODO
  // on BUN_MALFORMED / BUN_UNSUPPORTED, print violation list to stderr.
  // See project brief for output requirements.
  printf("Header:\n");
  printf("  magic: 0x%08x\n", header.magic);
  printf("  version: %u.%u\n", header.version_major, header.version_minor);
  printf("  asset count: %u\n", header.asset_count);
  printf("  asset table offset: %llu\n", (unsigned long long)header.asset_table_offset);
  printf("  string table offset: %llu\n", (unsigned long long)header.string_table_offset);
  printf("  string table size: %llu\n", (unsigned long long)header.string_table_size);
  printf("  data section offset: %llu\n", (unsigned long long)header.data_section_offset);
  printf("  data section size: %llu\n", (unsigned long long)header.data_section_size);

  printf("\nAssets:\n");
  for (u32 i = 0; i < ctx.asset_count; i++) {
    BunAssetRecord asset = ctx.assets[i];
    printf("Asset %u:\n", i+1);
    printf("  name_offset: %u\n", asset.name_offset);
    printf("  name_length: %u\n", asset.name_length);
    printf("  data_offset: %llu\n", (unsigned long long)asset.data_offset);
    printf("  data_size: %llu\n", (unsigned long long)asset.data_size);
    printf("  uncompressed_size: %llu\n", (unsigned long long)asset.uncompressed_size);
    printf("  compression: %u\n", asset.compression);
    printf("  type: %u\n", asset.type);
    printf("  checksum: %u\n", asset.checksum);
    printf("  flags: %u\n", asset.flags);
  }

  bun_close(&ctx);
  return result;
}
