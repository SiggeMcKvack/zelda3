// Empty stub - embedded assets no longer used
// All assets are extracted directly from ROM at runtime

#include "embedded_assets.h"
#include <stddef.h>
#include <string.h>

// Empty lookup - always returns NULL
const EmbeddedAsset* EmbeddedAssets_Find(const char *path) {
    (void)path;
    return NULL;
}

// Empty lookup - always returns NULL
const uint8_t* EmbeddedAssets_GetData(const char *path, size_t *out_size) {
    (void)path;
    if (out_size) *out_size = 0;
    return NULL;
}
