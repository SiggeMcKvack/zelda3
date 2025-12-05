// Empty stub - embedded assets no longer used
// All assets are extracted directly from ROM at runtime

#ifndef EMBEDDED_ASSETS_H
#define EMBEDDED_ASSETS_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    const char *name;
    const uint8_t *data;
    size_t size;
} EmbeddedAsset;

// Get embedded asset by path - always returns NULL (no embedded assets)
const EmbeddedAsset* EmbeddedAssets_Find(const char *path);

// Get asset data directly - always returns NULL (no embedded assets)
const uint8_t* EmbeddedAssets_GetData(const char *path, size_t *out_size);

#define EMBEDDED_ASSETS_COUNT 0

#endif // EMBEDDED_ASSETS_H
