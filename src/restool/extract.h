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
void ExtractSpriteGraphics(Rom *rom, AssetBuilder *builder);

// Extract kBgGfx (115 background tilesets)
void ExtractBackgroundGraphics(Rom *rom, AssetBuilder *builder);

// Extract Link sprite graphics from PNG
void ExtractLinkGraphics(AssetBuilder *builder);

// ============================================================================
// Dungeon Extraction (extract_dungeon.c)
// ============================================================================

// Extract dungeon map data (2 packed assets)
void ExtractDungeonMap(Rom *rom, AssetBuilder *builder);

// Extract dungeon sprites from YAML
void ExtractDungeonSprites(AssetBuilder *builder);

// Extract dungeon secrets from YAML
void ExtractDungeonSecrets(AssetBuilder *builder);

// Extract dungeon room headers
void ExtractDungeonRoomHeaders(AssetBuilder *builder);

// Extract simple dungeon room data (chests, tele_msg, pits)
void ExtractDungeonRoomSimple(AssetBuilder *builder);

// Extract dungeon room object data (3-layer encoding)
void ExtractDungeonRoomData(AssetBuilder *builder);

// Extract default and overlay dungeon rooms
void ExtractDefaultOverlayRooms(AssetBuilder *builder);

// Extract entrances and starting points (33 assets)
void ExtractEntrancesAndStartingPoints(AssetBuilder *builder);

// ============================================================================
// Overworld Extraction (extract_overworld.c)
// ============================================================================

// Extract overworld data from 160 YAML files (~48 assets)
void ExtractOverworldYAML(AssetBuilder *builder, Rom *rom);

// ============================================================================
// Dialogue/Text Extraction (extract_dialogue.c)
// ============================================================================

// Extract dialogue assets (kDialogue, kDialogueFont, kDialogueMap)
void ExtractDialogueAssets(AssetBuilder *builder);

// Extract dialogue (wrapper for full extraction)
bool ExtractDialogue(AssetBuilder *builder);

// ============================================================================
// Misc Extraction (extract_misc.c)
// ============================================================================

// Extract misc assets - simple ROM reads
void ExtractMiscAssets(Rom *rom, AssetBuilder *builder);

// Extract ROM-based assets (no YAML required)
void ExtractRomBasedAssets(Rom *rom, AssetBuilder *builder);

// Extract tilemaps from ROM
void ExtractTilemaps(Rom *rom, AssetBuilder *builder);

// ============================================================================
// Map32 Extraction (extract_map32.c)
// ============================================================================

// Extract kMap32ToMap16 (4 assets from text file)
void ExtractMap32toMap16(AssetBuilder *builder);

// ============================================================================
// Sound Extraction (in music_compiler.c)
// ============================================================================

// Extract sound banks using pure C music compiler
bool ExtractSoundBanks(AssetBuilder *builder);

#endif // RESTOOL_EXTRACT_H
