// extract.h - Shared declarations for asset extraction modules
#ifndef RESTOOL_EXTRACT_H
#define RESTOOL_EXTRACT_H

#include "types.h"
#include "asset_compiler.h"
#include "restool_util.h"
#include "yaml_util.h"

// ============================================================================
// Shared Helper Functions (defined in extract_common.c)
// ============================================================================

// Load YAML from embedded assets or filesystem
YamlDoc* LoadAssetYaml(const char *path);

// Load binary/text data from embedded assets or filesystem
// Returns allocated buffer that caller must free with AssetReader_Free()
uint8_t* LoadAssetData(const char *path, size_t *out_size);

// ============================================================================
// Graphics Extraction (extract_graphics.c)
// ============================================================================

// Extract kSprGfx (108 sprite tilesets)
// If sprites_from_png is true, loads tilesets 0-102 from PNG files instead of ROM
void ExtractSpriteGraphics(Rom *rom, AssetBuilder *builder, bool sprites_from_png);

// Extract kBgGfx (115 background tilesets)
void ExtractBackgroundGraphics(Rom *rom, AssetBuilder *builder);

// Extract Link sprite graphics
// use_custom_png: If true, load from linksprite.png; if false, extract from ROM
void ExtractLinkGraphics(AssetBuilder *builder, Rom *rom, bool use_custom_png);

// ============================================================================
// Dungeon Extraction (extract_dungeon.c)
// ============================================================================

// Extract dungeon map data (2 packed assets)
void ExtractDungeonMap(Rom *rom, AssetBuilder *builder);

// Extract dungeon sprites from ROM
void ExtractDungeonSprites(AssetBuilder *builder, Rom *rom);

// Extract dungeon secrets from ROM
void ExtractDungeonSecrets(AssetBuilder *builder, Rom *rom);

// Extract misc dungeon ROM assets (5 assets: kDungAttrsForTile_Offs, kDungAttrsForTile, etc.)
void ExtractMiscDungeonRomAssets(Rom *rom, AssetBuilder *builder);

// Extract dungeon room headers
void ExtractDungeonRoomHeaders(AssetBuilder *builder, Rom *rom);

// Extract simple dungeon room data (chests, tele_msg, pits)
void ExtractDungeonRoomSimple(AssetBuilder *builder, Rom *rom);

// Extract dungeon room object data (3-layer encoding)
void ExtractDungeonRoomData(AssetBuilder *builder, Rom *rom);

// Extract default and overlay dungeon rooms
void ExtractDefaultOverlayRooms(AssetBuilder *builder, Rom *rom);

// Extract entrances and starting points (33 assets)
void ExtractEntrancesAndStartingPoints(AssetBuilder *builder, Rom *rom);

// ============================================================================
// Overworld Extraction (extract_overworld.c)
// ============================================================================

// Extract overworld data from 160 YAML files (~48 assets)
void ExtractOverworldYAML(AssetBuilder *builder, Rom *rom);

// Extract kOverworld_Hibytes_Comp and kOverworld_Lobytes_Comp (2 packed assets)
void ExtractOverworldCompressed(Rom *rom, AssetBuilder *builder);

// ============================================================================
// Dialogue/Text Extraction (extract_dialogue.c)
// ============================================================================

// Extract dialogue assets (kDialogue, kDialogueFont, kDialogueMap)
// languages_arg: comma-separated list like "de,fr" (US is always included first), or NULL for US only
bool ExtractDialogueAssets(AssetBuilder *builder, Rom *rom, const char *languages_arg);

// Extract dialogue (wrapper for full extraction)
// languages_arg: comma-separated list like "de,fr" (US is always included first), or NULL for US only
bool ExtractDialogue(AssetBuilder *builder, Rom *rom, const char *languages_arg);

// ============================================================================
// Misc Extraction (extract_misc.c)
// ============================================================================

// Extract misc assets - simple ROM reads
void ExtractMiscAssets(Rom *rom, AssetBuilder *builder);

// Extract ROM-based assets (no YAML required)
void ExtractRomBasedAssets(Rom *rom, AssetBuilder *builder);

// Extract tilemaps from ROM
void ExtractTilemaps(Rom *rom, AssetBuilder *builder);

// Extract kEnemyDamageData (1 decompressed asset)
void ExtractEnemyDamageData(Rom *rom, AssetBuilder *builder);

// ============================================================================
// Map32 Extraction (extract_map32.c)
// ============================================================================

// Extract kMap32ToMap16 (4 assets from text file or ROM)
void ExtractMap32toMap16(AssetBuilder *builder, Rom *rom);

// ============================================================================
// Sound Extraction (in main.c)
// ============================================================================

// Extract sound banks - uses ROM data directly if embedded assets unavailable
bool ExtractSoundBanks(AssetBuilder *builder, Rom *rom);

// ============================================================================
// Font Extraction (in main.c)
// ============================================================================

// Extract font data from ROM and save to binary files
// Creates: font_{lang}.bin (4096 bytes) and fontwidth_{lang}.bin (width_count bytes)
// Used by CLI and library API for non-US language ROMs
bool ExtractFontFromRom(Rom *rom, const char *lang_code, const char *output_dir);

#endif // RESTOOL_EXTRACT_H
