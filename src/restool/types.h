// types.h - Common types and structures for restool
#ifndef RESTOOL_TYPES_H
#define RESTOOL_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ROM language identifiers (matching Python's util.py)
typedef enum {
  ROM_LANG_UNKNOWN = 0,
  ROM_LANG_US,      // USA
  ROM_LANG_DE,      // Germany
  ROM_LANG_FR,      // France
  ROM_LANG_FR_C,    // Canada (French)
  ROM_LANG_EN,      // Europe (English)
  ROM_LANG_ES,      // Spanish translation
  ROM_LANG_PL,      // Polish translation
  ROM_LANG_PT,      // Portuguese translation
  ROM_LANG_REDUX,   // English Redux
  ROM_LANG_NL,      // Dutch translation
  ROM_LANG_SV,      // Swedish translation
} RomLanguage;

// ROM structure
typedef struct {
  uint8_t *data;
  size_t size;
  bool has_smc_header;
  char sha1[41];       // Hex string (40 chars + null terminator)
  RomLanguage language;
  const char *language_name;  // Human-readable name
} Rom;

// Decompressed data from SNES compression
typedef struct {
  uint8_t *data;
  size_t size;
  size_t capacity;
  size_t compressed_size;  // Number of compressed bytes read (for Python compatibility)
} DecompressedData;

// Compressed data for SNES compression
typedef struct {
  uint8_t *data;
  size_t size;
  size_t capacity;
} CompressedData;

// Tile data (decoded SNES graphics)
typedef struct {
  uint8_t *pixels;  // Linear bitmap (indexed color)
  int width;        // In pixels
  int height;       // In pixels
  uint8_t bpp;      // Bits per pixel (2, 3, or 4)
} TileData;

// Color structure for palettes
typedef struct {
  uint8_t r, g, b, a;
} Color;

// Known ROM SHA1 hashes (from Python tool)
#define ROM_SHA1_USA    "6d4f10a8b10e10dbe624cb23cf03b88bb8252973"  // USA
#define ROM_SHA1_DE     "2e62494967fb0afdf5da1635607f9641df7c6559"  // Germany
#define ROM_SHA1_FR     "229364a1b92a05167cd38609b1aa98f7041987cc"  // France
#define ROM_SHA1_FR_C   "c1c6c7f76fff936c534ff11f87a54162fc0aa100"  // Canada (French)
#define ROM_SHA1_EN     "7c073a222569b9b8e8ca5fcb5dfec3b5e31da895"  // Europe (English)
#define ROM_SHA1_ES     "461fcbd700d1332009c0e85a7a136e2a8e4b111e"  // Spanish translation
#define ROM_SHA1_PL     "3c4d605eefda1d76f101965138f238476655b11d"  // Polish translation
#define ROM_SHA1_PT     "d0d09ed41f9c373fe6afdccafbf0da8c88d3d90d"  // Portuguese translation
#define ROM_SHA1_REDUX1 "b2a07a59e64c498bc1b2f28728f9bf4014c8d582"  // English Redux v1
#define ROM_SHA1_REDUX2 "9325c22eb0a2a1f0017157c8b620bc3a605cede1"  // English Redux v2
#define ROM_SHA1_NL     "fa8adfdba2697c9a54d583a1284a22ac764c7637"  // Dutch translation
#define ROM_SHA1_SV     "43cd3438469b2c3fe879ea2f410b3ef3cb3f1ca4"  // Swedish translation

#endif // RESTOOL_TYPES_H
