// asset_reader.h - Unified asset access (embedded or filesystem)
#ifndef ASSET_READER_H
#define ASSET_READER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Load asset data by path (e.g., "assets/dungeon/dungeon-0.yaml" or "dungeon/dungeon-0.yaml")
// Tries embedded assets first, falls back to filesystem
// Returns allocated buffer that caller must free with AssetReader_Free()
// Returns NULL on error
uint8_t* AssetReader_Load(const char *path, size_t *out_size);

// Free data returned by AssetReader_Load
// Safe to call with NULL
void AssetReader_Free(uint8_t *data);

// Check if an asset exists (embedded or filesystem)
bool AssetReader_Exists(const char *path);

// Get asset data without copying (embedded only, returns NULL for filesystem)
// Useful for read-only access to avoid allocation
// Do NOT free the returned pointer
const uint8_t* AssetReader_GetEmbedded(const char *path, size_t *out_size);

#endif // ASSET_READER_H
