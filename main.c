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

  ctx.errors = tmpfile();
  if (ctx.errors == NULL) {
    fprintf(stderr, "Error: could not create tempfile.\n");
    return 1;
  }

  result = bun_parse_header(&ctx, &header);
  if (result != BUN_OK) {
    // bun_parse_header returns a code; printing the specifics is up to
    // you -- you may want to extend the API to return error details
    fprintf(stderr, "\033[31mError: header invalid or unsupported (code %d)\033[0m\n", result);
  }

  // NOTE: the parser will attempt asset read and printout even on a fail!

  bun_result_t new_result = bun_parse_assets(&ctx, &header);
  if (new_result != BUN_OK){
    if (new_result == BUN_MALFORMED){
      result = new_result;
      fprintf(stderr, "\033[31mError: assets invalid or unsupported (code %d)\033[0m\n", result);
    }
  }
  if (new_result != BUN_OK) {
  }

  //
  // Print errors and close tmpfile
  //
  rewind(ctx.errors);
  char error_buffer[10000]; // should be large enough tee hee
  fgets(error_buffer, sizeof(error_buffer), ctx.errors);
  fprintf(stderr, "\033[31m%s\033[0m", error_buffer);
  fclose(ctx.errors);

  //on BUN_OK, print human-readable summary to stdout.
  //TODO
  // on BUN_MALFORMED / BUN_UNSUPPORTED, print violation list to stderr.
  // See project brief for output requirements.
  //
  // Print out header
  //
  printf("Header:\n");
  printf("  magic: 0x%08x\n", header.magic);
  printf("  version: %u.%u\n", header.version_major, header.version_minor);
  printf("  asset count: %u\n", header.asset_count);
  printf("  asset table offset: %llu\n", (unsigned long long)header.asset_table_offset);
  printf("  string table offset: %llu\n", (unsigned long long)header.string_table_offset);
  printf("  string table size: %llu\n", (unsigned long long)header.string_table_size);
  printf("  data section offset: %llu\n", (unsigned long long)header.data_section_offset);
  printf("  data section size: %llu\n", (unsigned long long)header.data_section_size);

  //
  // Print out asset details
  //
  printf("\nAssets:\n");
  for (u32 i = 0; i < ctx.asset_count; i++) {
    BunAssetRecord asset = ctx.assets[i];
    //==asset name printing==//
    char name_buffer[61];    //buffer for storing asset name; re-using for all assets
    size_t read_size = asset.name_length < 60 ? asset.name_length : 60;
    u64 name_pos = header.string_table_offset + asset.name_offset;
    if (fseek(ctx.file, (long)name_pos, SEEK_SET) != 0) {
      return BUN_ERR_IO;
    }
    if (fread(name_buffer, 1, read_size, ctx.file) != read_size) {
      return BUN_ERR_IO;
    }
    name_buffer[read_size] = '\0';
    //===data printing===//
    char data_buffer[61];    //buffer for storing asset data; re-using for all assets
    size_t read_size_data = asset.data_size < 60 ? asset.data_size : 60;
    u64 data_pos = header.data_section_offset + asset.data_offset;
    if (fseek(ctx.file, (long)data_pos, SEEK_SET) != 0) {
      return BUN_ERR_IO;
    }
    if (fread(data_buffer, 1, read_size_data, ctx.file) != read_size_data) {
      return BUN_ERR_IO;
    }
    data_buffer[read_size_data] = '\0';
    printf("Asset %u:\n", i);
    printf("  Name: %s\n", name_buffer);
    printf("  name_offset: %u\n", asset.name_offset);
    printf("  name_length: %u\n", asset.name_length);
    printf("  data_offset: %llu\n", (unsigned long long)asset.data_offset);
    printf("  data_size: %llu\n", (unsigned long long)asset.data_size);
    printf("  uncompressed_size: %llu\n", (unsigned long long)asset.uncompressed_size);
    printf("  compression: %u\n", asset.compression);
    printf("  type: %u\n", asset.type);
    printf("  checksum: %u\n", asset.checksum);
    printf("  flags: %u\n", asset.flags);
    printf("  Data: ");
    for (size_t j = 0; j < read_size_data; j++) {
        unsigned char b = (unsigned char)data_buffer[j];

        if (b >= 32 && b <= 126) {
            printf("%c", b);   // printable ASCII
        } else {
            printf(".");       // non-printable
        }
    }
    printf("\n");
  }
  
  bun_close(&ctx);
  return result;
}
