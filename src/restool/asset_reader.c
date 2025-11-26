// asset_reader.c - Unified asset access (embedded or filesystem)

#include "asset_reader.h"
#include "embedded_assets.h"
#include "../platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Normalize path by stripping "assets/" prefix if present
static const char* normalize_path(const char *path) {
    if (!path) return NULL;
    if (strncmp(path, "assets/", 7) == 0) return path + 7;
    return path;
}

const uint8_t* AssetReader_GetEmbedded(const char *path, size_t *out_size) {
    const char *norm = normalize_path(path);
    if (!norm) return NULL;
    return EmbeddedAssets_GetData(norm, out_size);
}

uint8_t* AssetReader_Load(const char *path, size_t *out_size) {
    if (!path) return NULL;

    // Try embedded assets first
    size_t size;
    const uint8_t *embedded = AssetReader_GetEmbedded(path, &size);
    if (embedded) {
        // Copy to new buffer (caller expects to free it)
        uint8_t *copy = malloc(size);
        if (copy) {
            memcpy(copy, embedded, size);
            if (out_size) *out_size = size;
            return copy;
        }
    }

    // Fall back to filesystem
    uint8_t *data = Platform_ReadWholeFile(path, out_size);
    if (data) return data;

    // Try with "assets/" prefix if not already present
    if (strncmp(path, "assets/", 7) != 0) {
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "assets/%s", path);
        data = Platform_ReadWholeFile(full_path, out_size);
        if (data) return data;
    }

    return NULL;
}

void AssetReader_Free(uint8_t *data) {
    free(data);
}

bool AssetReader_Exists(const char *path) {
    // Check embedded
    const char *norm = normalize_path(path);
    if (norm && EmbeddedAssets_Find(norm)) {
        return true;
    }

    // Check filesystem
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return true;
    }

    // Try with "assets/" prefix
    if (strncmp(path, "assets/", 7) != 0) {
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "assets/%s", path);
        f = fopen(full_path, "rb");
        if (f) {
            fclose(f);
            return true;
        }
    }

    return false;
}
