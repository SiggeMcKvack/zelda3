// restool_lib.c - Library API implementation for restool functions
// Used by Android app and other embedding scenarios

#include "restool_lib.h"
#include "restool_util.h"
#include "asset_compiler.h"
#include "extract.h"
#include "text_decode.h"
#include "music_compiler.h"
#include "../logging.h"
#include "../rom_sha1.h"
#include "../platform.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Global dialogue directory override (NULL = use default assets/)
static char *g_dialogue_dir = NULL;

void Restool_SetDialogueDir(const char *dir) {
    free(g_dialogue_dir);
    g_dialogue_dir = dir ? strdup(dir) : NULL;
}

const char *Restool_GetDialogueDir(void) {
    return g_dialogue_dir;
}

bool Restool_IdentifyRom(const char *rom_path, RestoolRomInfo *out_info) {
    if (!out_info) return false;

    memset(out_info, 0, sizeof(*out_info));

    RomIdentification id;
    if (!RomSha1_ValidateFile(rom_path, &id)) {
        return false;
    }

    strncpy(out_info->lang_code, id.lang_code, sizeof(out_info->lang_code) - 1);
    strncpy(out_info->lang_name, id.lang_name, sizeof(out_info->lang_name) - 1);
    out_info->valid = id.valid;
    return true;
}

int Restool_ExtractDialogue(const char *rom_path, const char *output_dir) {
    // Load ROM
    Rom *rom = Rom_Load(rom_path);
    if (!rom) {
        return RESTOOL_ERR_ROM_LOAD;
    }

    // Get language code from ROM
    const char *lang_code = TextDecode_GetLanguageCode(rom->language);
    if (!lang_code) {
        LogError("Unknown ROM language");
        Rom_Free(rom);
        return RESTOOL_ERR_ROM_INVALID;
    }

    LogInfo("Identified ROM as: %s - \"%s\"", lang_code, rom->language_name);

    // Check if language is supported for text decoding
    const LanguageConfig *config = TextDecode_GetLanguageConfig(lang_code);
    if (!config) {
        LogError("Language '%s' not yet supported for dialogue extraction", lang_code);
        Rom_Free(rom);
        return RESTOOL_ERR_ROM_INVALID;
    }

    // Decode dialogue strings
    LogInfo("Extracting dialogue strings...");
    DecodedStringsArray *strings = TextDecode_DecodeStrings(rom, lang_code);
    if (!strings) {
        LogError("Failed to decode dialogue strings");
        Rom_Free(rom);
        return RESTOOL_ERR_DIALOGUE;
    }

    LogInfo("Decoded %zu dialogue strings", strings->count);

    // Write to file
    if (!TextDecode_WriteDialogueFile(strings, lang_code, output_dir)) {
        LogError("Failed to write dialogue file");
        TextDecode_FreeStrings(strings);
        Rom_Free(rom);
        return RESTOOL_ERR_WRITE;
    }

    TextDecode_FreeStrings(strings);
    Rom_Free(rom);
    return RESTOOL_OK;
}

