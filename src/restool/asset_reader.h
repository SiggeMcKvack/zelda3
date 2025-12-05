// asset_reader.h - Filesystem asset access
#ifndef ASSET_READER_H
#define ASSET_READER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Load asset data by path (e.g., "assets/dungeon/dungeon-0.yaml" or "dungeon/dungeon-0.yaml")
// Returns allocated buffer that caller must free with AssetReader_Free()
// Returns NULL on error
uint8_t* AssetReader_Load(const char *path, size_t *out_size);

// Free data returned by AssetReader_Load
// Safe to call with NULL
void AssetReader_Free(uint8_t *data);

// Check if an asset exists on filesystem
bool AssetReader_Exists(const char *path);

#endif // ASSET_READER_H
