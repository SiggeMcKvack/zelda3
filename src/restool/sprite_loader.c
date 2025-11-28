// sprite_loader.c - Load sprite graphics from PNG files
// Implements --sprites-from-png functionality matching Python restool

#include "sprite_loader.h"
#include "../logging.h"
#include "../platform.h"
#include "lodepng.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// PNG image dimensions for sprite sheets
#define PNG_WIDTH 148  // BIGW in Python

// Special pixel colors used for tag encoding and structure
#define COLOR_TAG_MARKER  0x404040  // Gray marker for tag detection
#define COLOR_TAG_ONE     0xf0f0f0  // Light gray = bit 1
#define COLOR_TAG_ZERO    0xe0e0e0  // Darker gray = bit 0
#define COLOR_TRANSPARENT 0x808000  // Teal = transparent (index 0)
#define COLOR_EMPTY       0xf0f0f0  // Empty tile background

// High palette tilesets (use indices 9-15 instead of 1-7)
static const int kHighPaletteTilesets[] = {
  0x52, 0x53, 0x5a, 0x5b, 0x5c, 0x5e, 0x5f
};
static const int kHighPaletteTilesetCount = sizeof(kHighPaletteTilesets) / sizeof(kHighPaletteTilesets[0]);

// PNG filenames to process
static const char *kPngFilenames[] = {
  "sprites_0.png", "sprites_1.png", "sprites_2.png", "sprites_3.png",
  "sprites_4.png", "sprites_5.png", "sprites_6.png", "sprites_7.png",
  "sprites_8.png", "sprites_9.png", "sprites_A.png", "sprites_B.png",
  "sprites_C.png", "sprites_D.png", "sprites_E.png", "sprites_F.png",
  "sprites_X.png"
};
static const int kPngFilenameCount = sizeof(kPngFilenames) / sizeof(kPngFilenames[0]);

// Palette lookup table for mapping RGB colors to indexed values
typedef struct {
  uint32_t colors[16];
  int count;
} PaletteLut;

// ============================================================================
// Helper Functions
// ============================================================================

bool SpriteLoader_IsHighPaletteTileset(int tileset) {
  for (int i = 0; i < kHighPaletteTilesetCount; i++) {
    if (kHighPaletteTilesets[i] == tileset)
      return true;
  }
  return false;
}

// Convert RGB24 to packed uint32 (0x00RRGGBB format)
static inline uint32_t PackRGB(uint8_t r, uint8_t g, uint8_t b) {
  return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16);
}

// Find a color in the palette lookup table
static int FindColorInLut(PaletteLut *lut, uint32_t color) {
  for (int i = 0; i < lut->count; i++) {
    if (lut->colors[i] == color)
      return i;
  }
  return -1;
}

// Check if an 8x8 region is empty (all COLOR_EMPTY)
static bool IsEmptyTile(const uint32_t *pixels, int pitch, int pos) {
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      if (pixels[pos + y * pitch + x] != COLOR_EMPTY)
        return false;
    }
  }
  return true;
}

// ============================================================================
// Tag Detection and Decoding
// ============================================================================

// Find next tag marker in the image
// Returns position of tag (2 rows below the marker) or -1 if not found
static int FindNextTag(const uint32_t *pixels, int size, int pitch, int start_pos) {
  int step = (start_pos == 0) ? 1 : pitch;

  for (int i = start_pos; i < size - pitch * 2; i += step) {
    // Check for tag marker pattern:
    // - Two vertical gray pixels (0x404040)
    // - Followed by alternating pattern in next row
    if (pixels[i] == COLOR_TAG_MARKER &&
        pixels[i + pitch] == COLOR_TAG_MARKER &&
        pixels[i + pitch * 2] == COLOR_TAG_ONE &&
        pixels[i + pitch * 2 - 1] == COLOR_TAG_ZERO &&
        pixels[i + pitch * 2 - 2] == COLOR_TAG_ONE &&
        pixels[i + pitch * 2 - 3] == COLOR_TAG_ZERO &&
        pixels[i + pitch * 2 - 4] == COLOR_TAG_ONE &&
        pixels[i + pitch * 2 - 5] == COLOR_TAG_ZERO &&
        pixels[i + pitch * 2 - 6] == COLOR_TAG_ONE &&
        pixels[i + pitch * 2 - 7] == COLOR_TAG_ZERO) {
      return i + pitch * 2;
    }
  }
  return -1;
}

