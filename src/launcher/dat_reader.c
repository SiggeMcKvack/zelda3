// dat_reader.c - Lightweight DAT file reader for launcher
// Reads language info without loading full game assets

#include "dat_reader.h"
#include "../logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// DAT file asset name we're looking for
#define DAT_DIALOGUE_MAP_NAME "kDialogueMap"

// DAT file signature (first 16 bytes - "Zelda3_v0     \n\0")
// Note: Bytes 16-47 contain a hash that varies per DAT file, so only validate the text portion
static const uint8_t kDatSignature[] = {
    90, 101, 108, 100, 97, 51, 95, 118, 48, 32, 32, 32, 32, 32, 10, 0
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
    LogInfo("DatReader: Checking path: %s", path);

    FILE *f = fopen(path, "rb");
    if (!f) {
        LogWarn("DatReader: Cannot open file: %s", path);
        return 0;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    LogInfo("DatReader: File size: %ld bytes", file_size);

    // Check minimum size for header
    if (file_size < 88) {
        LogWarn("DatReader: File too small (%ld < 88)", file_size);
        fclose(f);
        return 0;
    }

    // Read header (88 bytes)
    uint8_t header[88];
    if (fread(header, 1, 88, f) != 88) {
        LogWarn("DatReader: Failed to read header");
        fclose(f);
        return 0;
    }

    // Validate signature (only first 16 bytes - the text portion)
    if (memcmp(header, kDatSignature, sizeof(kDatSignature)) != 0) {
        LogWarn("DatReader: Invalid signature. Got: %.16s", header);
        fclose(f);
        return 0;
    }
    LogInfo("DatReader: Signature valid");

    uint32_t num_assets = *(uint32_t*)(header + 80);
    uint32_t key_sig_size = *(uint32_t*)(header + 84);
    LogInfo("DatReader: num_assets=%u, key_sig_size=%u", num_assets, key_sig_size);

    if (num_assets == 0 || num_assets > 1000) {
        LogWarn("DatReader: Invalid asset count: %u", num_assets);
        fclose(f);
        return 0;
    }

    // Read asset sizes
    uint32_t *sizes = malloc(num_assets * 4);
    if (!sizes || fread(sizes, 4, num_assets, f) != num_assets) {
        LogWarn("DatReader: Failed to read asset sizes");
        free(sizes);
        fclose(f);
        return 0;
    }

    // Read key signature (asset names, null-separated)
    char *key_sig = malloc(key_sig_size + 1);
    if (!key_sig || fread(key_sig, 1, key_sig_size, f) != key_sig_size) {
        LogWarn("DatReader: Failed to read key signature");
        free(sizes);
        free(key_sig);
        fclose(f);
        return 0;
    }
    key_sig[key_sig_size] = '\0';

    // Find kDialogueMap asset by name
    int dialogue_map_index = -1;
    const char *name_ptr = key_sig;
    for (uint32_t i = 0; i < num_assets && name_ptr < key_sig + key_sig_size; i++) {
        if (strcmp(name_ptr, DAT_DIALOGUE_MAP_NAME) == 0) {
            dialogue_map_index = (int)i;
            LogInfo("DatReader: Found %s at index %d", DAT_DIALOGUE_MAP_NAME, i);
            break;
        }
        name_ptr += strlen(name_ptr) + 1;
    }
    free(key_sig);

    if (dialogue_map_index < 0) {
        LogWarn("DatReader: %s asset not found in DAT file", DAT_DIALOGUE_MAP_NAME);
        free(sizes);
        fclose(f);
        return 0;
    }

    // Calculate offset to kDialogueMap asset
    // Data starts after: header(88) + sizes(num_assets*4) + key_signature
    uint32_t data_offset = 88 + num_assets * 4 + key_sig_size;

    // Skip to the dialogue map asset
    for (int i = 0; i < dialogue_map_index; i++) {
        data_offset = (data_offset + 3) & ~3;  // 4-byte align
        if (data_offset > UINT32_MAX - sizes[i]) {
            LogWarn("DatReader: Overflow at asset %d", i);
            free(sizes);
            fclose(f);
            return 0;
        }
        data_offset += sizes[i];
    }
    data_offset = (data_offset + 3) & ~3;  // Align for target asset

    uint32_t dialogue_map_size = sizes[dialogue_map_index];
    free(sizes);

    LogInfo("DatReader: dialogue_map offset=%u size=%u", data_offset, dialogue_map_size);

    if (dialogue_map_size == 0 || data_offset + dialogue_map_size > (size_t)file_size) {
        LogWarn("DatReader: Invalid dialogue_map bounds (offset=%u size=%u file=%ld)",
                data_offset, dialogue_map_size, file_size);
        fclose(f);
        return 0;
    }

    // Read kDialogueMap asset
    uint8_t *dialogue_map = malloc(dialogue_map_size);
    if (!dialogue_map) {
        LogWarn("DatReader: Failed to allocate dialogue_map buffer");
        fclose(f);
        return 0;
    }

    fseek(f, data_offset, SEEK_SET);
    if (fread(dialogue_map, 1, dialogue_map_size, f) != dialogue_map_size) {
        LogWarn("DatReader: Failed to read dialogue_map data");
        free(dialogue_map);
        fclose(f);
        return 0;
    }
    fclose(f);

    // Log first 32 bytes of dialogue_map for debugging
    char hex[97] = {0};
    for (int i = 0; i < 32 && i < (int)dialogue_map_size; i++) {
        snprintf(hex + i*3, 4, "%02x ", dialogue_map[i]);
    }
    LogInfo("DatReader: dialogue_map first bytes: %s", hex);

    // Parse language entries from kDialogueMap
    // Each entry is a packed array, first element (index 0) is the language code
    int count = 0;
    for (int i = 0; i < max_languages; i++) {
        size_t entry_size;
        const uint8_t *entry = find_index_in_memblk(dialogue_map, dialogue_map_size, i, &entry_size);
        if (!entry) {
            LogInfo("DatReader: Entry %d: NULL (end of list)", i);
            break;
        }
        LogInfo("DatReader: Entry %d: size=%zu", i, entry_size);

        // Get first element (language code) from this entry
        size_t name_size;
        const uint8_t *name = find_index_in_memblk(entry, entry_size, 0, &name_size);
        if (!name || name_size == 0 || name_size >= 16) {
            LogInfo("DatReader: Entry %d: invalid name (name=%p size=%zu)", i, (void*)name, name_size);
            continue;
        }

        // Bounds check before writing to output array
        if (count >= max_languages) break;
        memcpy(languages[count], name, name_size);
        languages[count][name_size] = '\0';
        LogInfo("DatReader: Found language: '%s'", languages[count]);
        count++;
    }

    free(dialogue_map);
    LogInfo("DatReader: Returning %d languages", count);
    return count;
}
