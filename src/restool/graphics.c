// graphics.c - SNES graphics decoding implementation
#include "graphics.h"
#include "../logging.h"
#include <stdlib.h>
#include <string.h>

// stb_image_write for PNG output
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// ============================================================================
// SNES Tile Decoding
// ============================================================================

void DecodeTile2bpp(const uint8_t *src, uint8_t *dst) {
  // 2bpp: 16 bytes per tile (2 bytes per row, 8 rows)
  // Bitplane 0: bytes 0,2,4,6,8,10,12,14
  // Bitplane 1: bytes 1,3,5,7,9,11,13,15

  for (int y = 0; y < 8; y++) {
    uint8_t d0 = src[y * 2 + 0];  // Bitplane 0
    uint8_t d1 = src[y * 2 + 1];  // Bitplane 1

    for (int x = 0; x < 8; x++) {
      // Extract bit from each plane (LSB first, so reverse x)
      uint8_t bit0 = (d0 >> (7 - x)) & 1;
      uint8_t bit1 = (d1 >> (7 - x)) & 1;

      // Combine bits into palette index (0-3)
      uint8_t color = bit0 | (bit1 << 1);
      dst[y * 8 + x] = color;
    }
  }
}

void DecodeTile3bpp(const uint8_t *src, uint8_t *dst) {
  // 3bpp: 24 bytes per tile
  // Bitplanes 0,1: bytes 0-15 (2 bytes per row)
  // Bitplane 2: bytes 16-23 (1 byte per row)

  for (int y = 0; y < 8; y++) {
    uint8_t d0 = src[y * 2 + 0];   // Bitplane 0
    uint8_t d1 = src[y * 2 + 1];   // Bitplane 1
    uint8_t d2 = src[y + 16];      // Bitplane 2 (offset by 16)

    for (int x = 0; x < 8; x++) {
      uint8_t bit0 = (d0 >> (7 - x)) & 1;
      uint8_t bit1 = (d1 >> (7 - x)) & 1;
      uint8_t bit2 = (d2 >> (7 - x)) & 1;

      // Combine bits into palette index (0-7)
      uint8_t color = bit0 | (bit1 << 1) | (bit2 << 2);
      dst[y * 8 + x] = color;
    }
  }
}

void DecodeTile4bpp(const uint8_t *src, uint8_t *dst) {
  // 4bpp: 32 bytes per tile
  // Bitplanes 0,1: bytes 0-15 (2 bytes per row)
  // Bitplanes 2,3: bytes 16-31 (2 bytes per row, offset by 16)

  for (int y = 0; y < 8; y++) {
    uint8_t d0 = src[y * 2 + 0];   // Bitplane 0
    uint8_t d1 = src[y * 2 + 1];   // Bitplane 1
    uint8_t d2 = src[y * 2 + 16];  // Bitplane 2
    uint8_t d3 = src[y * 2 + 17];  // Bitplane 3

    for (int x = 0; x < 8; x++) {
      uint8_t bit0 = (d0 >> (7 - x)) & 1;
      uint8_t bit1 = (d1 >> (7 - x)) & 1;
      uint8_t bit2 = (d2 >> (7 - x)) & 1;
      uint8_t bit3 = (d3 >> (7 - x)) & 1;

      // Combine bits into palette index (0-15)
      uint8_t color = bit0 | (bit1 << 1) | (bit2 << 2) | (bit3 << 3);
      dst[y * 8 + x] = color;
    }
  }
}

TileData* DecodeTileset2bpp(const uint8_t *src, int tile_count, int tiles_per_row) {
  TileData *tile_data = calloc(1, sizeof(TileData));
  if (!tile_data) {
    LogError("Failed to allocate TileData");
    return NULL;
  }

  int rows = (tile_count + tiles_per_row - 1) / tiles_per_row;
  tile_data->width = tiles_per_row * 8;
  tile_data->height = rows * 8;
  tile_data->bpp = 2;
  tile_data->pixels = malloc(tile_data->width * tile_data->height);

  if (!tile_data->pixels) {
    LogError("Failed to allocate pixel buffer");
    free(tile_data);
    return NULL;
  }

  // Decode each tile
  for (int i = 0; i < tile_count; i++) {
    int tile_x = (i % tiles_per_row) * 8;
    int tile_y = (i / tiles_per_row) * 8;

    uint8_t tile_buffer[64];
    DecodeTile2bpp(src + i * 16, tile_buffer);

    // Copy tile to bitmap
    for (int y = 0; y < 8; y++) {
      for (int x = 0; x < 8; x++) {
        int dst_x = tile_x + x;
        int dst_y = tile_y + y;
        tile_data->pixels[dst_y * tile_data->width + dst_x] = tile_buffer[y * 8 + x];
      }
    }
  }

  return tile_data;
}

