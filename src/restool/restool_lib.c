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