// Decode tag from 64 pixels to extract metadata
// Returns true on success, false on checksum failure
static bool DecodeTag(const uint32_t *pixels, int tag_pos,
                      int *out_pal_base, int *out_tileset,
                      bool *out_headerless, int *out_pal_subidx) {
  // Read 64 bits from pixels (right to left from tag_pos)
  uint64_t tag = 0;
  for (int i = 0; i < 64; i++) {
    uint32_t v = pixels[tag_pos - 63 + i];
    if (v != COLOR_TAG_ZERO && v != COLOR_TAG_ONE) {
      LogError("Invalid tag pixel at offset %d: 0x%06X", i, v);
      return false;
    }
    tag = tag * 2 + (v == COLOR_TAG_ONE ? 1 : 0);
  }

  // Validate magic marker (lower 9 bits should be 0x55)
  if ((tag & 0x1FF) != 0x55) {
    LogError("Invalid tag magic: expected 0x55, got 0x%X", (int)(tag & 0x1FF));
    return false;
  }
  tag >>= 9;

  // Validate checksum
  // After removing magic, format is: [data...][checksum_byte]
  // where checksum_byte = (data + 41) % 255
  int stored_checksum = tag & 0xFF;
  int data_for_checksum = (tag >> 8) & 0xFFFF;  // 16 bits of data used for checksum
  int expected_checksum = (data_for_checksum + 41) % 255;
  if (stored_checksum != expected_checksum) {
    LogError("Invalid tag checksum: expected %d, got %d", expected_checksum, stored_checksum);
    return false;
  }
  tag >>= 8;

  // Extract fields
  *out_pal_base = tag & 0x7;          // 3 bits
  *out_tileset = (tag >> 3) & 0x7F;   // 7 bits
  *out_headerless = (tag >> 10) & 1;  // 1 bit
  *out_pal_subidx = (tag >> 11) & 0x1F; // 5 bits

  return true;
}

// Determine image rectangles based on box height
// Returns number of rects (1 or 2)
static int DetermineImageRects(const uint32_t *pixels, int tag_pos, int pitch,
                               int *out_idx, int *out_pos, int *out_box_height) {
  // Count height of the border (count vertical gray pixels above tag)
  int h = 1;
  while (pixels[tag_pos - pitch * (h + 1)] == COLOR_TAG_MARKER) {
    h++;
  }
  *out_box_height = h;

  if (h == 19) {
    // Single half-height box - determine if upper or lower
    if (pixels[tag_pos - pitch * 2 - 1] == COLOR_TAG_ZERO) {
      out_idx[0] = 0;
      out_pos[0] = tag_pos - 135 - pitch * 18;
      return 1;
    } else if (pixels[tag_pos - pitch * 18 - 1] == COLOR_TAG_ZERO) {
      out_idx[0] = 1;
      out_pos[0] = tag_pos - 135 - pitch * 17;
      return 1;
    } else {
      LogError("Cannot determine compact box position");
      return 0;
    }
  } else if (h == 35) {
    // Full height box with both halves
    out_idx[0] = 0;
    out_pos[0] = tag_pos - 135 - pitch * 34;
    out_idx[1] = 1;
    out_pos[1] = tag_pos - 135 - pitch * 17;
    return 2;
  }

  LogError("Unexpected box height: %d", h);
  return 0;
}

// Build palette lookup from header pixels
static void BuildPaletteLut(const uint32_t *pixels, int header_pos, bool is_high, PaletteLut *lut) {
  lut->count = 16;
  memset(lut->colors, 0, sizeof(lut->colors));

  // Transparent color is always at index 0
  lut->colors[0] = COLOR_TRANSPARENT;

  // Read 7 palette colors from header (5 pixels apart)
  int base_idx = is_high ? 9 : 1;
  for (int i = 0; i < 7; i++) {
    lut->colors[base_idx + i] = pixels[header_pos + 5 * i];
  }
}