TileData* DecodeTileset3bpp(const uint8_t *src, int tile_count, int tiles_per_row) {
  TileData *tile_data = calloc(1, sizeof(TileData));
  if (!tile_data) {
    LogError("Failed to allocate TileData");
    return NULL;
  }

  int rows = (tile_count + tiles_per_row - 1) / tiles_per_row;
  tile_data->width = tiles_per_row * 8;
  tile_data->height = rows * 8;
  tile_data->bpp = 3;
  tile_data->pixels = malloc(tile_data->width * tile_data->height);

  if (!tile_data->pixels) {
    LogError("Failed to allocate pixel buffer");
    free(tile_data);
    return NULL;
  }

  for (int i = 0; i < tile_count; i++) {
    int tile_x = (i % tiles_per_row) * 8;
    int tile_y = (i / tiles_per_row) * 8;

    uint8_t tile_buffer[64];
    DecodeTile3bpp(src + i * 24, tile_buffer);

    for (int y = 0; y < 8; y++) {
      for (int x = 0; x < 8; x++) {
        int dst_x = tile_x + x;
        int dst_y = tile_y + y;
        tile_data->pixels[dst_y * tile_data->width + dst_x] = tile_buffer[y * 8 + x];
      }
    }
  }

  return tile_data;
}

TileData* DecodeTileset4bpp(const uint8_t *src, int tile_count, int tiles_per_row) {
  TileData *tile_data = calloc(1, sizeof(TileData));
  if (!tile_data) {
    LogError("Failed to allocate TileData");
    return NULL;
  }

  int rows = (tile_count + tiles_per_row - 1) / tiles_per_row;
  tile_data->width = tiles_per_row * 8;
  tile_data->height = rows * 8;
  tile_data->bpp = 4;
  tile_data->pixels = malloc(tile_data->width * tile_data->height);

  if (!tile_data->pixels) {
    LogError("Failed to allocate pixel buffer");
    free(tile_data);
    return NULL;
  }

  for (int i = 0; i < tile_count; i++) {
    int tile_x = (i % tiles_per_row) * 8;
    int tile_y = (i / tiles_per_row) * 8;

    uint8_t tile_buffer[64];
    DecodeTile4bpp(src + i * 32, tile_buffer);

    for (int y = 0; y < 8; y++) {
      for (int x = 0; x < 8; x++) {
        int dst_x = tile_x + x;
        int dst_y = tile_y + y;
        tile_data->pixels[dst_y * tile_data->width + dst_x] = tile_buffer[y * 8 + x];
      }
    }
  }

  return tile_data;
}

void FreeTileData(TileData *tile_data) {
  if (tile_data) {
    free(tile_data->pixels);
    free(tile_data);
  }
}

// ============================================================================
// Palette Conversion
// ============================================================================

Color SnesColorToRGBA(uint16_t snes_color) {
  // SNES BGR555 format: xBBBBBGGGGGRRRRR
  uint8_t r5 = (snes_color >> 0) & 0x1F;
  uint8_t g5 = (snes_color >> 5) & 0x1F;
  uint8_t b5 = (snes_color >> 10) & 0x1F;

  // Expand 5-bit to 8-bit by replicating upper bits
  // Formula: (x << 3) | (x >> 2)
  // This ensures 0x1F (31) -> 0xFF (255)
  Color color;
  color.r = (r5 << 3) | (r5 >> 2);
  color.g = (g5 << 3) | (g5 >> 2);
  color.b = (b5 << 3) | (b5 >> 2);
  color.a = 255;  // Fully opaque

  return color;
}

void SnesPaletteToRGBA(const uint16_t *snes_palette, int count, Color *out_palette) {
  for (int i = 0; i < count; i++) {
    out_palette[i] = SnesColorToRGBA(snes_palette[i]);
  }
}

// ============================================================================
// PNG Output
// ============================================================================

bool WritePNG_Indexed(const char *filename, int width, int height,
                      const uint8_t *pixels, const Color *palette, int palette_size) {
  // Convert indexed image to RGBA for PNG output
  uint8_t *rgba = malloc(width * height * 4);
  if (!rgba) {
    LogError("Failed to allocate RGBA buffer for PNG");
    return false;
  }

  for (int i = 0; i < width * height; i++) {
    uint8_t index = pixels[i];
    if (index >= palette_size) {
      LogWarn("Pixel index %d out of palette range (0-%d)", index, palette_size - 1);
      index = 0;
    }

    rgba[i * 4 + 0] = palette[index].r;
    rgba[i * 4 + 1] = palette[index].g;
    rgba[i * 4 + 2] = palette[index].b;
    rgba[i * 4 + 3] = palette[index].a;
  }

  int result = stbi_write_png(filename, width, height, 4, rgba, width * 4);
  free(rgba);

  if (!result) {
    LogError("Failed to write PNG: %s", filename);
    return false;
  }

  return true;
}

bool WritePNG_RGBA(const char *filename, int width, int height, const uint8_t *rgba_pixels) {
  int result = stbi_write_png(filename, width, height, 4, rgba_pixels, width * 4);

  if (!result) {
    LogError("Failed to write PNG: %s", filename);
    return false;
  }

  return true;
}
