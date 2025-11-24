// test_enemy_decomp.c - Test enemy damage data decompression
#include "restool_util.h"
#include "../logging.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: %s <rom_file>\n", argv[0]);
    return 1;
  }

  InitializeLogging();

  Rom *rom = Rom_Load(argv[1]);
  if (!rom) {
    LogError("Failed to load ROM: %s", argv[1]);
    return 1;
  }

  printf("Decompressing kEnemyDamageData from 0x83E800...\n");
  DecompressedData *decomp = Snes_Decompress(rom, 0x83E800, true);
  if (!decomp) {
    LogError("Decompression failed");
    Rom_Free(rom);
    return 1;
  }

  printf("Decompressed size: %zu bytes\n", decomp->size);
  printf("Compressed size: %zu bytes\n", decomp->compressed_size);

  printf("First 20 bytes: ");
  for (int i = 0; i < 20 && i < decomp->size; i++) {
    printf("%02X ", decomp->data[i]);
  }
  printf("\n");

  printf("Last 20 bytes: ");
  for (int i = decomp->size - 20; i < decomp->size; i++) {
    printf("%02X ", decomp->data[i]);
  }
  printf("\n");

  // Check byte 205
  if (decomp->size > 205) {
    printf("Byte 205: 0x%02X (Python expects 0x03)\n", decomp->data[205]);
  }

  Snes_FreeDecompressed(decomp);
  Rom_Free(rom);
  return 0;
}
