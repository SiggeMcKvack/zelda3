// graphics.h - SNES graphics decoding and conversion
#ifndef RESTOOL_GRAPHICS_H
#define RESTOOL_GRAPHICS_H

#include "types.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ============================================================================
// SNES Tile Decoding
// ============================================================================

// Decode SNES 2bpp planar tile data
// Input: 16 bytes per 8x8 tile (2 bitplanes)
// Output: 64 bytes of indexed color data (8x8 pixels, 0-3 per pixel)
void DecodeTile2bpp(const uint8_t *src, uint8_t *dst);

// Decode SNES 3bpp planar tile data
// Input: 24 bytes per 8x8 tile (3 bitplanes)
// Output: 64 bytes of indexed color data (8x8 pixels, 0-7 per pixel)
void DecodeTile3bpp(const uint8_t *src, uint8_t *dst);

// Decode SNES 4bpp planar tile data
// Input: 32 bytes per 8x8 tile (4 bitplanes)
// Output: 64 bytes of indexed color data (8x8 pixels, 0-15 per pixel)
void DecodeTile4bpp(const uint8_t *src, uint8_t *dst);

// Decode multiple tiles into a linear bitmap
// tile_count: Number of 8x8 tiles to decode
// tiles_per_row: Tiles per row (for layout, e.g., 16 for 128-pixel width)
// Returns allocated TileData (caller must free)
TileData* DecodeTileset2bpp(const uint8_t *src, int tile_count, int tiles_per_row);
TileData* DecodeTileset3bpp(const uint8_t *src, int tile_count, int tiles_per_row);
TileData* DecodeTileset4bpp(const uint8_t *src, int tile_count, int tiles_per_row);

// Free tile data
void FreeTileData(TileData *tile_data);

// ============================================================================
// Palette Conversion
// ============================================================================

// Convert SNES 15-bit BGR555 color to 32-bit RGBA
// SNES format: xBBBBBGGGGGRRRRR (15-bit, little-endian)
// Returns RGBA color structure
Color SnesColorToRGBA(uint16_t snes_color);

// Convert array of SNES colors to RGBA palette
// count: Number of colors (typically 16 for 4bpp, 8 for 3bpp, 4 for 2bpp)
// out_palette: Output array (must have space for count colors)
void SnesPaletteToRGBA(const uint16_t *snes_palette, int count, Color *out_palette);

// ============================================================================
// PNG Output
// ============================================================================

// Write indexed color image to PNG file with palette
// Uses stb_image_write.h
// Returns true on success
bool WritePNG_Indexed(const char *filename, int width, int height,
                      const uint8_t *pixels, const Color *palette, int palette_size);

// Write 32-bit RGBA image to PNG file
bool WritePNG_RGBA(const char *filename, int width, int height, const uint8_t *rgba_pixels);

#endif // RESTOOL_GRAPHICS_H
