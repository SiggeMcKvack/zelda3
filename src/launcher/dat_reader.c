// dat_reader.c - Lightweight DAT file reader for launcher
// Reads language info without loading full game assets

#include "dat_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// DAT file constants (must match src/assets.h)
#define DAT_NUMBER_OF_ASSETS 165
#define DAT_DIALOGUE_MAP_ASSET 96

// DAT file signature (first 48 bytes)
static const uint8_t kDatSignature[] = {
    90, 101, 108, 100, 97, 51, 95, 118, 48, 32, 32, 32, 32, 32, 10, 0,
    27, 174, 233, 45, 74, 174, 252, 50, 49, 27, 153, 197, 27, 43, 216, 197,
    132, 101, 173, 169, 36, 108, 15, 155, 176, 169, 57, 131, 174, 101, 51, 207
};

bool DatReader_Exists(const char *dir) {
    char path[512];
    snprintf(path, sizeof(path), "%s/zelda3_assets.dat", dir);

    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

// Find indexed element within a packed array (replicates FindIndexInMemblk from assets.c)
//
// Binary format for packed arrays:
// - If count < 8192: [uint16 offsets * count] [data0] [data1] ... [uint16 count at end]
// - If count >= 8192: [uint32 offsets * count] [data0] [data1] ... [uint16 (count + 8192) at end]
//
// The offsets are relative to the end of the offset table. The count at the end
// uses 8192 as a flag to indicate uint32 offsets instead of uint16.
//
// Returns pointer to element and sets *out_size, or returns NULL if invalid.
static const uint8_t* find_index_in_memblk(const uint8_t *data, size_t data_size,
                                           size_t idx, size_t *out_size) {
    if (data_size < 2) return NULL;

    size_t end = data_size - 2;
    uint16_t mx = *(uint16_t*)(data + end);

    size_t left_off, right_off;

    if (mx < 8192) {
        // uint16 offsets
        if (idx > mx || mx * 2 > end) return NULL;
        left_off = (idx == 0) ? mx * 2 : mx * 2 + *(uint16_t*)(data + idx * 2 - 2);
        right_off = (idx == mx) ? end : mx * 2 + *(uint16_t*)(data + idx * 2);
    } else {
        // uint32 offsets
        mx -= 8192;
        if (idx > mx || mx * 4 > end) return NULL;
        left_off = (idx == 0) ? mx * 4 : mx * 4 + *(uint32_t*)(data + idx * 4 - 4);
        right_off = (idx == mx) ? end : mx * 4 + *(uint32_t*)(data + idx * 4);
    }

    if (right_off <= left_off || right_off > data_size) return NULL;

    *out_size = right_off - left_off;
    return data + left_off;
}

int DatReader_GetLanguages(const char *dir, char languages[][16], int max_languages) {
    char path[512];
    snprintf(path, sizeof(path), "%s/zelda3_assets.dat", dir);

    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Check minimum size
    size_t min_size = 88 + DAT_NUMBER_OF_ASSETS * 4;
    if (file_size < (long)min_size) {
        fclose(f);
        return 0;
    }

    // Read header (88 bytes + asset sizes array)
    uint8_t header[88];
    if (fread(header, 1, 88, f) != 88) {
        fclose(f);
        return 0;
    }

    // Validate signature
    if (memcmp(header, kDatSignature, 48) != 0) {
        fclose(f);
        return 0;
    }

    // Check asset count
    uint32_t num_assets = *(uint32_t*)(header + 80);
    if (num_assets != DAT_NUMBER_OF_ASSETS) {
        fclose(f);
        return 0;
    }

    uint32_t key_sig_size = *(uint32_t*)(header + 84);

    // Read asset sizes
    uint32_t *sizes = malloc(num_assets * 4);
    if (!sizes || fread(sizes, 4, num_assets, f) != num_assets) {
        free(sizes);
        fclose(f);
        return 0;
    }

    // Calculate offset to kDialogueMap asset (index 96)
    // Offset starts after: header(88) + sizes(165*4) + key_signature
    uint32_t data_offset = 88 + num_assets * 4 + key_sig_size;

    // Skip to asset 96 by summing sizes of assets 0-95
    for (int i = 0; i < DAT_DIALOGUE_MAP_ASSET; i++) {
        data_offset = (data_offset + 3) & ~3;  // 4-byte align
        // Overflow check: ensure adding size won't wrap around
        if (data_offset > UINT32_MAX - sizes[i]) {
            free(sizes);
            fclose(f);
            return 0;
        }
        data_offset += sizes[i];
    }
    data_offset = (data_offset + 3) & ~3;  // Align for asset 96

    uint32_t dialogue_map_size = sizes[DAT_DIALOGUE_MAP_ASSET];
    free(sizes);

    if (dialogue_map_size == 0 || data_offset + dialogue_map_size > (size_t)file_size) {
        fclose(f);
        return 0;
    }

    // Read kDialogueMap asset
    uint8_t *dialogue_map = malloc(dialogue_map_size);
    if (!dialogue_map) {
        fclose(f);
        return 0;
    }

    fseek(f, data_offset, SEEK_SET);
    if (fread(dialogue_map, 1, dialogue_map_size, f) != dialogue_map_size) {
        free(dialogue_map);
        fclose(f);
        return 0;
    }
    fclose(f);

    // Parse language entries from kDialogueMap
    // Each entry is a packed array, first element (index 0) is the language code
    int count = 0;
    for (int i = 0; i < max_languages; i++) {
        size_t entry_size;
        const uint8_t *entry = find_index_in_memblk(dialogue_map, dialogue_map_size, i, &entry_size);
        if (!entry) break;

        // Get first element (language code) from this entry
        size_t name_size;
        const uint8_t *name = find_index_in_memblk(entry, entry_size, 0, &name_size);
        if (!name || name_size == 0 || name_size >= 16) continue;

        // Bounds check before writing to output array
        if (count >= max_languages) break;
        memcpy(languages[count], name, name_size);
        languages[count][name_size] = '\0';
        count++;
    }

    free(dialogue_map);
    return count;
}
