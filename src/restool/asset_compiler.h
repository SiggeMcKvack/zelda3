// asset_compiler.h - Zelda3 asset compilation to binary format
// Compiles extracted assets into zelda3_assets.dat

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Asset types
typedef enum {
  ASSET_TYPE_UINT8,
  ASSET_TYPE_UINT16,
  ASSET_TYPE_INT8,
  ASSET_TYPE_INT16,
  ASSET_TYPE_PACKED,
} AssetType;

// Single asset entry
typedef struct {
  const char *name;       // Asset name (e.g., "kLinkGraphics")
  AssetType type;         // Data type
  uint8_t *data;          // Asset data
  uint32_t size;          // Data size in bytes
} Asset;

// Asset compiler/builder
typedef struct {
  Asset *assets;          // Dynamic array of assets
  uint32_t asset_count;   // Number of assets
  uint32_t asset_capacity; // Capacity of assets array
} AssetBuilder;

// Initialize asset builder
AssetBuilder* AssetBuilder_Create(void);

// Add asset to builder
void AssetBuilder_AddAsset(AssetBuilder *builder, const char *name, AssetType type,
                           const uint8_t *data, uint32_t size);

// Pack arrays into Python-compatible packed format
// Returns allocated buffer (caller must free), sets out_size
// Format: [offsets (uint16/32)...] [data0] [data1] ... [count (uint16)]
uint8_t* AssetBuilder_PackArrays(uint8_t **arrays, uint32_t *sizes, uint32_t count, uint32_t *out_size);

// Write assets to zelda3_assets.dat
bool AssetBuilder_WriteToFile(AssetBuilder *builder, const char *filepath);

// Free asset builder
void AssetBuilder_Free(AssetBuilder *builder);
