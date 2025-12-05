// asset_reader.c - Filesystem asset access

#include "asset_reader.h"
#include "../platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

uint8_t* AssetReader_Load(const char *path, size_t *out_size) {
    if (!path) return NULL;

    // Try path as-is
    uint8_t *data = Platform_ReadWholeFile(path, out_size);
    if (data) return data;

    // Try with "assets/" prefix if not already present
    if (strncmp(path, "assets/", 7) != 0) {
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "assets/%s", path);
        data = Platform_ReadWholeFile(full_path, out_size);
        if (data) return data;
    }

    // Try stripping "assets/" prefix if present (for files written directly to output dir)
    if (strncmp(path, "assets/", 7) == 0) {
        const char *stripped = path + 7;
        data = Platform_ReadWholeFile(stripped, out_size);
        if (data) return data;
    }

    return NULL;
}

void AssetReader_Free(uint8_t *data) {
    free(data);
}

bool AssetReader_Exists(const char *path) {
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

    // Try stripping "assets/" prefix if present
    if (strncmp(path, "assets/", 7) == 0) {
        const char *stripped = path + 7;
        f = fopen(stripped, "rb");
        if (f) {
            fclose(f);
            return true;
        }
    }

    return false;
}
