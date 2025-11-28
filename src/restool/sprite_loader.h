// sprite_loader.h - Load sprite graphics from PNG files
// Implements --sprites-from-png functionality matching Python restool
#ifndef RESTOOL_SPRITE_LOADER_H
#define RESTOOL_SPRITE_LOADER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Number of editable sprite tilesets (from PNG)
#define SPRITE_SHEET_COUNT 103

// Each sprite sheet is 128x32 pixels (16x4 tiles of 8x8)
#define SPRITE_SHEET_WIDTH 128
#define SPRITE_SHEET_HEIGHT 32
#define SPRITE_SHEET_PIXELS (SPRITE_SHEET_WIDTH * SPRITE_SHEET_HEIGHT)

// SNES 3bpp format: 24 bytes per 8x8 tile, 64 tiles per sheet
#define BYTES_PER_3BPP_TILE 24
#define TILES_PER_SHEET 64
#define SNES_SHEET_SIZE (BYTES_PER_3BPP_TILE * TILES_PER_SHEET)

// Single sprite sheet (128x32 indexed pixels)
typedef struct {
  uint8_t pixels[SPRITE_SHEET_PIXELS];  // Indexed color (0-15)
  bool loaded;
} SpriteSheet;

// Container for all loaded sprite sheets
typedef struct {
  SpriteSheet sheets[SPRITE_SHEET_COUNT];
} SpriteSheetLoader;

// Load all sprite sheets from PNG files in the given directory
// Returns NULL on failure
// Caller must free with SpriteLoader_Free()
SpriteSheetLoader* SpriteLoader_Load(const char *sprites_dir);

// Free sprite loader and all loaded data
void SpriteLoader_Free(SpriteSheetLoader *loader);

// Encode a single sprite sheet to SNES 3bpp format
// Returns allocated buffer of SNES_SHEET_SIZE bytes, or NULL on error
// Caller must free the returned buffer
uint8_t* SpriteLoader_EncodeSheet(SpriteSheetLoader *loader, int sheet_index, uint32_t *out_size);

// Check if a tileset uses high palette (indices 9-15 vs 1-7)
bool SpriteLoader_IsHighPaletteTileset(int tileset);

#endif // RESTOOL_SPRITE_LOADER_H