// Copy an 8x8 tile from PNG to sprite sheet, converting colors
static bool CopyTileToSheet(SpriteSheet *sheet, int dst_x, int dst_y,
                            const uint32_t *pixels, int pitch, int src_pos,
                            PaletteLut *lut) {
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      uint32_t color = pixels[src_pos + y * pitch + x];
      int idx = FindColorInLut(lut, color);

      if (idx < 0) {
        LogError("Pixel color 0x%06X not found in palette", color);
        return false;
      }

      int dst_offset = (dst_y + y) * SPRITE_SHEET_WIDTH + (dst_x + x);
      sheet->pixels[dst_offset] = (uint8_t)idx;
    }
  }
  return true;
}

// ============================================================================
// Main Loading Functions
// ============================================================================

// Process a single PNG file and extract tiles to sheets
static bool ProcessPngFile(SpriteSheetLoader *loader, const char *filepath) {
  // Read PNG file
  size_t png_size;
  uint8_t *png_data = Platform_ReadWholeFile(filepath, &png_size);
  if (!png_data) {
    LogError("Failed to read PNG file: %s", filepath);
    return false;
  }

  // Decode PNG to RGB24
  unsigned char *rgb_data = NULL;
  unsigned width, height;
  unsigned error = lodepng_decode24(&rgb_data, &width, &height, png_data, png_size);
  free(png_data);

  if (error) {
    LogError("Failed to decode PNG %s: %s", filepath, lodepng_error_text(error));
    return false;
  }

  if (width != PNG_WIDTH) {
    LogError("PNG width mismatch: expected %d, got %u", PNG_WIDTH, width);
    free(rgb_data);
    return false;
  }

  // Convert RGB24 to packed uint32 array
  int pixel_count = width * height;
  uint32_t *pixels = malloc(pixel_count * sizeof(uint32_t));
  if (!pixels) {
    LogError("Failed to allocate pixel buffer");
    free(rgb_data);
    return false;
  }

  for (int i = 0; i < pixel_count; i++) {
    pixels[i] = PackRGB(rgb_data[i * 3], rgb_data[i * 3 + 1], rgb_data[i * 3 + 2]);
  }
  free(rgb_data);

  int pitch = width;
  PaletteLut current_lut = {0};
  int tag_pos = 0;

  // Process all tags in the PNG
  while ((tag_pos = FindNextTag(pixels, pixel_count, pitch, tag_pos)) >= 0) {
    int pal_base, tileset, pal_subidx;
    bool headerless;

    if (!DecodeTag(pixels, tag_pos, &pal_base, &tileset, &headerless, &pal_subidx)) {
      LogWarn("Failed to decode tag at position %d", tag_pos);
      continue;
    }

    // Validate tileset index
    if (tileset >= SPRITE_SHEET_COUNT) {
      LogWarn("Tileset %d out of range (max %d)", tileset, SPRITE_SHEET_COUNT - 1);
      continue;
    }

    // Determine image rectangles
    int rect_idx[2], rect_pos[2];
    int box_height;
    int rect_count = DetermineImageRects(pixels, tag_pos, pitch, rect_idx, rect_pos, &box_height);

    if (rect_count == 0) {
      continue;
    }

    // Build palette lookup if this is a header entry (not headerless)
    if (!headerless) {
      bool is_high = SpriteLoader_IsHighPaletteTileset(tileset);
      int header_pos = tag_pos - pitch * (box_height + 3) - 34;
      BuildPaletteLut(pixels, header_pos, is_high, &current_lut);
    }

    // Extract tiles from each rectangle
    SpriteSheet *sheet = &loader->sheets[tileset];

    for (int r = 0; r < rect_count; r++) {
      int idx = rect_idx[r];
      int sheet_pos = rect_pos[r];

      // Each rectangle contains 2 rows of 16 tiles
      for (int ty = 0; ty < 2; ty++) {
        for (int tx = 0; tx < 16; tx++) {
          // Source position in PNG (accounting for 1-pixel gaps between tiles)
          int src_pos = sheet_pos + (ty * 8 + (ty >> 1)) * pitch + (tx * 8 + (tx >> 1));

          // Skip empty tiles
          if (IsEmptyTile(pixels, pitch, src_pos)) {
            continue;
          }

          // Destination position in sheet
          int dst_x = tx * 8;
          int dst_y = (ty + idx * 2) * 8;

          if (!CopyTileToSheet(sheet, dst_x, dst_y, pixels, pitch, src_pos, &current_lut)) {
            LogWarn("Failed to copy tile at sheet %d pos (%d,%d)", tileset, dst_x, dst_y);
          }

          sheet->loaded = true;
        }
      }
    }
  }

  free(pixels);
  return true;
}

