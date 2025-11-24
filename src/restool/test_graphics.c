// test_graphics.c - Unit tests for graphics decoding
#include "graphics.h"
#include <stdio.h>
#include <string.h>

// Test 4bpp decoding with a known pattern
bool TestTile4bpp(void) {
  // Create a simple test pattern: vertical stripes
  // Left half (pixels 0-3): color 0xF (white)
  // Right half (pixels 4-7): color 0x0 (black)
  uint8_t test_tile[32] = {0};

  // For 4bpp, each row needs 4 bytes of data
  // Bitplane pattern for left=0xF, right=0x0:
  //   Pixels: FFFFFFFF 00000000 (but SNES is reversed, MSB=left)
  //   Bit pattern: 11110000 for each bitplane
  for (int y = 0; y < 8; y++) {
    test_tile[y * 2 + 0] = 0xF0;  // Bitplane 0
    test_tile[y * 2 + 1] = 0xF0;  // Bitplane 1
    test_tile[y * 2 + 16] = 0xF0; // Bitplane 2
    test_tile[y * 2 + 17] = 0xF0; // Bitplane 3
  }

  uint8_t decoded[64];
  DecodeTile4bpp(test_tile, decoded);

  // Check pattern
  printf("Test 4bpp decoding:\n");
  for (int y = 0; y < 8; y++) {
    printf("  Row %d: ", y);
    for (int x = 0; x < 8; x++) {
      printf("%X ", decoded[y * 8 + x]);
    }
    printf("\n");
  }

  // Verify: left half should be 0xF, right half 0x0
  bool pass = true;
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 4; x++) {
      if (decoded[y * 8 + x] != 0xF) {
        printf("ERROR: Expected 0xF at (%d,%d), got 0x%X\n", x, y, decoded[y * 8 + x]);
        pass = false;
      }
    }
    for (int x = 4; x < 8; x++) {
      if (decoded[y * 8 + x] != 0x0) {
        printf("ERROR: Expected 0x0 at (%d,%d), got 0x%X\n", x, y, decoded[y * 8 + x]);
        pass = false;
      }
    }
  }

  return pass;
}

int main(void) {
  printf("Running graphics decoder tests...\n\n");

  if (TestTile4bpp()) {
    printf("\n✓ 4bpp test PASSED\n");
  } else {
    printf("\n✗ 4bpp test FAILED\n");
    return 1;
  }

  return 0;
}