int Restool_CompileAssetsEx(const RestoolCompileOptions *options) {
    if (!options || !options->us_rom_path || !options->output_path) {
        LogError("Invalid options");
        return RESTOOL_ERR_ROM_LOAD;
    }

    // Set dialogue directory if specified
    if (options->dialogue_dir) {
        Restool_SetDialogueDir(options->dialogue_dir);
    }

    // Load US ROM
    Rom *rom = Rom_Load(options->us_rom_path);
    if (!rom) {
        return RESTOOL_ERR_ROM_LOAD;
    }

    // Verify it's the US ROM
    if (rom->language != ROM_LANG_US) {
        LogError("ROM is not US version (detected: %s)", rom->language_name ? rom->language_name : "unknown");
        Rom_Free(rom);
        return RESTOOL_ERR_ROM_NOT_US;
    }

    LogInfo("Identified ROM as: %s - \"%s\"", Rom_GetLanguageCode(rom->language), rom->language_name);

    // Create asset builder
    AssetBuilder *builder = AssetBuilder_Create();
    if (!builder) {
        LogError("Failed to create asset builder");
        Rom_Free(rom);
        return RESTOOL_ERR_MEMORY;
    }

    LogInfo("Compiling assets...");

    // ========================================================================
    // EXTRACTION ORDER - DO NOT REORDER!
    // The key signature hash depends on asset order.
    // ========================================================================

    // 1. print_sound_banks() - Music data (3 assets)
    if (!ExtractSoundBanks(builder)) {
        LogError("Failed to extract sound banks");
        AssetBuilder_Free(builder);
        Rom_Free(rom);
        return RESTOOL_ERR_EXTRACT;
    }

    // 2. print_dungeon_rooms() - All dungeon room data
    ExtractDungeonRoomData(builder);
    ExtractDungeonRoomHeaders(builder);
    ExtractDungeonRoomSimple(builder);
    ExtractEntrancesAndStartingPoints(builder);
    ExtractDefaultOverlayRooms(builder);
    ExtractDungeonSecrets(builder);
    ExtractMiscDungeonRomAssets(rom, builder);

    // 3. print_enemy_damage_data() - 1 asset
    ExtractEnemyDamageData(rom, builder);

    // 4. print_link_graphics() - 1 asset
    ExtractLinkGraphics(builder);

    // 5. print_dungeon_sprites() - 2 assets
    ExtractDungeonSprites(builder);

    // 6. print_map32_to_map16() - 4 assets
    ExtractMap32toMap16(builder);

    // 7. print_images() - Sprite and background graphics
    ExtractSpriteGraphics(rom, builder, options->sprites_from_png);
    ExtractBackgroundGraphics(rom, builder);

    // 8. print_misc() - Misc ROM assets (~28 assets)
    ExtractMiscAssets(rom, builder);

    // 9. print_dialogue() - 3 assets per language
    if (!ExtractDialogue(builder, options->languages)) {
        LogError("Failed to extract dialogue");
        AssetBuilder_Free(builder);
        Rom_Free(rom);
        return RESTOOL_ERR_DIALOGUE;
    }

    // 10. print_dungeon_map() - 2 packed assets
    ExtractDungeonMap(rom, builder);

    // 11. print_tilemaps() - 6 assets
    ExtractTilemaps(rom, builder);

    // 12. print_overworld() - Compressed overworld data (2 packed assets)
    ExtractOverworldCompressed(rom, builder);

    // 13. print_overworld_tables() - Overworld YAML data (48 assets)
    ExtractOverworldYAML(builder, rom);

    LogInfo("Total: %u assets extracted", builder->asset_count);

    // Write to output file
    if (!AssetBuilder_WriteToFile(builder, options->output_path)) {
        LogError("Failed to write assets file");
        AssetBuilder_Free(builder);
        Rom_Free(rom);
        return RESTOOL_ERR_WRITE;
    }

    LogInfo("Successfully compiled assets to %s", options->output_path);

    // Cleanup
    AssetBuilder_Free(builder);
    Rom_Free(rom);
    Restool_SetDialogueDir(NULL);  // Reset global state

    return RESTOOL_OK;
}

// Legacy API wrapper
int Restool_CompileAssets(const char *us_rom_path, const char *output_path,
                          const char *languages, const char *dialogue_dir) {
    RestoolCompileOptions options = {
        .us_rom_path = us_rom_path,
        .output_path = output_path,
        .languages = languages,
        .dialogue_dir = dialogue_dir,
        .sprites_from_png = false
    };
    return Restool_CompileAssetsEx(&options);
}

// ===========================================================================
// DAT File Access
// ===========================================================================

// DAT file signature (first 16 bytes - "Zelda3_v0     \n\0")
static const uint8_t kDatSignature[] = {
    90, 101, 108, 100, 97, 51, 95, 118, 48, 32, 32, 32, 32, 32, 10, 0
};

bool Restool_DatFileExists(const char *dir) {
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

int Restool_GetDatLanguages(const char *dir, char languages[][16], int max_languages) {
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
        if (strcmp(name_ptr, "kDialogueMap") == 0) {
            dialogue_map_index = (int)i;
            LogInfo("DatReader: Found kDialogueMap at index %d", i);
            break;
        }
        name_ptr += strlen(name_ptr) + 1;
    }
    free(key_sig);

    if (dialogue_map_index < 0) {
        LogWarn("DatReader: kDialogueMap asset not found in DAT file");
        free(sizes);
        fclose(f);
        return 0;
    }

    // Calculate offset to kDialogueMap asset
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

    // Parse language entries from kDialogueMap
    int count = 0;
    for (int i = 0; i < max_languages; i++) {
        size_t entry_size;
        const uint8_t *entry = find_index_in_memblk(dialogue_map, dialogue_map_size, i, &entry_size);
        if (!entry) break;

        // Get first element (language code) from this entry
        size_t name_size;
        const uint8_t *name = find_index_in_memblk(entry, entry_size, 0, &name_size);
        if (!name || name_size == 0 || name_size >= 16) continue;

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