SpriteSheetLoader* SpriteLoader_Load(const char *sprites_dir) {
  SpriteSheetLoader *loader = calloc(1, sizeof(SpriteSheetLoader));
  if (!loader) {
    LogError("Failed to allocate sprite loader");
    return NULL;
  }

  // Initialize all sheets to unloaded state with transparent pixels
  for (int i = 0; i < SPRITE_SHEET_COUNT; i++) {
    memset(loader->sheets[i].pixels, 0, SPRITE_SHEET_PIXELS);
    loader->sheets[i].loaded = false;
  }

  // Process each PNG file
  char filepath[512];
  int loaded_count = 0;

  for (int i = 0; i < kPngFilenameCount; i++) {
    snprintf(filepath, sizeof(filepath), "%s/%s", sprites_dir, kPngFilenames[i]);

    if (ProcessPngFile(loader, filepath)) {
      loaded_count++;
    } else {
      LogWarn("Failed to process %s", kPngFilenames[i]);
    }
  }

  if (loaded_count == 0) {
    LogError("No sprite PNG files could be loaded from %s", sprites_dir);
    SpriteLoader_Free(loader);
    return NULL;
  }

  // Count loaded sheets
  int sheets_loaded = 0;
  for (int i = 0; i < SPRITE_SHEET_COUNT; i++) {
    if (loader->sheets[i].loaded)
      sheets_loaded++;
  }

  printf("    Loaded %d sprite sheets from %d PNG files\n", sheets_loaded, loaded_count);

  return loader;
}

void SpriteLoader_Free(SpriteSheetLoader *loader) {
  if (loader) {
    free(loader);
  }
}

// ============================================================================
// SNES Encoding
// ============================================================================

uint8_t* SpriteLoader_EncodeSheet(SpriteSheetLoader *loader, int sheet_index, uint32_t *out_size) {
  if (!loader || sheet_index < 0 || sheet_index >= SPRITE_SHEET_COUNT) {
    return NULL;
  }

  SpriteSheet *sheet = &loader->sheets[sheet_index];

  // Allocate output buffer (24 bytes per tile * 64 tiles = 1536 bytes)
  uint8_t *result = calloc(SNES_SHEET_SIZE, 1);
  if (!result) {
    LogError("Failed to allocate SNES encoding buffer");
    return NULL;
  }

  // Encode each 8x8 tile to SNES 3bpp planar format
  for (int ty = 0; ty < 4; ty++) {
    for (int tx = 0; tx < 16; tx++) {
      int tile_idx = ty * 16 + tx;
      int dst_pos = tile_idx * BYTES_PER_3BPP_TILE;
      int src_x = tx * 8;
      int src_y = ty * 8;

      // Encode 8x8 tile: 3 bitplanes
      // Planes 0,1: bytes 0-15 (2 bytes per row, interleaved)
      // Plane 2: bytes 16-23 (1 byte per row)
      for (int y = 0; y < 8; y++) {
        uint8_t plane0 = 0, plane1 = 0, plane2 = 0;

        for (int x = 0; x < 8; x++) {
          int src_offset = (src_y + y) * SPRITE_SHEET_WIDTH + src_x + (7 - x);
          uint8_t pixel = sheet->pixels[src_offset];

          plane0 |= (pixel & 1) << x;
          plane1 |= ((pixel >> 1) & 1) << x;
          plane2 |= ((pixel >> 2) & 1) << x;
        }

        result[dst_pos + y * 2 + 0] = plane0;
        result[dst_pos + y * 2 + 1] = plane1;
        result[dst_pos + y + 16] = plane2;
      }
    }
  }

  *out_size = SNES_SHEET_SIZE;
  return result;
}
