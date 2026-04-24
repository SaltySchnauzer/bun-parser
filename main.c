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
    fprintf(stderr, "Error: header invalid or unsupported (code %d)\n", result);
    bun_close(&ctx);
    return result;
  }

  result = bun_parse_assets(&ctx, &header);

  // Print out assets if successful!

  if (result == BUN_OK){
    for (u32 i = 0; i < ctx.asset_count; i++){
      printf("Asset Number: %d\n", i);
      printf("  Name: %.60s\n", ctx.assets[i].string_table_entry);
      printf("  Data: %.60s\n", ctx.assets[i].data_table_entry);
    }
  }

  //
  // Print out all errors!
  //

  rewind(ctx.errors);
  char error_buffer[10000]; // should be large enough tee hee
  fgets(error_buffer, sizeof(error_buffer), ctx.errors);
  fprintf(stderr, "%s", error_buffer);
  fclose(ctx.errors);
  // TODO: on BUN_OK, print human-readable summary to stdout.
  //     on BUN_MALFORMED / BUN_UNSUPPORTED, print violation list to stderr.
  //     See project brief for output requirements.

  bun_close(&ctx);
  return result;
}
