// main.c - Zelda3 Asset Extraction Tool
// Main entry point and CLI handling

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Shared utilities
#include "../platform.h"
#include "../logging.h"

// Restool modules
#include "types.h"
#include "restool_util.h"
#include "restool_lib.h"
#include "graphics.h"
#include "asset_compiler.h"
#include "overworld.h"
#include "text.h"
#include "text_decode.h"
#include "yaml_util.h"
#include "tables.h"
#include "music_compiler.h"
#include "asset_reader.h"
#include "rom_addresses.h"
#include "extract.h"
#include "sprite_loader.h"

// Third-party
#include "sha256.h"
#include "lodepng.h"
#include "stb_image.h"

// ============================================================================
// Graphics Extraction
// ============================================================================

// Extract kSprGfx (108 sprite tilesets) - Python-compatible
// If sprites_from_png is true, loads tilesets 0-102 from PNG files instead of ROM
void ExtractSpriteGraphics(Rom *rom, AssetBuilder *builder, bool sprites_from_png) {
  printf("  Extracting kSprGfx (108 sprite tilesets)%s...\n",
         sprites_from_png ? " (from PNG)" : "");

  uint8_t **arrays = (uint8_t**)malloc(108 * sizeof(uint8_t*));
  uint32_t *sizes = (uint32_t*)malloc(108 * sizeof(uint32_t));

  if (!arrays || !sizes) {
    LogError("Failed to allocate arrays for kSprGfx");
    free(arrays);
    free(sizes);
    return;
  }

  // Load from PNG if requested
  SpriteSheetLoader *sprite_loader = NULL;
  if (sprites_from_png) {
    sprite_loader = SpriteLoader_Load("assets/sprites");
    if (!sprite_loader) {
      LogError("Failed to load sprite sheets from PNG, falling back to ROM");
      sprites_from_png = false;
    }
  }

  // Extract all 108 sprite tilesets
  for (uint32_t i = 0; i < 108; i++) {
    // For tilesets 0-102, use PNG data if available
    if (sprites_from_png && i < SPRITE_SHEET_COUNT) {
      arrays[i] = SpriteLoader_EncodeSheet(sprite_loader, i, &sizes[i]);
      if (arrays[i]) {
        continue;  // Successfully loaded from PNG
      }
      // Fall through to ROM extraction if PNG failed
      LogWarn("Tileset %u not in PNG, loading from ROM", i);
    }

    uint32_t addr = kCompSpritePtrs[i];

    if (i < 12) {
      // First 12 are uncompressed: 0x600 bytes each
      uint8_t *data = Rom_ReadPtr(rom, addr, 0x600);
      if (!data) {
        LogError("Failed to read sprite tileset %u at 0x%X", i, addr);
        arrays[i] = NULL;
        sizes[i] = 0;
        continue;
      }

      arrays[i] = (uint8_t*)malloc(0x600);
      if (arrays[i]) {
        memcpy(arrays[i], data, 0x600);
        sizes[i] = 0x600;
      } else {
        LogError("Failed to allocate sprite tileset %u", i);
        sizes[i] = 0;
      }
    } else {
      // Remaining 96 are compressed: decompress to get length, then read compressed data
      DecompressedData *decomp = Snes_Decompress(rom, addr, false);  // Little-endian for sprite gfx
      if (!decomp) {
        LogError("Failed to decompress sprite tileset %u at 0x%X", i, addr);
        arrays[i] = NULL;
        sizes[i] = 0;
        continue;
      }

      uint32_t comp_len = decomp->compressed_size;
      Snes_FreeDecompressed(decomp);  // Don't need decompressed data, just the length

      // Now read the compressed data
      uint8_t *comp_data = Rom_ReadPtr(rom, addr, comp_len);
      if (!comp_data) {
        LogError("Failed to read compressed sprite tileset %u", i);
        arrays[i] = NULL;
        sizes[i] = 0;
        continue;
      }

      arrays[i] = (uint8_t*)malloc(comp_len);
      if (arrays[i]) {
        memcpy(arrays[i], comp_data, comp_len);
        sizes[i] = comp_len;
      } else {
        LogError("Failed to allocate compressed sprite tileset %u", i);
        sizes[i] = 0;
      }
    }
  }

  // Free sprite loader
  if (sprite_loader) {
    SpriteLoader_Free(sprite_loader);
  }

  // Pack arrays and add to builder
  uint32_t packed_size = 0;
  uint8_t *packed = AssetBuilder_PackArrays(arrays, sizes, 108, &packed_size);

  if (packed) {
    AssetBuilder_AddAsset(builder, "kSprGfx", ASSET_TYPE_PACKED, packed, packed_size);
    printf("    Added kSprGfx (%u bytes packed from 108 tilesets)\n", packed_size);
    free(packed);
  } else {
    LogError("Failed to pack kSprGfx arrays");
  }

  // Free temporary arrays
  for (uint32_t i = 0; i < 108; i++) {
    free(arrays[i]);
  }
  free(arrays);
  free(sizes);
}

// Extract background graphics (kBgGfx) - 115 compressed tilesets
void ExtractBackgroundGraphics(Rom *rom, AssetBuilder *builder) {
  const uint32_t count = 115;
  uint8_t **arrays = malloc(count * sizeof(uint8_t*));
  uint32_t *sizes = malloc(count * sizeof(uint32_t));

  if (!arrays || !sizes) {
    LogError("Failed to allocate arrays for background graphics");
    free(arrays);
    free(sizes);
    return;
  }

  printf("  Extracting kBgGfx (%u background tilesets)...\n", count);
  fflush(stdout);

  for (uint32_t i = 0; i < count; i++) {
    // All background tilesets are compressed
    DecompressedData *decomp = Snes_Decompress(rom, kCompBgPtrs[i], false);  // Little-endian for bg gfx
    if (!decomp) {
      LogError("Failed to decompress background tileset %u at 0x%06X", i, kCompBgPtrs[i]);
      // Use NULL on failure - packer handles NULL entries
      arrays[i] = NULL;
      sizes[i] = 0;
      continue;
    }

    uint32_t comp_len = decomp->compressed_size;
    Snes_FreeDecompressed(decomp);

    // Read compressed data from ROM
    uint8_t *comp_data = Rom_ReadPtr(rom, kCompBgPtrs[i], comp_len);
    arrays[i] = malloc(comp_len);
    if (!arrays[i]) {
      LogError("Failed to allocate memory for background tileset %u", i);
      sizes[i] = 0;
      continue;
    }

    memcpy(arrays[i], comp_data, comp_len);
    sizes[i] = comp_len;
  }

  // Pack and add to builder
  uint32_t packed_size;
  uint8_t *packed = AssetBuilder_PackArrays(arrays, sizes, count, &packed_size);
  if (packed) {
    AssetBuilder_AddAsset(builder, "kBgGfx", ASSET_TYPE_PACKED, packed, packed_size);
    printf("    Added kBgGfx (%u bytes packed from %u tilesets)\n", packed_size, count);
    fflush(stdout);
    free(packed);
  } else {
    printf("    ERROR: Failed to pack background graphics\n");
    fflush(stdout);
  }

  // Free temporary arrays
  for (uint32_t i = 0; i < count; i++) {
    free(arrays[i]);
  }
  free(arrays);
  free(sizes);
}

// Extract dungeon map data (2 packed assets)
void ExtractDungeonMap(Rom *rom, AssetBuilder *builder) {
  const uint32_t kSizes[14] = {75, 125, 50, 75, 175, 75, 50, 75, 50, 200, 150, 75, 100, 200};

  printf("  Extracting dungeon map data (2 packed assets, 14 dungeons each)...\n");
  fflush(stdout);

  // First asset: kDungMap_FloorLayout
  uint8_t **floor_arrays = malloc(14 * sizeof(uint8_t*));
  uint32_t *floor_sizes = malloc(14 * sizeof(uint32_t));

  for (int i = 0; i < 14; i++) {
    uint16_t offset = Rom_ReadWord(rom, 0x8AF605 + i * 2);
    uint32_t addr = 0x8A0000 + offset;  // Bank 0x0A
    floor_sizes[i] = kSizes[i];
    floor_arrays[i] = malloc(floor_sizes[i]);
    memcpy(floor_arrays[i], Rom_ReadPtr(rom, addr, floor_sizes[i]), floor_sizes[i]);
  }

  uint32_t floor_packed_size;
  uint8_t *floor_packed = AssetBuilder_PackArrays(floor_arrays, floor_sizes, 14, &floor_packed_size);
  AssetBuilder_AddAsset(builder, "kDungMap_FloorLayout", ASSET_TYPE_PACKED, floor_packed, floor_packed_size);
  free(floor_packed);

  // Second asset: kDungMap_Tiles (variable sizes based on non-0xF byte count)
  uint8_t **tile_arrays = malloc(14 * sizeof(uint8_t*));
  uint32_t *tile_sizes = malloc(14 * sizeof(uint32_t));

  for (int i = 0; i < 14; i++) {
    // Count non-0xF bytes in floor layout
    uint32_t nonzero_bytes = 0;
    for (uint32_t j = 0; j < floor_sizes[i]; j++) {
      if (floor_arrays[i][j] != 0xF) {
        nonzero_bytes++;
      }
    }

    uint16_t offset = Rom_ReadWord(rom, 0x8AFBE4 + i * 2);
    uint32_t addr = 0x8A0000 + offset;  // Bank 0x0A
    tile_sizes[i] = nonzero_bytes;
    tile_arrays[i] = malloc(tile_sizes[i]);
    memcpy(tile_arrays[i], Rom_ReadPtr(rom, addr, tile_sizes[i]), tile_sizes[i]);
  }

  uint32_t tile_packed_size;
  uint8_t *tile_packed = AssetBuilder_PackArrays(tile_arrays, tile_sizes, 14, &tile_packed_size);
  AssetBuilder_AddAsset(builder, "kDungMap_Tiles", ASSET_TYPE_PACKED, tile_packed, tile_packed_size);
  free(tile_packed);

  // Free temporary arrays
  for (int i = 0; i < 14; i++) {
    free(floor_arrays[i]);
    free(tile_arrays[i]);
  }
  free(floor_arrays);
  free(floor_sizes);
  free(tile_arrays);
  free(tile_sizes);

  printf("    Added kDungMap_FloorLayout and kDungMap_Tiles\n");
  fflush(stdout);
}

// Tilemap decoder: reads until terminator (byte with 0x80 set)
static uint32_t DecodeTilemapLength(Rom *rom, uint32_t snes_addr) {
  uint32_t start_addr = snes_addr;
  while (true) {
    uint8_t byte0 = Rom_ReadByte(rom, snes_addr);
    if (byte0 & 0x80) {
      // Found terminator
      return (snes_addr - start_addr) + 1;
    }
    // Read 4-byte header
    uint8_t byte2 = Rom_ReadByte(rom, snes_addr + 2);
    uint8_t byte3 = Rom_ReadByte(rom, snes_addr + 3);
    bool is_memset = (byte2 & 0x40) != 0;
    uint32_t len = ((byte2 * 256 + byte3) & 0x3fff) + 1;

    snes_addr += 4;  // Skip header
    snes_addr += is_memset ? 2 : len;  // Skip data
  }
}

void ExtractTilemaps(Rom *rom, AssetBuilder *builder) {
  const uint32_t kTilemapAddrs[6] = {
    0x8CDD6D, 0x8CE7BF, 0x8CE2A8, 0x8CE63C, 0x8CE456, 0x8EDA9C
  };

  printf("  Extracting tilemaps (6 assets)...\n");
  fflush(stdout);

  for (int i = 0; i < 6; i++) {
    uint32_t addr = kTilemapAddrs[i];
    uint32_t len = DecodeTilemapLength(rom, addr);
    uint8_t *data = Rom_ReadPtr(rom, addr, len);

    if (data) {
      char asset_name[64];
      snprintf(asset_name, sizeof(asset_name), "kBgTilemap_%d", i);
      AssetBuilder_AddAsset(builder, asset_name, ASSET_TYPE_UINT8, data, len);
    } else {
      LogError("Failed to read tilemap %d at 0x%06X", i, addr);
    }
  }

  printf("    Added 6 tilemaps\n");
  fflush(stdout);
}

// Extract misc dungeon ROM assets (5 assets)
void ExtractMiscDungeonRomAssets(Rom *rom, AssetBuilder *builder) {
  uint16_t *words = (uint16_t*)Rom_ReadPtr(rom, 0x8e9000, 21 * 2);
  if (words) AssetBuilder_AddAsset(builder, "kDungAttrsForTile_Offs", ASSET_TYPE_UINT16, (const uint8_t*)words, 21*2);
  AssetBuilder_AddAsset(builder, "kDungAttrsForTile", ASSET_TYPE_UINT8,
                       Rom_ReadPtr(rom, 0x8e902a, 1024), 1024);
  words = (uint16_t*)Rom_ReadPtr(rom, 0x84f1de, 198 * 2);
  if (words) AssetBuilder_AddAsset(builder, "kMovableBlockDataInit", ASSET_TYPE_UINT16, (const uint8_t*)words, 198*2);
  words = (uint16_t*)Rom_ReadPtr(rom, 0x84F36A, 144 * 2);
  if (words) AssetBuilder_AddAsset(builder, "kTorchDataInit", ASSET_TYPE_UINT16, (const uint8_t*)words, 144*2);
  words = (uint16_t*)Rom_ReadPtr(rom, 0x84F48a, 48 * 2);
  if (words) AssetBuilder_AddAsset(builder, "kTorchDataJunk", ASSET_TYPE_UINT16, (const uint8_t*)words, 48*2);
}

// Extract kEnemyDamageData (1 decompressed asset)
void ExtractEnemyDamageData(Rom *rom, AssetBuilder *builder) {
  printf("  Extracting kEnemyDamageData (decompressed)...\n");
  fflush(stdout);
  DecompressedData *enemy_dmg = Snes_Decompress(rom, 0x83e800, true);
  if (enemy_dmg) {
    size_t enemy_dmg_size = enemy_dmg->size;
    AssetBuilder_AddAsset(builder, "kEnemyDamageData", ASSET_TYPE_UINT8,
                          enemy_dmg->data, enemy_dmg->size);
    Snes_FreeDecompressed(enemy_dmg);
    printf("    Added kEnemyDamageData (%zu bytes)\n", enemy_dmg_size);
  }
}

// Extract misc assets - simple ROM reads (28 assets from print_misc)
void ExtractMiscAssets(Rom *rom, AssetBuilder *builder) {
  printf("  Extracting misc assets (28 simple ROM reads)...\n");
  fflush(stdout);

  // Graphics data (uint8 arrays)
  AssetBuilder_AddAsset(builder, "kOverworldMapGfx", ASSET_TYPE_UINT8,
                        Rom_ReadPtr(rom, 0x18c000, 0x4000), 0x4000);
  AssetBuilder_AddAsset(builder, "kLightOverworldTilemap", ASSET_TYPE_UINT8,
                        Rom_ReadPtr(rom, 0xac727, 4096), 4096);
  AssetBuilder_AddAsset(builder, "kDarkOverworldTilemap", ASSET_TYPE_UINT8,
                        Rom_ReadPtr(rom, 0xaD727, 1024), 1024);

  // Tile data (uint16 - 6438 words = 12876 bytes)
  AssetBuilder_AddAsset(builder, "kPredefinedTileData", ASSET_TYPE_UINT16,
                        Rom_ReadPtr(rom, 0x9B52, 6438 * 2), 6438 * 2);

  // Map16 to Map8 (uint16 - 3752*4 = 15008 words = 30016 bytes)
  AssetBuilder_AddAsset(builder, "kMap16ToMap8", ASSET_TYPE_UINT16,
                        Rom_ReadPtr(rom, 0x8f8000, 3752 * 4 * 2), 3752 * 4 * 2);

  // Generated arrays (uint8)
  AssetBuilder_AddAsset(builder, "kGeneratedWishPondItem", ASSET_TYPE_UINT8,
                        Rom_ReadPtr(rom, 0x888450, 256), 256);
  AssetBuilder_AddAsset(builder, "kGeneratedBombosArr", ASSET_TYPE_UINT8,
                        Rom_ReadPtr(rom, 0x8890FC, 256), 256);
  AssetBuilder_AddAsset(builder, "kGeneratedEndSequence15", ASSET_TYPE_UINT8,
                        Rom_ReadPtr(rom, 0x8ead25, 256), 256);

  // Ending sequence data
  AssetBuilder_AddAsset(builder, "kEnding_Credits_Text", ASSET_TYPE_UINT8,
                        Rom_ReadPtr(rom, 0x8EB178, 1989), 1989);
  AssetBuilder_AddAsset(builder, "kEnding_Credits_Offs", ASSET_TYPE_UINT16,
                        Rom_ReadPtr(rom, 0x8EB93d, 394 * 2), 394 * 2);
  AssetBuilder_AddAsset(builder, "kEnding_MapData", ASSET_TYPE_UINT16,
                        Rom_ReadPtr(rom, 0x8EB038, 160 * 2), 160 * 2);
  AssetBuilder_AddAsset(builder, "kEnding0_Offs", ASSET_TYPE_UINT16,
                        Rom_ReadPtr(rom, 0x8EC2E1, 17 * 2), 17 * 2);
  AssetBuilder_AddAsset(builder, "kEnding0_Data", ASSET_TYPE_UINT8,
                        Rom_ReadPtr(rom, 0x8EBF4C, 917), 917);

  // Palettes (all uint16)
  AssetBuilder_AddAsset(builder, "kPalette_DungBgMain", ASSET_TYPE_UINT16,
                        Rom_ReadPtr(rom, 0x9BD734, 1800 * 2), 1800 * 2);
  AssetBuilder_AddAsset(builder, "kPalette_MainSpr", ASSET_TYPE_UINT16,
                        Rom_ReadPtr(rom, 0x9BD218, 120 * 2), 120 * 2);
  AssetBuilder_AddAsset(builder, "kPalette_ArmorAndGloves", ASSET_TYPE_UINT16,
                        Rom_ReadPtr(rom, 0x9BD308, 75 * 2), 75 * 2);
  AssetBuilder_AddAsset(builder, "kPalette_Sword", ASSET_TYPE_UINT16,
                        Rom_ReadPtr(rom, 0x9BD630, 12 * 2), 12 * 2);
  AssetBuilder_AddAsset(builder, "kPalette_Shield", ASSET_TYPE_UINT16,
                        Rom_ReadPtr(rom, 0x9BD648, 12 * 2), 12 * 2);
  AssetBuilder_AddAsset(builder, "kPalette_SpriteAux3", ASSET_TYPE_UINT16,
                        Rom_ReadPtr(rom, 0x9BD39E, 84 * 2), 84 * 2);
  AssetBuilder_AddAsset(builder, "kPalette_MiscSprite_Indoors", ASSET_TYPE_UINT16,
                        Rom_ReadPtr(rom, 0x9BD446, 77 * 2), 77 * 2);
  AssetBuilder_AddAsset(builder, "kPalette_SpriteAux1", ASSET_TYPE_UINT16,
                        Rom_ReadPtr(rom, 0x9BD4E0, 168 * 2), 168 * 2);
  AssetBuilder_AddAsset(builder, "kPalette_OverworldBgMain", ASSET_TYPE_UINT16,
                        Rom_ReadPtr(rom, 0x9BE6C8, 210 * 2), 210 * 2);
  AssetBuilder_AddAsset(builder, "kPalette_OverworldBgAux12", ASSET_TYPE_UINT16,
                        Rom_ReadPtr(rom, 0x9BE86C, 420 * 2), 420 * 2);
  AssetBuilder_AddAsset(builder, "kPalette_OverworldBgAux3", ASSET_TYPE_UINT16,
                        Rom_ReadPtr(rom, 0x9BE604, 98 * 2), 98 * 2);
  AssetBuilder_AddAsset(builder, "kPalette_PalaceMapBg", ASSET_TYPE_UINT16,
                        Rom_ReadPtr(rom, 0x9BE544, 96 * 2), 96 * 2);
  AssetBuilder_AddAsset(builder, "kPalette_PalaceMapSpr", ASSET_TYPE_UINT16,
                        Rom_ReadPtr(rom, 0x9BD70A, 21 * 2), 21 * 2);
  AssetBuilder_AddAsset(builder, "kHudPalData", ASSET_TYPE_UINT16,
                        Rom_ReadPtr(rom, 0x9BD660, 64 * 2), 64 * 2);
  AssetBuilder_AddAsset(builder, "kOverworldMapPaletteData", ASSET_TYPE_UINT16,
                        Rom_ReadPtr(rom, 0x8ADB27, 256 * 2), 256 * 2);

  printf("    Added 28 misc assets\n");
  fflush(stdout);
}

// ============================================================================
// Map32toMap16 Extraction
// ============================================================================

// Pack 4 12-bit values into 6 bytes
// res[0-3] = low 8 bits of each value
// res[4] = (v0>>8)<<4 | (v1>>8)  - high 4 bits of v0 and v1
// res[5] = (v2>>8)<<4 | (v3>>8)  - high 4 bits of v2 and v3
static inline void PackMap32Quad(const uint16_t *a, uint8_t *res) {
  res[0] = a[0] & 0xff;
  res[1] = a[1] & 0xff;
  res[2] = a[2] & 0xff;
  res[3] = a[3] & 0xff;
  res[4] = ((a[0] >> 8) << 4) | (a[1] >> 8);
  res[5] = ((a[2] >> 8) << 4) | (a[3] >> 8);
}

// ROM addresses for Map32-to-Map16 tables (SNES addresses)
// Each table has 2218 entries × 6 bytes = 13,308 bytes
static const uint32_t kMap32RomAddresses[] = {
  0x838000,  // Table 0
  0x83B400,  // Table 1
  0x848000,  // Table 2
  0x84B400,  // Table 3
};

// 2218 entries per table, 6 bytes per entry
#define MAP32_ENTRIES 2218
#define MAP32_SIZE (MAP32_ENTRIES * 6)  // 13308 bytes

// Extract Map32-to-Map16 data directly from ROM
static bool ExtractMap32toMap16FromROM(AssetBuilder *builder, Rom *rom) {
  printf("  Extracting kMap32ToMap16 (4 assets from ROM)...\n");

  const char *names[] = {
    "kMap32ToMap16_0",
    "kMap32ToMap16_1",
    "kMap32ToMap16_2",
    "kMap32ToMap16_3"
  };

  for (int i = 0; i < 4; i++) {
    uint8_t *data = malloc(MAP32_SIZE);
    if (!data) {
      LogError("Failed to allocate Map32toMap16 buffer");
      return false;
    }

    // ROM data is already in packed format - just copy it
    const uint8_t *src = Rom_ReadPtr(rom, kMap32RomAddresses[i], MAP32_SIZE);
    if (!src) {
      LogError("Failed to read Map32toMap16 table %d from ROM", i);
      free(data);
      return false;
    }
    memcpy(data, src, MAP32_SIZE);

    AssetBuilder_AddAsset(builder, names[i], ASSET_TYPE_UINT8, data, MAP32_SIZE);
    free(data);
  }

  printf("    Added kMap32ToMap16_0-3 (%d bytes each) from ROM\n", MAP32_SIZE);
  return true;
}

void ExtractMap32toMap16(AssetBuilder *builder, Rom *rom) {
  // Extract directly from ROM - this is the only supported method
  if (!ExtractMap32toMap16FromROM(builder, rom)) {
    LogError("Failed to extract kMap32ToMap16 from ROM");
  }
}

void TestMap32ToMap16(void) {
  printf("Testing Map32ToMap16 extraction...\n");

  // Load file from embedded assets or filesystem
  size_t file_size;
  char *file_data = (char *)LoadAssetData("assets/map32_to_map16.txt", &file_size);
  if (!file_data) {
    LogError("Failed to open assets/map32_to_map16.txt");
    return;
  }

  // Allocate table on heap (8872 * 4 * 2 = 71KB - too large for stack)
  uint16_t (*tab)[4] = malloc(8872 * sizeof(*tab));
  if (!tab) {
    LogError("Failed to allocate map32 table");
    AssetReader_Free((uint8_t *)file_data);
    return;
  }
  int line_count = 0;

  const char *ptr = file_data;
  const char *end = file_data + file_size;
  char line_buf[256];

  while (ptr < end && line_count < 8872) {
    const char *line_end = ptr;
    while (line_end < end && *line_end != '\n' && *line_end != '\r')
      line_end++;

    size_t line_len = line_end - ptr;
    if (line_len >= sizeof(line_buf)) line_len = sizeof(line_buf) - 1;
    memcpy(line_buf, ptr, line_len);
    line_buf[line_len] = 0;

    ptr = line_end;
    while (ptr < end && (*ptr == '\n' || *ptr == '\r'))
      ptr++;

    int index, v0, v1, v2, v3;
    if (sscanf(line_buf, "%d: %d, %d, %d, %d", &index, &v0, &v1, &v2, &v3) == 5) {
      if (index >= 0 && index < 8872) {
        tab[index][0] = v0;
        tab[index][1] = v1;
        tab[index][2] = v2;
        tab[index][3] = v3;
        line_count++;
      }
    }
  }
  AssetReader_Free((uint8_t *)file_data);

  if (line_count != 8872) {
    LogError("Expected 8872 lines, got %d", line_count);
    free(tab);
    return;
  }

  // Generate C version
  uint8_t *res[4];
  for (int j = 0; j < 4; j++) {
    res[j] = malloc(13308);
    if (!res[j]) {
      LogError("Failed to allocate res[%d]", j);
      for (int k = 0; k < j; k++) free(res[k]);
      free(tab);
      return;
    }
  }

  int out_pos = 0;
  for (int i = 0; i < 8872; i += 4) {
    for (int j = 0; j < 4; j++) {
      uint16_t quad[4] = {tab[i][j], tab[i+1][j], tab[i+2][j], tab[i+3][j]};
      PackMap32Quad(quad, &res[j][out_pos]);
    }
    out_pos += 6;
  }

  // Write C version to temp files
  for (int i = 0; i < 4; i++) {
    char filename[64];
    snprintf(filename, sizeof(filename), "/tmp/map32_c_%d.bin", i);
    FILE *out = fopen(filename, "wb");
    if (out) {
      fwrite(res[i], 1, 13308, out);
      fclose(out);
      printf("  Wrote C version: %s (13308 bytes)\n", filename);
    }
  }

  // Print first 20 bytes for comparison
  for (int i = 0; i < 4; i++) {
    printf("  C [%d][0-19]: ", i);
    for (int j = 0; j < 20; j++) {
      printf("%02x ", res[i][j]);
    }
    printf("\n");
  }

  // Cleanup
  for (int j = 0; j < 4; j++) free(res[j]);
  free(tab);

  printf("\nNow compare:\n");
  printf("  diff /tmp/map32_python_0.bin /tmp/map32_c_0.bin\n");
  printf("  diff /tmp/map32_python_1.bin /tmp/map32_c_1.bin\n");
  printf("  diff /tmp/map32_python_2.bin /tmp/map32_c_2.bin\n");
  printf("  diff /tmp/map32_python_3.bin /tmp/map32_c_3.bin\n");
}

// ============================================================================
// Link Graphics Extraction
// ============================================================================

// Encode an 8x8 tile from indexed color data to SNES 4bpp format (32 bytes)
// SNES 4bpp layout:
//   Bytes 0-15:  Bitplanes 0-1 (bits 0-1 of pixel color)
//   Bytes 16-31: Bitplanes 2-3 (bits 2-3 of pixel color)
// Each bitplane pair encodes one row (2 bytes per row, 8 rows = 16 bytes per pair)
static void Encode4bppSprite(const uint8_t *data, int offset, int pitch, uint8_t *out) {
  memset(out, 0, 32);

  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      uint8_t pixel = data[offset + y * pitch + x];

      // Bitplane 0 (bit 0 of pixel)
      out[y * 2 + 0] |= ((pixel >> 0) & 1) << (7 - x);

      // Bitplane 1 (bit 1 of pixel)
      out[y * 2 + 1] |= ((pixel >> 1) & 1) << (7 - x);

      // Bitplane 2 (bit 2 of pixel)
      out[y * 2 + 16] |= ((pixel >> 2) & 1) << (7 - x);

      // Bitplane 3 (bit 3 of pixel)
      out[y * 2 + 17] |= ((pixel >> 3) & 1) << (7 - x);
    }
  }
}

// Encode an 8x8 sprite to SNES 2bpp planar format (16 bytes)
// Format: 2 bits per pixel, interleaved bitplanes
//   Bytes 0-15: Bitplanes 0-1 (bits 0-1 of pixel color)
// Each bitplane pair encodes one row (2 bytes per row, 8 rows = 16 bytes total)
static void Encode2bppSprite(const uint8_t *data, int offset, int pitch, uint8_t *out) {
  memset(out, 0, 16);

  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      uint8_t pixel = data[offset + y * pitch + x];

      // Bitplane 0 (bit 0 of pixel)
      out[y * 2 + 0] |= ((pixel >> 0) & 1) << (7 - x);

      // Bitplane 1 (bit 1 of pixel)
      out[y * 2 + 1] |= ((pixel >> 1) & 1) << (7 - x);
    }
  }
}

// ROM address for Link graphics (4bpp SNES tiles, 128x448 pixels)
#define ROM_LINK_GFX 0x908000  // SNES $10:8000

void ExtractLinkGraphics(AssetBuilder *builder, Rom *rom, bool use_custom_png) {
  printf("  Extracting kLinkGraphics...\n");

  // Default: Extract from ROM (use_custom_png = false)
  // Custom sprites: Load from PNG (use_custom_png = true)
  if (!use_custom_png) {
    // Extract directly from ROM - this is the default behavior
    // Link graphics are already in SNES 4bpp tile format at ROM_LINK_GFX
    // Size: 16 tiles/row × 56 rows × 32 bytes/tile = 28,672 bytes
    const int total_size = 16 * 56 * 32;

    uint8_t *output = Rom_ReadPtr(rom, ROM_LINK_GFX, total_size);
    if (!output) {
      LogError("Failed to read Link graphics from ROM at 0x%X", ROM_LINK_GFX);
      return;
    }

    AssetBuilder_AddAsset(builder, "kLinkGraphics", ASSET_TYPE_UINT8, output, total_size);
    printf("    Added kLinkGraphics from ROM (%d bytes, 896 tiles)\n", total_size);
    return;
  }

  // Custom sprites mode: Load from linksprite.png
  printf("    Using custom linksprite.png...\n");

  size_t png_size;
  unsigned char *png_data = LoadAssetData("assets/linksprite.png", &png_size);
  if (!png_data) {
    LogError("--custom-sprites specified but assets/linksprite.png not found");
    return;
  }

  // Decode PNG using lodepng (supports indexed color mode)
  unsigned char *image = NULL;
  unsigned width, height;
  LodePNGState state;
  lodepng_state_init(&state);

  // Disable color conversion - keep raw palette indices
  state.decoder.color_convert = 0;

  unsigned error = lodepng_decode(&image, &width, &height, &state, png_data, png_size);

  free(png_data);

  if (error) {
    LogError("Failed to decode assets/linksprite.png: %s", lodepng_error_text(error));
    lodepng_state_cleanup(&state);
    return;
  }

  // Verify it's indexed color mode (palette)
  if (state.info_png.color.colortype != LCT_PALETTE) {
    LogError("linksprite.png must be indexed color (palette mode), got type %d",
             state.info_png.color.colortype);
    free(image);
    lodepng_state_cleanup(&state);
    return;
  }

  printf("    Loaded linksprite.png: %ux%u, palette mode with %zu colors\n",
         width, height, state.info_png.color.palettesize);

  // Verify dimensions (should be 128x448 = 16x56 tiles)
  if (width != 128 || height != 448) {
    LogError("Expected 128x448 image, got %ux%u", width, height);
    free(image);
    lodepng_state_cleanup(&state);
    return;
  }

  // Unpack 4-bit palette indices (lodepng packs 2 pixels per byte for 4-bit images)
  // With color_convert=0, 4-bit data is packed: high nibble = first pixel, low nibble = second pixel
  size_t unpacked_size = width * height;
  uint8_t *unpacked = malloc(unpacked_size);
  if (!unpacked) {
    LogError("Failed to allocate unpacked buffer");
    free(image);
    lodepng_state_cleanup(&state);
    return;
  }

  // Unpack 4-bit data to 8-bit (1 byte per pixel)
  for (size_t i = 0; i < unpacked_size / 2; i++) {
    unpacked[i * 2 + 0] = (image[i] >> 4) & 0x0F;  // High nibble
    unpacked[i * 2 + 1] = image[i] & 0x0F;          // Low nibble
  }

  free(image);
  image = unpacked;

  // Calculate output size: 56 rows × 16 cols × 32 bytes/tile = 28,672 bytes
  const int tiles_x = 16;
  const int tiles_y = 56;
  const int tile_size = 32;
  const int total_size = tiles_x * tiles_y * tile_size;

  uint8_t *output = malloc(total_size);
  if (!output) {
    LogError("Failed to allocate output buffer");
    free(image);
    lodepng_state_cleanup(&state);
    return;
  }

  // Encode all tiles (image buffer contains raw palette indices 0-15, 1 byte per pixel)
  int out_offset = 0;
  for (int tile_y = 0; tile_y < tiles_y; tile_y++) {
    for (int tile_x = 0; tile_x < tiles_x; tile_x++) {
      int pixel_offset = tile_y * width * 8 + tile_x * 8;
      Encode4bppSprite(image, pixel_offset, width, &output[out_offset]);
      out_offset += tile_size;
    }
  }

  // Add asset
  AssetBuilder_AddAsset(builder, "kLinkGraphics", ASSET_TYPE_UINT8, output, total_size);
  printf("    Added kLinkGraphics (%d bytes, %d tiles)\n", total_size, tiles_x * tiles_y);

  // Cleanup
  free(output);
  free(image);
  lodepng_state_cleanup(&state);
}

// ROM addresses for font data (2bpp SNES tiles, 256 characters × 16 bytes each)
#define ROM_FONT_US       0x8E8000   // US font
#define ROM_FONT_WIDTH_US 0x8ECADF   // US font width table (99 entries)
#define ROM_FONT_SIZE     4096       // 256 chars × 16 bytes

// Extract font directly from US ROM (works for US-based fonts)
// Non-US languages (de, fr, en) need their font extracted via --extract-dialogue
// Returns: font_data (256*16 bytes), font_width (width_count bytes)
// Caller must free both returned pointers
static bool ExtractDialogueFontFromROM(Rom *rom, const char *lang,
                                       uint8_t **out_font_data,
                                       uint8_t **out_font_width, size_t *out_width_count) {
  // Only US-based font extraction from US ROM is supported
  // These languages use the same font layout as US ROM
  if (strcmp(lang, "us") != 0 && strcmp(lang, "redux") != 0 &&
      strcmp(lang, "retrans-kal") != 0 && strcmp(lang, "es") != 0 &&
      strcmp(lang, "pl") != 0 && strcmp(lang, "nl") != 0 &&
      strcmp(lang, "sv") != 0 && strcmp(lang, "pt") != 0) {
    // Non-US languages (de, fr, en, fr-c) have different character sets
    // They require font extraction from their respective ROMs
    return false;  // Silently fail - caller will try other sources
  }

  // All US-based languages use the same font from US ROM
  int width_count = 99;
  if (strcmp(lang, "pt") == 0) width_count = 121;

  // Read font tile data from ROM (Rom_ReadPtr returns ptr to ROM buffer, so we copy)
  const uint8_t *rom_font = Rom_ReadPtr(rom, ROM_FONT_US, ROM_FONT_SIZE);
  if (!rom_font) {
    LogError("Failed to read font data from ROM at 0x%X", ROM_FONT_US);
    return false;
  }

  // Read width table from ROM
  const uint8_t *rom_width = Rom_ReadPtr(rom, ROM_FONT_WIDTH_US, width_count);
  if (!rom_width) {
    LogError("Failed to read font width table from ROM at 0x%X", ROM_FONT_WIDTH_US);
    return false;
  }

  // Allocate and copy (caller will free these)
  uint8_t *font_data = malloc(ROM_FONT_SIZE);
  uint8_t *font_width = malloc(width_count);
  if (!font_data || !font_width) {
    free(font_data);
    free(font_width);
    LogError("Failed to allocate memory for font data");
    return false;
  }
  memcpy(font_data, rom_font, ROM_FONT_SIZE);
  memcpy(font_width, rom_width, width_count);

  *out_font_data = font_data;
  *out_font_width = font_width;
  *out_width_count = width_count;
  return true;
}

// Load font from extracted binary files (font_{lang}.bin + fontwidth_{lang}.bin)
// These files are created by --extract-dialogue from language ROMs
// Returns: font_data (4096 bytes), font_width (width_count bytes)
static bool ExtractDialogueFontFromBin(const char *lang, uint8_t **out_font_data,
                                       uint8_t **out_font_width, size_t *out_width_count) {
  // Get dialogue directory (where extracted files are stored)
  const char *dir = Restool_GetDialogueDir();
  if (!dir) dir = ".";

  // Replace '-' with '_' in language code for filename
  char lang_clean[16];
  strncpy(lang_clean, lang, sizeof(lang_clean) - 1);
  lang_clean[sizeof(lang_clean) - 1] = '\0';
  for (char *p = lang_clean; *p; p++) {
    if (*p == '-') *p = '_';
  }

  // Build filenames
  char font_path[512], width_path[512];
  snprintf(font_path, sizeof(font_path), "%s/font_%s.bin", dir, lang_clean);
  snprintf(width_path, sizeof(width_path), "%s/fontwidth_%s.bin", dir, lang_clean);

  // Try to load font data
  FILE *f = fopen(font_path, "rb");
  if (!f) {
    return false;  // File doesn't exist - not an error, just not available
  }

  fseek(f, 0, SEEK_END);
  long font_size = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (font_size != ROM_FONT_SIZE) {
    LogError("Invalid font file size: %s (%ld bytes, expected %d)", font_path, font_size, ROM_FONT_SIZE);
    fclose(f);
    return false;
  }

  uint8_t *font_data = malloc(ROM_FONT_SIZE);
  if (fread(font_data, 1, ROM_FONT_SIZE, f) != ROM_FONT_SIZE) {
    LogError("Failed to read %s", font_path);
    fclose(f);
    free(font_data);
    return false;
  }
  fclose(f);

  // Load width data
  f = fopen(width_path, "rb");
  if (!f) {
    LogError("Font file found but width file missing: %s", width_path);
    free(font_data);
    return false;
  }

  fseek(f, 0, SEEK_END);
  long width_size = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (width_size < 1 || width_size > 256) {
    LogError("Invalid width file size: %s (%ld bytes)", width_path, width_size);
    fclose(f);
    free(font_data);
    return false;
  }

  uint8_t *width_data = malloc(width_size);
  if (fread(width_data, 1, width_size, f) != (size_t)width_size) {
    LogError("Failed to read %s", width_path);
    fclose(f);
    free(font_data);
    free(width_data);
    return false;
  }
  fclose(f);

  printf("    Loaded font from %s (%d bytes) + %s (%ld bytes)\n",
         font_path, ROM_FONT_SIZE, width_path, width_size);

  *out_font_data = font_data;
  *out_font_width = width_data;
  *out_width_count = (size_t)width_size;
  return true;
}

// Extract font from PNG and encode to SNES 2bpp format
// Returns: font_data (256*16 bytes), font_width (chars_per_lang bytes)
// Caller must free both returned pointers
static bool ExtractDialogueFontFromPNG(Rom *rom, const char *lang, uint8_t **out_font_data,
                                       uint8_t **out_font_width, size_t *out_width_count) {
  // Font configuration for each language
  // Format: { filename, chars_per_lang }
  const struct {
    const char *lang;
    const char *filename;
    int chars_per_lang;
  } kFontTypes[] = {
    { "us", "font.png", 99 },
    { "de", "font_de.png", 112 },
    { "fr", "font_fr.png", 112 },
    { "fr-c", "font_fr_c.png", 112 },
    { "en", "font_en.png", 102 },
    { "es", "font_es.png", 99 },
    { "sv", "font_sv.png", 99 },
    { "pl", "font_pl.png", 99 },
    { "pt", "font_pt.png", 121 },
    { "nl", "font_nl.png", 99 },       // Dutch
    { "redux", "font.png", 99 },       // Redux uses US font
    { "retrans-kal", "font.png", 99 }, // Kaleidoscope uses US font
  };

  // Find language config
  const char *font_filename = NULL;
  int chars_per_lang = 0;
  for (size_t i = 0; i < sizeof(kFontTypes) / sizeof(kFontTypes[0]); i++) {
    if (strcmp(lang, kFontTypes[i].lang) == 0) {
      font_filename = kFontTypes[i].filename;
      chars_per_lang = kFontTypes[i].chars_per_lang;
      break;
    }
  }

  if (!font_filename) {
    LogError("Unknown language: %s", lang);
    return false;
  }

  // Priority 1: Try to load from extracted binary files (font_{lang}.bin)
  // These are created by --extract-dialogue from language ROMs
  if (ExtractDialogueFontFromBin(lang, out_font_data, out_font_width, out_width_count)) {
    return true;  // Successfully loaded from .bin files
  }

  // Priority 2: Try ROM extraction (works for US-based fonts)
  if (ExtractDialogueFontFromROM(rom, lang, out_font_data, out_font_width, out_width_count)) {
    return true;  // Successfully extracted from ROM
  }

  // Priority 3: Try PNG file as last resort (for backwards compatibility)
  char path[256];
  snprintf(path, sizeof(path), "assets/%s", font_filename);

  size_t png_size;
  unsigned char *png_data = LoadAssetData(path, &png_size);
  if (!png_data) {
    // No source available
    LogError("Font for '%s' not found. Extract with: --extract-from-rom <lang_rom> --extract-dialogue", lang);
    return false;
  }

  // Decode PNG using lodepng
  unsigned char *image = NULL;
  unsigned width, height;
  LodePNGState state;
  lodepng_state_init(&state);

  // Disable color conversion - keep raw grayscale values
  state.decoder.color_convert = 0;

  unsigned error = lodepng_decode(&image, &width, &height, &state, png_data, png_size);
  free(png_data);

  if (error) {
    LogError("Failed to decode %s: %s", path, lodepng_error_text(error));
    lodepng_state_cleanup(&state);
    return false;
  }

  // Verify it's grayscale or palette mode
  LodePNGColorType colortype = state.info_png.color.colortype;
  if (colortype != LCT_GREY && colortype != LCT_PALETTE) {
    LogError("%s must be grayscale or palette mode, got type %d", font_filename, colortype);
    free(image);
    lodepng_state_cleanup(&state);
    return false;
  }

  // Font dimensions: width = 128 + 15 = 143, height varies
  const int w = 143;

  // Allocate output buffers
  const int tile_count = 256;
  const int tile_size = 16;  // 2bpp: 16 bytes per 8x8 tile
  uint8_t *font_data = calloc(tile_count * tile_size, 1);
  uint8_t *font_width = calloc(chars_per_lang, 1);

  if (!font_data || !font_width) {
    LogError("Failed to allocate font buffers");
    free(font_data);
    free(font_width);
    free(image);
    lodepng_state_cleanup(&state);
    return false;
  }

  // Extract 256 characters (16x16 grid)
  int width_idx = 0;
  for (int i = 0; i < tile_count; i++) {
    int x = i % 16;
    int y = i / 16;

    // Calculate base offset using Python's formula:
    // base_offs = x * 9 + (y * 8 + (y >> 1)) * w
    int base_offs = x * 9 + (y * 8 + (y >> 1)) * w;

    // Extract width from first row (only for even rows: y & 1 == 0)
    if ((y & 1) == 0 && width_idx < chars_per_lang) {
      // Scan for pixel value 255 in first 8 pixels
      // Python: for i in range(8): if pixel==255: break; return i+1
      // So width is index where 255 is found + 1 (or 8+1=9 if not found, but 9 is capped to 8)
      int char_width = 8;  // Default if 255 not found
      for (int j = 0; j < 8; j++) {
        if (image[base_offs + j] == 255) {
          char_width = j + 1;
          break;
        }
      }
      font_width[width_idx++] = char_width;
    }

    // Encode sprite from base_offs + w (skip the width marker row)
    // Python: encode_one_spr_2bit(font_data, base_offs + w, dst, i * 16, w)
    Encode2bppSprite(image, base_offs + w, w, &font_data[i * tile_size]);
  }

  // Cleanup
  free(image);
  lodepng_state_cleanup(&state);

  // Return results
  *out_font_data = font_data;
  *out_font_width = font_width;
  *out_width_count = chars_per_lang;

  return true;
}

// ============================================================================
// Generic Multi-Language Text Compression
// ============================================================================

// Encode dictionary strings using alphabet mapping (generic version)
// Returns array of bytearrays (caller must free each entry and the array)
// Also returns array of lengths in out_lens (caller must free)
static uint8_t **EncodeDictionary_Generic(const LanguageConfig *config, size_t *out_count, size_t **out_lens) {
  // Allocate array of pointers and lengths
  uint8_t **result = calloc(config->dictionary_size, sizeof(uint8_t *));
  size_t *lens = calloc(config->dictionary_size, sizeof(size_t));
  if (!result || !lens) {
    free(result);
    free(lens);
    return NULL;
  }

  // Encode each dictionary entry
  for (size_t i = 0; i < config->dictionary_size; i++) {
    const char *dict_str = config->dictionary[i];
    size_t dict_len = strlen(dict_str);

    // Allocate bytearray for this entry (may be shorter than dict_len due to multi-byte chars)
    result[i] = malloc(dict_len + 1);
    if (!result[i]) {
      // Cleanup on failure
      for (size_t j = 0; j < i; j++) free(result[j]);
      free(result);
      free(lens);
      return NULL;
    }

    // Encode each character using alphabet
    size_t out_pos = 0;
    for (size_t j = 0; j < dict_len; ) {
      int found = -1;
      size_t match_len = 0;

      // Try to match multi-byte UTF-8 characters first
      // Check each alphabet entry for a match at this position
      // NOTE: For duplicate entries (like two spaces in SV alphabet), use the LAST
      // match to match Python's behavior (dict comprehension takes last key)
      for (size_t k = 0; k < config->alphabet_size; k++) {
        size_t alpha_len = strlen(config->alphabet[k]);
        if (alpha_len > 0 && j + alpha_len <= dict_len &&
            strncmp(&dict_str[j], config->alphabet[k], alpha_len) == 0) {
          if (alpha_len >= match_len) {  // >= instead of > to take last match for duplicates
            found = k;
            match_len = alpha_len;
          }
        }
      }

      if (found < 0) {
        LogError("Character '%c' (0x%02x) at position %zu not found in alphabet for dictionary entry %zu",
                 dict_str[j], (unsigned char)dict_str[j], j, i);
        for (size_t k = 0; k <= i; k++) free(result[k]);
        free(result);
        free(lens);
        return NULL;
      }

      result[i][out_pos++] = (uint8_t)found;
      j += match_len;
    }
    lens[i] = out_pos;  // Store actual encoded length
  }

  *out_count = config->dictionary_size;
  *out_lens = lens;
  return result;
}

// Helper: Encode a command using the "new" EU format (DE, FR, FR-C, PT)
// Returns number of bytes written to out, or 0 on error, or -1 for "no output"
static int EncodeCommand_New(const char *cmd, int param, uint8_t *out) {
  // Simple commands (no param expected)
  if (strcmp(cmd, "Scroll") == 0) { out[0] = 0x80; return 1; }
  if (strcmp(cmd, "Waitkey") == 0) { out[0] = 0x81; return 1; }
  if (strcmp(cmd, "1") == 0) { out[0] = 0x82; return 1; }
  if (strcmp(cmd, "2") == 0) { out[0] = 0x83; return 1; }
  if (strcmp(cmd, "3") == 0) { out[0] = 0x84; return 1; }
  if (strcmp(cmd, "Name") == 0) { out[0] = 0x85; return 1; }

  // Two-byte commands (0x87 + computed param)
  if (strcmp(cmd, "Wait") == 0 && param >= 0 && param < 16) {
    out[0] = 0x87; out[1] = param + 0x00; return 2;
  }
  if (strcmp(cmd, "Color") == 0 && param >= 0 && param < 16) {
    out[0] = 0x87; out[1] = param + 0x10; return 2;
  }
  if (strcmp(cmd, "Number") == 0 && param >= 0 && param < 16) {
    out[0] = 0x87; out[1] = param + 0x20; return 2;
  }
  if (strcmp(cmd, "Speed") == 0 && param >= 0 && param < 16) {
    out[0] = 0x87; out[1] = param + 0x30; return 2;
  }
  if (strcmp(cmd, "Sound") == 0) {
    if (param == 45) { out[0] = 0x87; out[1] = 0x40; return 2; }
    if (param == 64) { return -1; }  // No output
  }
  if (strcmp(cmd, "Choose") == 0) { out[0] = 0x87; out[1] = 0x80; return 2; }
  if (strcmp(cmd, "Choose2") == 0) { out[0] = 0x87; out[1] = 0x81; return 2; }
  if (strcmp(cmd, "Choose3") == 0) { out[0] = 0x87; out[1] = 0x82; return 2; }
  if (strcmp(cmd, "Selchg") == 0) { out[0] = 0x87; out[1] = 0x83; return 2; }
  if (strcmp(cmd, "Item") == 0) { out[0] = 0x87; out[1] = 0x84; return 2; }
  if (strcmp(cmd, "NextPic") == 0) { out[0] = 0x87; out[1] = 0x85; return 2; }
  if (strcmp(cmd, "Window") == 0) {
    if (param == 0) { return -1; }  // No output
    if (param == 2) { out[0] = 0x87; out[1] = 0x86; return 2; }
  }
  if (strcmp(cmd, "Position") == 0) {
    if (param == 0) { out[0] = 0x87; out[1] = 0x87; return 2; }
    if (param == 1) { out[0] = 0x87; out[1] = 0x88; return 2; }
  }
  if (strcmp(cmd, "ScrollSpd") == 0 && param == 0) { return -1; }  // No output

  LogError("Unknown command for new encoder: %s (param=%d)", cmd, param);
  return 0;
}

// Helper: Encode a command using the "org" original US format
// Returns number of bytes written to out, or 0 on error
static int EncodeCommand_Org(const char *cmd, int param, uint8_t *out, const LanguageConfig *config) {
  // Find command in list
  int cmd_index = -1;
  for (size_t i = 0; i < config->command_count; i++) {
    if (strcmp(cmd, config->command_names[i]) == 0) {
      cmd_index = i;
      break;
    }
  }

  if (cmd_index < 0) {
    LogError("Unknown command: %s", cmd);
    return 0;
  }

  // Verify parameter matches expected length
  int expected_len = config->command_lengths[cmd_index];
  int has_param = (param >= 0) ? 2 : 1;

  if (expected_len != has_param) {
    LogError("Command %s expects %d bytes, got %d", cmd, expected_len, has_param);
    return 0;
  }

  // Encode: cmd_index + COMMAND_START, with optional parameter
  out[0] = cmd_index + config->COMMAND_START;
  if (param >= 0) {
    out[1] = param;
    return 2;
  }
  return 1;
}

// Helper: Encode a command using the appropriate encoder for the language
// Returns number of bytes written to out, or 0 on error, or -1 for "no output"
static int EncodeCommand_Generic(const char *cmd, int param, uint8_t *out, const LanguageConfig *config) {
  if (config->uses_new_format) {
    return EncodeCommand_New(cmd, param, out);
  } else {
    return EncodeCommand_Org(cmd, param, out, config);
  }
}

// Compress a single dialogue string using greedy dictionary matching (generic version)
// Returns compressed bytearray (caller must free), or NULL on error
static uint8_t *CompressString_Generic(const char *str, const LanguageConfig *config, size_t *out_len) {
  // Build reverse lookup: first_char -> list of (dict_entry, dict_index)
  struct DictEntry {
    const char *str;
    int index;
    struct DictEntry *next;
  };

  struct DictEntry *reverse[256] = {0};  // Hash by first character

  for (size_t i = 0; i < config->dictionary_size; i++) {
    const char *dict_str = config->dictionary[i];
    unsigned char first_char = dict_str[0];

    struct DictEntry *entry = malloc(sizeof(struct DictEntry));
    entry->str = dict_str;
    entry->index = i;
    entry->next = reverse[first_char];
    reverse[first_char] = entry;
  }

  // Compress string using greedy matching
  size_t capacity = strlen(str) * 2 + 100;  // Overestimate
  uint8_t *result = malloc(capacity);
  size_t result_len = 0;

  size_t i = 0;
  size_t str_len = strlen(str);

  while (i < str_len) {
    const char *remaining = &str[i];
    int matched = 0;

    // Try dictionary match first (greedy: longest match wins)
    unsigned char first_char = remaining[0];
    if (reverse[first_char]) {
      struct DictEntry *entry = reverse[first_char];
      size_t best_len = 0;
      int best_index = -1;

      while (entry) {
        size_t dict_len = strlen(entry->str);
        if (strncmp(remaining, entry->str, dict_len) == 0 && dict_len > best_len) {
          best_len = dict_len;
          best_index = entry->index;
        }
        entry = entry->next;
      }

      if (best_index >= 0) {
        result[result_len++] = best_index + config->DICT_BASE_ENC;
        i += best_len;
        matched = 1;
      }
    }

    // If no dict match, try commands or alphabet
    if (!matched) {
      // Handle commands: [CommandName] or [CommandName param]
      if (remaining[0] == '[') {
        const char *close_bracket = strchr(remaining, ']');
        if (close_bracket) {
          size_t cmd_len = close_bracket - remaining - 1;  // Length of text between [ and ]
          char cmd_buf[64];
          if (cmd_len >= sizeof(cmd_buf)) cmd_len = sizeof(cmd_buf) - 1;
          strncpy(cmd_buf, remaining + 1, cmd_len);
          cmd_buf[cmd_len] = '\0';

          // Check if it's a multi-char alphabet entry first (like "[Ankh]", "[Up]", etc.)
          char full_cmd[66];
          snprintf(full_cmd, sizeof(full_cmd), "[%s]", cmd_buf);
          int alphabet_index = -1;
          for (size_t k = 0; k < config->alphabet_size; k++) {
            if (strcmp(full_cmd, config->alphabet[k]) == 0) {
              alphabet_index = k;
              // Don't break - continue to find last match for duplicates
            }
          }

          if (alphabet_index >= 0) {
            // It's an alphabet entry like [Ankh]
            result[result_len++] = alphabet_index;
            i += cmd_len + 2;  // Skip [...]
          } else {
            // It's a command like [Wait 05]
            char *space = strchr(cmd_buf, ' ');
            int param = -1;

            if (space) {
              *space = '\0';
              param = atoi(space + 1);
            }

            uint8_t cmd_bytes[2];
            int cmd_result_len = EncodeCommand_Generic(cmd_buf, param, cmd_bytes, config);
            if (cmd_result_len > 0) {
              memcpy(&result[result_len], cmd_bytes, cmd_result_len);
              result_len += cmd_result_len;
              i += cmd_len + 2;  // Skip [...]
            } else if (cmd_result_len == -1) {
              // No output for this command (e.g., [Window 0], [Sound 64])
              i += cmd_len + 2;  // Skip [...] but don't add any bytes
            } else {
              LogError("Failed to encode command: %s", cmd_buf);
              goto cleanup_error;
            }
          }
          matched = 1;
        }
      }

      // If still no match, try single character or multi-byte from alphabet
      if (!matched) {
        int alphabet_index = -1;
        size_t best_len = 0;

        // Try all alphabet entries, prefer longest match
        // NOTE: For duplicates (like two spaces), use >= to take the last match
        for (size_t k = 0; k < config->alphabet_size; k++) {
          size_t alpha_len = strlen(config->alphabet[k]);
          if (alpha_len > 0 && alpha_len <= (str_len - i) &&
              strncmp(remaining, config->alphabet[k], alpha_len) == 0) {
            if (alpha_len >= best_len) {  // >= to match Python's behavior for duplicates
              alphabet_index = k;
              best_len = alpha_len;
            }
          }
        }

        if (alphabet_index >= 0) {
          result[result_len++] = alphabet_index;
          i += best_len;
        } else {
          LogError("Character not found in alphabet: '%c' (0x%02x) at position %zu",
                   remaining[0], (unsigned char)remaining[0], i);
          goto cleanup_error;
        }
      }
    }
  }

  // Cleanup reverse lookup and return success
  for (int j = 0; j < 256; j++) {
    struct DictEntry *entry = reverse[j];
    while (entry) {
      struct DictEntry *next = entry->next;
      free(entry);
      entry = next;
    }
  }

  *out_len = result_len;
  return result;

cleanup_error:
  free(result);
  for (int j = 0; j < 256; j++) {
    struct DictEntry *entry = reverse[j];
    while (entry) {
      struct DictEntry *next = entry->next;
      free(entry);
      entry = next;
    }
  }
  *out_len = 0;
  return NULL;
}

// Pack multiple byte arrays with offset table (matching Python's pack_arrays exactly)
// Format: [offsets] + [data arrays] + [trailer]
// Uses uint16 if total < 65536 and count <= 8192, otherwise uint32
// Returns packed data (caller must free)
static uint8_t *PackArrays(uint8_t **arrays, size_t *array_lens, size_t count, size_t *out_len) {
  if (count == 0) {
    *out_len = 0;
    return NULL;
  }

  // Calculate cumulative offsets
  // Python: offs is cumulative of first n-1 arrays, checked BEFORE adding last array
  size_t *offsets = malloc((count - 1) * sizeof(size_t));
  size_t offs = 0;  // Cumulative offset (excludes last array, matches Python)

  for (size_t i = 0; i < count - 1; i++) {
    offs += array_lens[i];
    offsets[i] = offs;
  }

  // Determine format: uint16 or uint32
  // Python checks offs (cumulative of first n-1) BEFORE adding last array
  int use_uint16 = (offs < 65536 && count <= 8192);

  // Now add last array for total size calculation
  size_t total_data_size = offs + array_lens[count - 1];
  size_t offset_size = use_uint16 ? 2 : 4;
  size_t offset_table_size = (count - 1) * offset_size;
  size_t trailer_size = 2;  // Always uint16

  // Calculate total output size
  size_t total_size = offset_table_size + total_data_size + trailer_size;

  // Allocate output buffer
  uint8_t *result = malloc(total_size);
  if (!result) {
    free(offsets);
    return NULL;
  }

  size_t pos = 0;

  // Write offset table
  for (size_t i = 0; i < count - 1; i++) {
    if (use_uint16) {
      result[pos++] = offsets[i] & 0xFF;
      result[pos++] = (offsets[i] >> 8) & 0xFF;
    } else {
      result[pos++] = offsets[i] & 0xFF;
      result[pos++] = (offsets[i] >> 8) & 0xFF;
      result[pos++] = (offsets[i] >> 16) & 0xFF;
      result[pos++] = (offsets[i] >> 24) & 0xFF;
    }
  }

  free(offsets);

  // Write data arrays
  for (size_t i = 0; i < count; i++) {
    memcpy(&result[pos], arrays[i], array_lens[i]);
    pos += array_lens[i];
  }

  // Write trailer: len(arrays) - 1, with 8192 offset for uint32 format
  uint16_t trailer = use_uint16 ? (count - 1) : (8192 + count - 1);
  result[pos++] = trailer & 0xFF;
  result[pos++] = (trailer >> 8) & 0xFF;

  *out_len = total_size;
  return result;
}

// Helper: Get dialogue filename for a language
// All languages use dialogue_dir override if set, otherwise "assets/"
// For US, file is named "dialogue.txt" (legacy), others are "dialogue_{lang}.txt"
static char* GetDialogueFilename(const char *lang) {
  char *filename = malloc(512);  // Larger buffer for full paths
  const char *dir = Restool_GetDialogueDir();
  if (!dir) dir = "assets";

  if (strcmp(lang, "us") == 0) {
    // US uses "dialogue.txt" (legacy naming)
    snprintf(filename, 512, "%s/dialogue.txt", dir);
  } else {
    // Other languages: dialogue_{lang}.txt
    // Replace '-' with '_' in language code for filename
    char lang_clean[16];
    strncpy(lang_clean, lang, sizeof(lang_clean) - 1);
    lang_clean[sizeof(lang_clean) - 1] = '\0';
    for (char *p = lang_clean; *p; p++) {
      if (*p == '-') *p = '_';
    }
    snprintf(filename, 512, "%s/dialogue_%s.txt", dir, lang_clean);
  }
  return filename;
}

// Helper: Process one language's dialogue data
// Returns inner pack (dict + dialogue) or NULL on error
static uint8_t *ProcessLanguageDialogue(const char *lang, const LanguageConfig *config, size_t *out_len) {
  // Get dialogue filename
  char *dialogue_filename = GetDialogueFilename(lang);

  // Load dialogue file
  size_t dialogue_size;
  char *dialogue_file_data = (char *)LoadAssetData(dialogue_filename, &dialogue_size);
  if (!dialogue_file_data) {
    LogError("Failed to read %s", dialogue_filename);
    free(dialogue_filename);
    return NULL;
  }
  free(dialogue_filename);

  // Encode dictionary
  size_t dict_count = 0;
  size_t *dict_lens = NULL;
  uint8_t **dict_data = EncodeDictionary_Generic(config, &dict_count, &dict_lens);
  if (!dict_data) {
    LogError("Failed to encode dictionary for %s", lang);
    free(dialogue_file_data);
    return NULL;
  }

  // Pack dictionary (dict_lens already populated by EncodeDictionary_Generic)
  size_t dict_packed_len = 0;
  uint8_t *dict_packed = PackArrays(dict_data, dict_lens, dict_count, &dict_packed_len);
  free(dict_lens);

  for (size_t i = 0; i < dict_count; i++) free(dict_data[i]);
  free(dict_data);

  if (!dict_packed) {
    LogError("Failed to pack dictionary for %s", lang);
    free(dialogue_file_data);
    return NULL;
  }

  // Count lines
  size_t line_count = 0;
  for (size_t i = 0; i < dialogue_size; i++) {
    if (dialogue_file_data[i] == '\n') line_count++;
  }

  // Allocate arrays for compressed strings
  uint8_t **dialogue_compressed = calloc(line_count, sizeof(uint8_t *));
  size_t *dialogue_lens = calloc(line_count, sizeof(size_t));

  // Compress each line
  size_t line_idx = 0;
  char *line_start = dialogue_file_data;

  for (size_t i = 0; i <= dialogue_size; i++) {
    if (i == dialogue_size || dialogue_file_data[i] == '\n') {
      if (i > 0 && line_start < &dialogue_file_data[i]) {
        size_t line_len = &dialogue_file_data[i] - line_start;
        char *line = malloc(line_len + 1);
        memcpy(line, line_start, line_len);
        line[line_len] = '\0';

        char *colon = strstr(line, ": ");
        if (colon) {
          char *text = colon + 2;
          size_t compressed_len = 0;
          dialogue_compressed[line_idx] = CompressString_Generic(text, config, &compressed_len);
          dialogue_lens[line_idx] = compressed_len;

          if (!dialogue_compressed[line_idx]) {
            LogError("Failed to compress dialogue line %zu for %s", line_idx, lang);
            free(line);
            for (size_t j = 0; j < line_idx; j++) free(dialogue_compressed[j]);
            free(dialogue_compressed);
            free(dialogue_lens);
            free(dialogue_file_data);
            free(dict_packed);
            return NULL;
          }
          line_idx++;
        }
        free(line);
      }
      line_start = &dialogue_file_data[i + 1];
    }
  }

  free(dialogue_file_data);

  // Pack dialogue strings
  size_t dialogue_packed_len = 0;
  uint8_t *dialogue_packed = PackArrays(dialogue_compressed, dialogue_lens, line_idx, &dialogue_packed_len);

  for (size_t i = 0; i < line_idx; i++) free(dialogue_compressed[i]);
  free(dialogue_compressed);
  free(dialogue_lens);

  if (!dialogue_packed) {
    LogError("Failed to pack dialogue for %s", lang);
    free(dict_packed);
    return NULL;
  }

  // Create inner pack (dict + dialogue)
  uint8_t *inner_arrays[] = {dict_packed, dialogue_packed};
  size_t inner_lens[] = {dict_packed_len, dialogue_packed_len};

  size_t inner_len = 0;
  uint8_t *inner = PackArrays(inner_arrays, inner_lens, 2, &inner_len);

  free(dict_packed);
  free(dialogue_packed);

  *out_len = inner_len;
  return inner;
}

// Extract all dialogue assets with multi-language support
// languages_arg: comma-separated list like "de,fr,es" (US is always included first)
// Returns true on success, false on error
bool ExtractDialogueAssets(AssetBuilder *builder, Rom *rom, const char *languages_arg) {
  // Parse languages - always start with "us"
  const char *lang_list[16];
  size_t lang_count = 1;
  lang_list[0] = "us";

  // Parse additional languages from comma-separated list
  char *languages_copy = NULL;
  if (languages_arg && strlen(languages_arg) > 0) {
    languages_copy = strdup(languages_arg);
    char *token = strtok(languages_copy, ",");
    while (token && lang_count < 16) {
      // Skip if already in list (e.g., "us" specified again)
      bool duplicate = false;
      for (size_t i = 0; i < lang_count; i++) {
        if (strcmp(lang_list[i], token) == 0) {
          duplicate = true;
          break;
        }
      }

      if (!duplicate) {
        // Validate language
        const LanguageConfig *config = TextDecode_GetLanguageConfig(token);
        if (!config) {
          LogError("Unknown language code: %s", token);
          free(languages_copy);
          return false;
        }

        // Check dialogue file exists
        char *dialogue_filename = GetDialogueFilename(token);
        size_t dummy_size;
        char *test_data = (char *)LoadAssetData(dialogue_filename, &dummy_size);
        if (!test_data) {
          LogError("Dialogue file not found: %s (extract with --extract-dialogue first)", dialogue_filename);
          free(dialogue_filename);
          free(languages_copy);
          return false;
        }
        free(test_data);
        free(dialogue_filename);

        lang_list[lang_count++] = token;
      }
      token = strtok(NULL, ",");
    }
  }

  printf("  Extracting dialogue (%zu language%s: ", lang_count, lang_count > 1 ? "s" : "");
  for (size_t i = 0; i < lang_count; i++) {
    printf("%s%s", lang_list[i], i < lang_count - 1 ? ", " : "");
  }
  printf(")...\n");

  // Allocate arrays for all languages
  uint8_t **all_dialogue = malloc(lang_count * sizeof(uint8_t*));
  size_t *all_dialogue_lens = malloc(lang_count * sizeof(size_t));
  uint8_t **all_fonts = malloc(lang_count * sizeof(uint8_t*));
  size_t *all_font_lens = malloc(lang_count * sizeof(size_t));
  uint8_t **all_maps = malloc(lang_count * sizeof(uint8_t*));
  size_t *all_map_lens = malloc(lang_count * sizeof(size_t));

  // Process each language
  for (size_t i = 0; i < lang_count; i++) {
    const char *lang = lang_list[i];
    const LanguageConfig *config = TextDecode_GetLanguageConfig(lang);

    printf("    Processing %s (alphabet_size=%zu, dict_size=%zu)...\n",
           lang, config->alphabet_size, config->dictionary_size);

    // For US language, auto-extract dialogue from ROM if file doesn't exist
    if (strcmp(lang, "us") == 0) {
      char *dialogue_filename = GetDialogueFilename(lang);
      size_t dummy_size;
      char *test_data = (char *)LoadAssetData(dialogue_filename, &dummy_size);
      if (!test_data) {
        // US dialogue file not found - extract directly from ROM
        printf("      (extracting US dialogue from ROM...)\n");
        DecodedStringsArray *strings = TextDecode_DecodeStrings(rom, "us");
        if (!strings) {
          LogError("Failed to decode US dialogue from ROM");
          free(dialogue_filename);
          free(all_dialogue); free(all_dialogue_lens);
          free(all_fonts); free(all_font_lens);
          free(all_maps); free(all_map_lens);
          if (languages_copy) free(languages_copy);
          return false;
        }
        // Write to dialogue.txt (in current directory or assets/)
        const char *out_dir = Restool_GetDialogueDir();
        if (!out_dir) out_dir = "assets";
        if (!TextDecode_WriteDialogueFile(strings, "us", out_dir)) {
          LogError("Failed to write US dialogue file");
          TextDecode_FreeStrings(strings);
          free(dialogue_filename);
          free(all_dialogue); free(all_dialogue_lens);
          free(all_fonts); free(all_font_lens);
          free(all_maps); free(all_map_lens);
          if (languages_copy) free(languages_copy);
          return false;
        }
        TextDecode_FreeStrings(strings);
        printf("      (wrote %s)\n", dialogue_filename);
      } else {
        free(test_data);
      }
      free(dialogue_filename);
    }

    // 1. Process dialogue (dict + strings)
    all_dialogue[i] = ProcessLanguageDialogue(lang, config, &all_dialogue_lens[i]);
    if (!all_dialogue[i]) {
      LogError("Failed to process dialogue for %s", lang);
      // Cleanup and return
      for (size_t j = 0; j < i; j++) {
        free(all_dialogue[j]);
        free(all_fonts[j]);
        free(all_maps[j]);
      }
      free(all_dialogue); free(all_dialogue_lens);
      free(all_fonts); free(all_font_lens);
      free(all_maps); free(all_map_lens);
      if (languages_copy) free(languages_copy);
      return false;
    }

    // 2. Extract font
    uint8_t *font_data = NULL, *font_width = NULL;
    size_t font_width_count = 0;

    if (!ExtractDialogueFontFromPNG(rom, lang, &font_data, &font_width, &font_width_count)) {
      LogError("Failed to extract dialogue font for %s", lang);
      for (size_t j = 0; j <= i; j++) free(all_dialogue[j]);
      for (size_t j = 0; j < i; j++) { free(all_fonts[j]); free(all_maps[j]); }
      free(all_dialogue); free(all_dialogue_lens);
      free(all_fonts); free(all_font_lens);
      free(all_maps); free(all_map_lens);
      if (languages_copy) free(languages_copy);
      return false;
    }

    // Pack font (inner: font_data + font_width)
    uint8_t *inner_font_arrays[] = {font_data, font_width};
    size_t inner_font_lens[] = {256 * 16, font_width_count};
    all_fonts[i] = PackArrays(inner_font_arrays, inner_font_lens, 2, &all_font_lens[i]);
    free(font_data);
    free(font_width);

    if (!all_fonts[i]) {
      LogError("Failed to pack font for %s", lang);
      for (size_t j = 0; j <= i; j++) free(all_dialogue[j]);
      for (size_t j = 0; j < i; j++) { free(all_fonts[j]); free(all_maps[j]); }
      free(all_dialogue); free(all_dialogue_lens);
      free(all_fonts); free(all_font_lens);
      free(all_maps); free(all_map_lens);
      if (languages_copy) free(languages_copy);
      return false;
    }

    // 3. Create map entry (lang_code + flags)
    // Store original language code (including hyphens) - matches Python behavior
    uint8_t lang_bytes[16];
    size_t lang_bytes_len = strlen(lang);
    memcpy(lang_bytes, lang, lang_bytes_len);

    // Flags: [index, index, flags]
    // Bit 0: uses_new_format (EU encoder)
    // Bit 1: non-US language marker
    uint8_t flags = 0;
    if (config->uses_new_format) flags |= 0x01;
    if (i != 0) flags |= 0x02;

    uint8_t flags_bytes[] = {(uint8_t)i, (uint8_t)i, flags};

    uint8_t *inner_map_arrays[] = {lang_bytes, flags_bytes};
    size_t inner_map_lens[] = {lang_bytes_len, 3};
    all_maps[i] = PackArrays(inner_map_arrays, inner_map_lens, 2, &all_map_lens[i]);

    if (!all_maps[i]) {
      LogError("Failed to pack map for %s", lang);
      for (size_t j = 0; j <= i; j++) { free(all_dialogue[j]); free(all_fonts[j]); }
      for (size_t j = 0; j < i; j++) free(all_maps[j]);
      free(all_dialogue); free(all_dialogue_lens);
      free(all_fonts); free(all_font_lens);
      free(all_maps); free(all_map_lens);
      if (languages_copy) free(languages_copy);
      return false;
    }
  }

  // Create outer packs with all languages
  size_t kDialogue_len = 0;
  uint8_t *kDialogue = PackArrays(all_dialogue, all_dialogue_lens, lang_count, &kDialogue_len);

  size_t kDialogueFont_len = 0;
  uint8_t *kDialogueFont = PackArrays(all_fonts, all_font_lens, lang_count, &kDialogueFont_len);

  size_t kDialogueMap_len = 0;
  uint8_t *kDialogueMap = PackArrays(all_maps, all_map_lens, lang_count, &kDialogueMap_len);

  // Cleanup per-language data
  for (size_t i = 0; i < lang_count; i++) {
    free(all_dialogue[i]);
    free(all_fonts[i]);
    free(all_maps[i]);
  }
  free(all_dialogue); free(all_dialogue_lens);
  free(all_fonts); free(all_font_lens);
  free(all_maps); free(all_map_lens);
  if (languages_copy) free(languages_copy);

  if (!kDialogue || !kDialogueFont || !kDialogueMap) {
    LogError("Failed to create outer packs");
    free(kDialogue);
    free(kDialogueFont);
    free(kDialogueMap);
    return false;
  }

  // Add assets
  AssetBuilder_AddAsset(builder, "kDialogue", ASSET_TYPE_UINT8, kDialogue, kDialogue_len);
  printf("    Added kDialogue (%zu bytes)\n", kDialogue_len);
  free(kDialogue);

  AssetBuilder_AddAsset(builder, "kDialogueFont", ASSET_TYPE_UINT8, kDialogueFont, kDialogueFont_len);
  printf("    Added kDialogueFont (%zu bytes)\n", kDialogueFont_len);
  free(kDialogueFont);

  AssetBuilder_AddAsset(builder, "kDialogueMap", ASSET_TYPE_UINT8, kDialogueMap, kDialogueMap_len);
  printf("    Added kDialogueMap (%zu bytes)\n", kDialogueMap_len);
  free(kDialogueMap);

  printf("  Dialogue complete: 3 assets (%zu language%s)\n", lang_count, lang_count > 1 ? "s" : "");
  return true;
}

void TestLinkGraphics(void) {
  printf("Testing Link graphics extraction...\n");

  // Load PNG file from embedded assets or filesystem
  size_t png_size;
  unsigned char *png_data = LoadAssetData("assets/linksprite.png", &png_size);
  if (!png_data) {
    LogError("Failed to read assets/linksprite.png");
    return;
  }

  // Decode PNG using lodepng
  unsigned char *image = NULL;
  unsigned width, height;
  LodePNGState state;
  lodepng_state_init(&state);

  // Disable color conversion - keep raw palette indices
  state.decoder.color_convert = 0;

  unsigned error = lodepng_decode(&image, &width, &height, &state, png_data, png_size);

  free(png_data);

  if (error) {
    LogError("Failed to decode assets/linksprite.png: %s", lodepng_error_text(error));
    lodepng_state_cleanup(&state);
    return;
  }

  printf("  Loaded: %ux%u (palette mode)\n", width, height);

  // Unpack 4-bit palette indices
  size_t unpacked_size = width * height;
  uint8_t *unpacked = malloc(unpacked_size);
  for (size_t i = 0; i < unpacked_size / 2; i++) {
    unpacked[i * 2 + 0] = (image[i] >> 4) & 0x0F;
    unpacked[i * 2 + 1] = image[i] & 0x0F;
  }
  free(image);
  image = unpacked;

  // Encode
  const int total_size = 16 * 56 * 32;
  uint8_t *output = malloc(total_size);

  int out_offset = 0;
  for (int tile_y = 0; tile_y < 56; tile_y++) {
    for (int tile_x = 0; tile_x < 16; tile_x++) {
      int pixel_offset = tile_y * width * 8 + tile_x * 8;
      Encode4bppSprite(image, pixel_offset, width, &output[out_offset]);
      out_offset += 32;
    }
  }

  // Write C version
  FILE *f = fopen("/tmp/link_graphics_c.bin", "wb");
  if (f) {
    fwrite(output, 1, total_size, f);
    fclose(f);
    printf("  Wrote C version: /tmp/link_graphics_c.bin (%d bytes)\n", total_size);
  }

  // Print first 32 bytes
  printf("  C [0-31]: ");
  for (int i = 0; i < 32; i++) {
    printf("%02x ", output[i]);
  }
  printf("\n");

  free(output);
  free(image);
  lodepng_state_cleanup(&state);

  printf("\nCompare:\n");
  printf("  diff /tmp/link_graphics_python.bin /tmp/link_graphics_c.bin\n");
}

void TestDungeonSprites(void) {
  printf("Testing dungeon sprites extraction...\n");

  // Allocate buffers (same as ExtractDungeonSprites)
  uint16_t *offsets = calloc(320, sizeof(uint16_t));
  uint8_t *data = malloc(64 * 1024);
  size_t data_len = 0;

  // Initialize with [0, 0xff]
  data[data_len++] = 0;
  data[data_len++] = 0xff;

  int rooms_processed = 0;
  int sprites_encoded = 0;

  // Process all 320 dungeon rooms
  for (int room = 0; room < 320; room++) {
    char filename[64];
    snprintf(filename, sizeof(filename), "assets/dungeon/dungeon-%d.yaml", room);

    YamlDoc *doc = LoadAssetYaml(filename);
    if (!doc) {
      continue;
    }

    YamlNode *root = Yaml_GetRoot(doc);
    YamlNode *header = Yaml_GetMapping(root, "Header");
    YamlNode *sprites = Yaml_GetMapping(root, "Sprites");

    if (!header || !sprites) {
      Yaml_Free(doc);
      continue;
    }

    int sortmode = Yaml_GetInt(header, "sort_sprites", 0);
    int sprite_count = Yaml_GetSequenceLength(sprites);

    if (sprite_count == 0 && sortmode == 0) {
      Yaml_Free(doc);
      continue;
    }

    offsets[room] = data_len;
    data[data_len++] = sortmode;

    for (int i = 0; i < sprite_count; i++) {
      YamlNode *sprite = Yaml_GetSequence(sprites, i);
      if (!sprite || Yaml_GetSequenceLength(sprite) < 4) {
        continue;
      }

      YamlNode *xx_node = Yaml_GetSequence(sprite, 0);
      YamlNode *yy_node = Yaml_GetSequence(sprite, 1);
      YamlNode *floor_node = Yaml_GetSequence(sprite, 2);
      YamlNode *name_node = Yaml_GetSequence(sprite, 3);

      int xx = Yaml_AsInt(xx_node);
      int yy = Yaml_AsInt(yy_node);
      const char *floor_str = Yaml_AsString(floor_node);
      const char *name = Yaml_AsString(name_node);

      int f = (strcmp(floor_str, "lower") == 0) ? 1 : 0;

      // Parse subcode
      int ss = 0;
      char name_buf[64];
      strncpy(name_buf, name, sizeof(name_buf) - 1);
      name_buf[sizeof(name_buf) - 1] = '\0';

      if (strlen(name_buf) > 2 && name_buf[2] == '.') {
        char *dash = strchr(name_buf + 3, '-');
        if (dash) {
          *dash = '\0';
          ss = atoi(name_buf + 3);
          snprintf(name_buf, sizeof(name_buf), "%.2s-%s", name, dash + 1);
        }
      }

      int sprite_idx = FindSpriteIndex(name_buf);
      if (sprite_idx < 0) sprite_idx = 0;

      // Encode 3-byte sprite data
      if (sprite_idx >= 0x100) {
        data[data_len++] = (f << 7) | (0 << 5) | yy;
        data[data_len++] = xx | (7 << 5);
        data[data_len++] = sprite_idx & 0xff;
      } else {
        data[data_len++] = (f << 7) | ((ss >> 3) << 5) | yy;
        data[data_len++] = xx | ((ss & 7) << 5);
        data[data_len++] = sprite_idx;
      }

      // Check for optional drop type
      if (Yaml_GetSequenceLength(sprite) == 5) {
        YamlNode *drop_node = Yaml_GetSequence(sprite, 4);
        const char *drop_type = Yaml_AsString(drop_node);
        if (drop_type && strcmp(drop_type, "drop_key") == 0) {
          data[data_len++] = 0xfe;
          data[data_len++] = 0;
          data[data_len++] = 0xe4;
        } else if (drop_type && strcmp(drop_type, "drop_big_key") == 0) {
          data[data_len++] = 0xfd;
          data[data_len++] = 0;
          data[data_len++] = 0xe4;
        }
      }

      sprites_encoded++;
    }

    data[data_len++] = 0xff;
    rooms_processed++;
    Yaml_Free(doc);
  }

  printf("  Processed %d rooms, encoded %d sprites\n", rooms_processed, sprites_encoded);
  printf("  Total: %zu bytes\n", data_len);

  // Write C version
  FILE *f = fopen("/tmp/c_dungeon_sprites.bin", "wb");
  if (f) {
    fwrite(data, 1, data_len, f);
    fclose(f);
    printf("  Wrote C sprites: /tmp/c_dungeon_sprites.bin\n");
  }

  f = fopen("/tmp/c_dungeon_offsets.bin", "wb");
  if (f) {
    fwrite(offsets, sizeof(uint16_t), 320, f);
    fclose(f);
    printf("  Wrote C offsets: /tmp/c_dungeon_offsets.bin\n");
  }

  // Print first 32 bytes
  printf("  C [0-31]: ");
  for (size_t i = 0; i < 32 && i < data_len; i++) {
    printf("%02x ", data[i]);
  }
  printf("\n");

  free(offsets);
  free(data);

  printf("\nCompare:\n");
  printf("  diff /tmp/python_dungeon_sprites.bin /tmp/c_dungeon_sprites.bin\n");
  printf("  diff /tmp/python_dungeon_offsets.bin /tmp/c_dungeon_offsets.bin\n");
}

// ============================================================================
// Dungeon Sprites Extraction (ROM-only)
// ============================================================================

// ROM addresses for dungeon sprite data
#define ROM_SPRITE_PTRS   0x89D62E   // Pointer table: 320 entries × 2 bytes
#define ROM_SPRITE_BASE   0x890000   // Base address for sprite data

void ExtractDungeonSprites(AssetBuilder *builder, Rom *rom) {
  printf("  Extracting dungeon sprites from ROM (320 rooms)...\n");

  // Allocate buffers
  uint16_t *offsets = calloc(320, sizeof(uint16_t));
  uint8_t *data = malloc(64 * 1024);  // 64KB max
  size_t data_len = 0;

  // Initialize with [0, 0xff] as the "no sprites" default
  data[data_len++] = 0;
  data[data_len++] = 0xff;

  int rooms_with_sprites = 0;
  int sprites_total = 0;

  // Process all 320 dungeon rooms
  for (int room = 0; room < 320; room++) {
    // Read pointer from table
    uint16_t ptr = Rom_ReadWord(rom, ROM_SPRITE_PTRS + room * 2);
    uint32_t sprite_addr = ROM_SPRITE_BASE + ptr;

    // Read sort_sprites byte
    uint8_t sortmode = Rom_ReadByte(rom, sprite_addr);

    // Count sprites to check if room has any
    uint32_t ea = sprite_addr + 1;
    int sprite_count = 0;
    while (Rom_ReadByte(rom, ea) != 0xFF) {
      sprite_count++;
      ea += 3;
      // Handle special drop markers (0xFE/0xFD, 0x00, 0xE4)
      if (Rom_ReadByte(rom, ea - 3) == 0xFE || Rom_ReadByte(rom, ea - 3) == 0xFD) {
        sprite_count--;  // This was a drop marker, not a sprite
      }
    }

    // Skip if no sprites and sortmode == 0
    if (sprite_count == 0 && sortmode == 0) {
      continue;
    }

    // Set offset for this room
    offsets[room] = data_len;

    // Copy sprite data directly from ROM (including sortmode byte)
    ea = sprite_addr;
    data[data_len++] = Rom_ReadByte(rom, ea++);  // sortmode

    while (1) {
      uint8_t b0 = Rom_ReadByte(rom, ea++);
      if (b0 == 0xFF) {
        data[data_len++] = 0xFF;  // Terminator
        break;
      }
      uint8_t b1 = Rom_ReadByte(rom, ea++);
      uint8_t b2 = Rom_ReadByte(rom, ea++);

      data[data_len++] = b0;
      data[data_len++] = b1;
      data[data_len++] = b2;
      sprites_total++;
    }

    rooms_with_sprites++;
  }

  // Add assets
  AssetBuilder_AddAsset(builder, "kDungeonSprites", ASSET_TYPE_UINT8, data, data_len);
  AssetBuilder_AddAsset(builder, "kDungeonSpriteOffs", ASSET_TYPE_UINT16, (const uint8_t*)offsets, 320*2);

  printf("    Processed %d rooms with sprites, %d sprite entries\n", rooms_with_sprites, sprites_total);
  printf("    kDungeonSprites: %zu bytes\n", data_len);
  printf("    kDungeonSpriteOffs: 320 entries (640 bytes)\n");

  free(offsets);
  free(data);
}

// ============================================================================
// ROM-Based Asset Extraction (no YAML required)
// ============================================================================

void ExtractRomBasedAssets(Rom *rom, AssetBuilder *builder) {
  printf("  Extracting ROM-based assets (32 assets)...\n");

  // Overworld graphics and palettes
  uint8_t *data = Rom_ReadPtr(rom, 0x18c000, 0x4000);
  if (data) AssetBuilder_AddAsset(builder, "kOverworldMapGfx", ASSET_TYPE_UINT8, data, 0x4000);

  uint16_t *words = (uint16_t*)Rom_ReadPtr(rom, 0x8ADB27, 256 * 2);
  if (words) AssetBuilder_AddAsset(builder, "kOverworldMapPaletteData", ASSET_TYPE_UINT16, (const uint8_t*)words, 256 * 2);

  words = (uint16_t*)Rom_ReadPtr(rom, 0x9BD660, 64 * 2);
  if (words) AssetBuilder_AddAsset(builder, "kHudPalData", ASSET_TYPE_UINT16, (const uint8_t*)words, 64*2);

  // Tilemaps
  data = Rom_ReadPtr(rom, 0xac727, 4096);
  if (data) AssetBuilder_AddAsset(builder, "kLightOverworldTilemap", ASSET_TYPE_UINT8, data, 4096);

  data = Rom_ReadPtr(rom, 0xaD727, 1024);
  if (data) AssetBuilder_AddAsset(builder, "kDarkOverworldTilemap", ASSET_TYPE_UINT8, data, 1024);

  // Map16 data
  words = (uint16_t*)Rom_ReadPtr(rom, 0x9B52, 6438 * 2);
  if (words) AssetBuilder_AddAsset(builder, "kPredefinedTileData", ASSET_TYPE_UINT16, (const uint8_t*)words, 6438*2);

  words = (uint16_t*)Rom_ReadPtr(rom, 0x8f8000, 3752 * 4 * 2);
  if (words) AssetBuilder_AddAsset(builder, "kMap16ToMap8", ASSET_TYPE_UINT16, (const uint8_t*)words, 3752*4*2);

  data = Rom_ReadPtr(rom, 0x8E9459, 512);
  if (data) AssetBuilder_AddAsset(builder, "kMap8DataToTileAttr", ASSET_TYPE_UINT8, data, 512);

  data = Rom_ReadPtr(rom, 0x9bf110, 3824);
  if (data) AssetBuilder_AddAsset(builder, "kSomeTileAttr", ASSET_TYPE_UINT8, data, 3824);

  // NOTE: Dungeon attributes (kDungAttrsForTile*, kMovableBlockDataInit, kTorchData*)
  // are now extracted in the dungeon rooms section to match Python ordering

  // Generated data arrays
  data = Rom_ReadPtr(rom, 0x888450, 256);
  if (data) AssetBuilder_AddAsset(builder, "kGeneratedWishPondItem", ASSET_TYPE_UINT8, data, 256);

  data = Rom_ReadPtr(rom, 0x8890FC, 256);
  if (data) AssetBuilder_AddAsset(builder, "kGeneratedBombosArr", ASSET_TYPE_UINT8, data, 256);

  data = Rom_ReadPtr(rom, 0x8ead25, 256);
  if (data) AssetBuilder_AddAsset(builder, "kGeneratedEndSequence15", ASSET_TYPE_UINT8, data, 256);

  // Ending sequence data
  data = Rom_ReadPtr(rom, 0x8EB178, 1989);
  if (data) AssetBuilder_AddAsset(builder, "kEnding_Credits_Text", ASSET_TYPE_UINT8, data, 1989);

  words = (uint16_t*)Rom_ReadPtr(rom, 0x8EB93d, 394 * 2);
  if (words) AssetBuilder_AddAsset(builder, "kEnding_Credits_Offs", ASSET_TYPE_UINT16, (const uint8_t*)words, 394*2);

  words = (uint16_t*)Rom_ReadPtr(rom, 0x8EB038, 160 * 2);
  if (words) AssetBuilder_AddAsset(builder, "kEnding_MapData", ASSET_TYPE_UINT16, (const uint8_t*)words, 160*2);

  words = (uint16_t*)Rom_ReadPtr(rom, 0x8EC2E1, 17 * 2);
  if (words) AssetBuilder_AddAsset(builder, "kEnding0_Offs", ASSET_TYPE_UINT16, (const uint8_t*)words, 17*2);

  data = Rom_ReadPtr(rom, 0x8EBF4C, 917);
  if (data) AssetBuilder_AddAsset(builder, "kEnding0_Data", ASSET_TYPE_UINT8, data, 917);

  // Palettes (many!)
  words = (uint16_t*)Rom_ReadPtr(rom, 0x9BD734, 1800 * 2);
  if (words) AssetBuilder_AddAsset(builder, "kPalette_DungBgMain", ASSET_TYPE_UINT16, (const uint8_t*)words, 1800*2);

  words = (uint16_t*)Rom_ReadPtr(rom, 0x9BD218, 120 * 2);
  if (words) AssetBuilder_AddAsset(builder, "kPalette_MainSpr", ASSET_TYPE_UINT16, (const uint8_t*)words, 120*2);

  words = (uint16_t*)Rom_ReadPtr(rom, 0x9BD308, 75 * 2);
  if (words) AssetBuilder_AddAsset(builder, "kPalette_ArmorAndGloves", ASSET_TYPE_UINT16, (const uint8_t*)words, 75*2);

  words = (uint16_t*)Rom_ReadPtr(rom, 0x9BD630, 12 * 2);
  if (words) AssetBuilder_AddAsset(builder, "kPalette_Sword", ASSET_TYPE_UINT16, (const uint8_t*)words, 12*2);

  words = (uint16_t*)Rom_ReadPtr(rom, 0x9BD648, 12 * 2);
  if (words) AssetBuilder_AddAsset(builder, "kPalette_Shield", ASSET_TYPE_UINT16, (const uint8_t*)words, 12*2);

  words = (uint16_t*)Rom_ReadPtr(rom, 0x9BD39E, 84 * 2);
  if (words) AssetBuilder_AddAsset(builder, "kPalette_SpriteAux3", ASSET_TYPE_UINT16, (const uint8_t*)words, 84*2);

  words = (uint16_t*)Rom_ReadPtr(rom, 0x9BD446, 77 * 2);
  if (words) AssetBuilder_AddAsset(builder, "kPalette_MiscSprite_Indoors", ASSET_TYPE_UINT16, (const uint8_t*)words, 77*2);

  words = (uint16_t*)Rom_ReadPtr(rom, 0x9BD4E0, 126 * 2);
  if (words) AssetBuilder_AddAsset(builder, "kPalette_SpriteAux1", ASSET_TYPE_UINT16, (const uint8_t*)words, 126*2);

  words = (uint16_t*)Rom_ReadPtr(rom, 0x9BD5E0, 84 * 2);
  if (words) AssetBuilder_AddAsset(builder, "kPalette_SpriteAux2", ASSET_TYPE_UINT16, (const uint8_t*)words, 84*2);

  words = (uint16_t*)Rom_ReadPtr(rom, 0x9BD66C, 28 * 2);
  if (words) AssetBuilder_AddAsset(builder, "kPalette_MiscSprite_Outdoors", ASSET_TYPE_UINT16, (const uint8_t*)words, 28*2);

  printf("    Extracted 32 ROM-based assets\n");
}

// ============================================================================
// Dungeon Secrets Extraction (ROM-only)
// ============================================================================

// ROM addresses for dungeon secrets
#define ROM_SECRETS_PTRS  0x81DB69   // Pointer table: 320 entries × 2 bytes
#define ROM_SECRETS_BASE  0x810000   // Base address for secret data

void ExtractDungeonSecrets(AssetBuilder *builder, Rom *rom) {
  printf("  Extracting dungeon secrets from ROM (320 rooms)...\n");

  // Allocate result array: 320 rooms × 2 bytes offset each = 640 bytes
  uint8_t *result = calloc(640, 1);
  uint8_t *data = malloc(64 * 1024);  // Secret data (64KB max)
  size_t data_len = 0;

  int secrets_found = 0;
  int rooms_with_secrets = 0;

  // Process all 320 dungeon rooms
  for (int i = 0; i < 320; i++) {
    // Read pointer from table
    uint16_t ptr = Rom_ReadWord(rom, ROM_SECRETS_PTRS + i * 2);
    uint32_t secret_addr = ROM_SECRETS_BASE | ptr;

    // Check if this room has secrets (not terminated immediately)
    uint16_t first = Rom_ReadWord(rom, secret_addr);
    if (first == 0xFFFF) {
      continue;  // No secrets in this room
    }

    // Set offset for this room (640 + data_len because offsets are relative to combined buffer)
    uint16_t offset = 640 + data_len;
    result[i * 2 + 0] = offset & 0xff;
    result[i * 2 + 1] = (offset >> 8) & 0xff;

    // Copy secret data directly from ROM until 0xFFFF terminator
    uint32_t ea = secret_addr;
    while (1) {
      uint16_t pos = Rom_ReadWord(rom, ea);
      if (pos == 0xFFFF) {
        // Append terminator
        data[data_len++] = 0xff;
        data[data_len++] = 0xff;
        break;
      }

      uint8_t secret_idx = Rom_ReadByte(rom, ea + 2);

      // Copy 3 bytes: [pos_lo, pos_hi, secret_idx]
      data[data_len++] = pos & 0xff;
      data[data_len++] = (pos >> 8) & 0xff;
      data[data_len++] = secret_idx;

      secrets_found++;
      ea += 3;
    }

    rooms_with_secrets++;
  }

  // For rooms with no secrets, set offset to point to end of data (last terminator)
  // We need to add a final terminator for empty rooms to point to
  data[data_len++] = 0xff;
  data[data_len++] = 0xff;
  uint16_t empty_offset = 640 + data_len - 2;

  for (int i = 0; i < 320; i++) {
    // Check if offset is still 0 (room has no secrets)
    if (result[i * 2 + 0] == 0 && result[i * 2 + 1] == 0) {
      result[i * 2 + 0] = empty_offset & 0xff;
      result[i * 2 + 1] = (empty_offset >> 8) & 0xff;
    }
  }

  // Combine offset table and data into single asset
  uint8_t *combined = malloc(640 + data_len);
  memcpy(combined, result, 640);
  memcpy(combined + 640, data, data_len);

  AssetBuilder_AddAsset(builder, "kDungeonSecrets", ASSET_TYPE_UINT8, combined, 640 + data_len);

  printf("    Processed %d rooms with secrets, found %d secrets\n", rooms_with_secrets, secrets_found);
  printf("    kDungeonSecrets: %zu bytes (640 offsets + %zu data)\n",
         640 + data_len, data_len);

  free(result);
  free(data);
  free(combined);
}

// ============================================================================
// Deduplication Helper
// ============================================================================

// Scan for overlapping bytes between end of 'big' and start of 'little'
// Appends only non-overlapping bytes and returns offset into 'big'
static size_t AppendScanBytes(uint8_t **big_ptr, size_t *big_len, size_t *big_cap,
                              const uint8_t *little, size_t little_len) {
  uint8_t *big = *big_ptr;

  // Try to find overlap, starting from full overlap down to no overlap
  for (int n = little_len; n >= 0; n--) {
    // Check if last n bytes of big match first n bytes of little
    bool match = true;
    if (n > 0 && *big_len >= (size_t)n) {
      for (int i = 0; i < n; i++) {
        if (big[*big_len - n + i] != little[i]) {
          match = false;
          break;
        }
      }
    } else if (n > 0) {
      match = false;
    }

    if (n == 0 || match) {
      size_t offset = *big_len - n;
      size_t append_len = little_len - n;

      // Ensure capacity
      if (*big_len + append_len > *big_cap) {
        *big_cap = (*big_len + append_len) * 2;
        *big_ptr = realloc(*big_ptr, *big_cap);
        big = *big_ptr;
      }

      // Append non-overlapping bytes
      memcpy(big + *big_len, little + n, append_len);
      *big_len += append_len;

      return offset;
    }
  }

  return 0;  // Should never reach here
}

// ============================================================================
// Dungeon Room Headers
// ============================================================================

// ROM addresses for dungeon room headers
#define ROM_ROOM_HEADER_PTRS 0x84F502   // Room metadata pointer table (320 * 2 bytes)

void ExtractDungeonRoomHeaders(AssetBuilder *builder, Rom *rom) {
  printf("  Extracting dungeon room headers from ROM (320 rooms)...\n");

  // Room headers data (deduplicated)
  uint8_t *headers = malloc(64 * 1024);
  size_t headers_len = 0;
  size_t headers_cap = 64 * 1024;

  // Offset for each room's header
  uint16_t *header_offsets = malloc(320 * sizeof(uint16_t));

  // Process all 320 dungeon rooms
  for (int i = 0; i < 320; i++) {
    // Get pointer to room metadata
    uint16_t ptr = Rom_ReadWord(rom, ROM_ROOM_HEADER_PTRS + i * 2);
    uint32_t header_addr;

    if (ptr == 0xFFEF) {
      // Invalid room - use zeros
      header_addr = 0x82EDC5;  // Points to zeros in ROM
    } else {
      header_addr = 0x840000 | ptr;  // Bank $84
    }

    // Read 14-byte header directly from ROM
    uint8_t header[14];
    for (int j = 0; j < 14; j++) {
      header[j] = Rom_ReadByte(rom, header_addr + j);
    }

    // Deduplicate and store offset
    header_offsets[i] = AppendScanBytes(&headers, &headers_len, &headers_cap, header, 14);
  }

  // Add assets
  AssetBuilder_AddAsset(builder, "kDungeonRoomHeaders", ASSET_TYPE_UINT8,
                       headers, headers_len);
  AssetBuilder_AddAsset(builder, "kDungeonRoomHeadersOffs", ASSET_TYPE_UINT16,
                       (uint8_t*)header_offsets, 320*2);

  printf("    kDungeonRoomHeaders: %zu bytes (deduplicated from %d headers)\n",
         headers_len, 320);
  printf("    kDungeonRoomHeadersOffs: 320 entries (640 bytes)\n");

  free(headers);
  free(header_offsets);
}

// ============================================================================
// Simple Dungeon Room Assets
// ============================================================================

// ROM addresses for simple dungeon data
#define ROM_CHEST_DATA       0x81E96E   // 168 entries × 3 bytes
#define ROM_CHEST_COUNT      168
#define ROM_TELEMSG_DATA     0x87F61D   // 320 entries × 2 bytes
#define ROM_PITS_HURT_DATA   0x80990C   // 57 entries × 2 bytes
#define ROM_PITS_HURT_COUNT  57

void ExtractDungeonRoomSimple(AssetBuilder *builder, Rom *rom) {
  printf("  Extracting simple dungeon room data from ROM...\n");

  // kDungeonRoomChests: Copy 168 entries directly from ROM
  uint8_t *chests = malloc(ROM_CHEST_COUNT * 3);
  for (int i = 0; i < ROM_CHEST_COUNT * 3; i++) {
    chests[i] = Rom_ReadByte(rom, ROM_CHEST_DATA + i);
  }

  // kDungeonRoomTeleMsg: Read 320 entries from ROM
  uint16_t *sign_texts = malloc(320 * sizeof(uint16_t));
  for (int i = 0; i < 320; i++) {
    sign_texts[i] = Rom_ReadWord(rom, ROM_TELEMSG_DATA + i * 2);
  }

  // kDungeonPitsHurtPlayer: Read 57 room IDs from ROM
  uint16_t *pits_rooms = malloc(ROM_PITS_HURT_COUNT * sizeof(uint16_t));
  for (int i = 0; i < ROM_PITS_HURT_COUNT; i++) {
    pits_rooms[i] = Rom_ReadWord(rom, ROM_PITS_HURT_DATA + i * 2);
  }

  // Add assets
  AssetBuilder_AddAsset(builder, "kDungeonRoomChests", ASSET_TYPE_UINT8,
                       chests, ROM_CHEST_COUNT * 3);
  AssetBuilder_AddAsset(builder, "kDungeonRoomTeleMsg", ASSET_TYPE_UINT16,
                       (uint8_t*)sign_texts, 320 * 2);
  AssetBuilder_AddAsset(builder, "kDungeonPitsHurtPlayer", ASSET_TYPE_UINT16,
                       (uint8_t*)pits_rooms, ROM_PITS_HURT_COUNT * 2);

  printf("    kDungeonRoomChests: %d chests (%d bytes)\n",
         ROM_CHEST_COUNT, ROM_CHEST_COUNT * 3);
  printf("    kDungeonRoomTeleMsg: 320 entries (640 bytes)\n");
  printf("    kDungeonPitsHurtPlayer: %d rooms where pits hurt (%d bytes)\n",
         ROM_PITS_HURT_COUNT, ROM_PITS_HURT_COUNT * 2);

  free(sign_texts);
  free(pits_rooms);
  free(chests);
}

// ============================================================================
// Dungeon Room Data (3-layer object encoding)
// ============================================================================

// ROM addresses for dungeon room data
#define ROM_ROOM_POINTERS 0x9F8000    // 3-byte pointers for 320 rooms

// Helper to read one layer from ROM (objects + optional doors)
// Returns the ROM offset after this layer, sets *door_off to door offset (or 0)
static uint32_t ReadRoomLayerFromROM(Rom *rom, uint32_t addr,
                                      uint8_t *out, size_t *out_len, size_t out_cap,
                                      uint16_t *door_off) {
  *door_off = 0;

  while (true) {
    uint16_t word = Rom_ReadWord(rom, addr);

    // End of layer (no doors)
    if (word == 0xFFFF) {
      if (*out_len + 2 <= out_cap) {
        out[(*out_len)++] = 0xFF;
        out[(*out_len)++] = 0xFF;
      }
      return addr + 2;
    }

    // Door marker
    if (word == 0xFFF0) {
      if (*out_len + 2 <= out_cap) {
        out[(*out_len)++] = 0xF0;
        out[(*out_len)++] = 0xFF;
      }
      addr += 2;
      *door_off = *out_len;

      // Read doors until 0xFFFF
      while (true) {
        uint16_t dword = Rom_ReadWord(rom, addr);
        if (dword == 0xFFFF) {
          if (*out_len + 2 <= out_cap) {
            out[(*out_len)++] = 0xFF;
            out[(*out_len)++] = 0xFF;
          }
          return addr + 2;
        }
        // Copy door data
        if (*out_len + 2 <= out_cap) {
          out[(*out_len)++] = dword & 0xFF;
          out[(*out_len)++] = (dword >> 8) & 0xFF;
        }
        addr += 2;
      }
    }

    // Regular object (3 bytes)
    if (*out_len + 3 <= out_cap) {
      out[(*out_len)++] = word & 0xFF;
      out[(*out_len)++] = (word >> 8) & 0xFF;
      out[(*out_len)++] = Rom_ReadByte(rom, addr + 2);
    }
    addr += 3;
  }
}

// Extract dungeon room data directly from ROM
static bool ExtractDungeonRoomDataFromROM(AssetBuilder *builder, Rom *rom) {
  printf("  Extracting dungeon room data from ROM (320 rooms)...\n");

  // Buffers
  uint8_t *room_data = malloc(512 * 1024);
  size_t room_data_len = 0;
  size_t room_data_cap = 512 * 1024;

  uint16_t *room_offsets = malloc(320 * sizeof(uint16_t));
  uint16_t *door_offsets = malloc(320 * sizeof(uint16_t));

  uint8_t *temp_room = malloc(64 * 1024);
  size_t temp_cap = 64 * 1024;

  if (!room_data || !room_offsets || !door_offsets || !temp_room) {
    LogError("Failed to allocate dungeon room buffers");
    free(room_data);
    free(room_offsets);
    free(door_offsets);
    free(temp_room);
    return false;
  }

  for (int i = 0; i < 320; i++) {
    // Read 3-byte pointer for this room
    uint32_t ptr_addr = ROM_ROOM_POINTERS + i * 3;
    uint32_t room_addr = Rom_ReadByte(rom, ptr_addr) |
                         (Rom_ReadByte(rom, ptr_addr + 1) << 8) |
                         (Rom_ReadByte(rom, ptr_addr + 2) << 16);

    size_t temp_len = 0;
    uint16_t door_off = 0;

    // Byte 0: floor1 + floor2 * 16
    // Byte 1: layout * 4 + start_quadrant
    temp_room[temp_len++] = Rom_ReadByte(rom, room_addr);
    temp_room[temp_len++] = Rom_ReadByte(rom, room_addr + 1);

    uint32_t addr = room_addr + 2;

    // Layer 1
    uint16_t layer_door_off;
    addr = ReadRoomLayerFromROM(rom, addr, temp_room, &temp_len, temp_cap, &layer_door_off);

    // Layer 2
    addr = ReadRoomLayerFromROM(rom, addr, temp_room, &temp_len, temp_cap, &layer_door_off);

    // Layer 3 - this is where doors are (if any)
    addr = ReadRoomLayerFromROM(rom, addr, temp_room, &temp_len, temp_cap, &layer_door_off);
    if (layer_door_off) {
      door_off = layer_door_off;
    }

    // Record offset for this room
    room_offsets[i] = room_data_len;

    // Ensure capacity and copy
    if (room_data_len + temp_len > room_data_cap) {
      room_data_cap = (room_data_len + temp_len) * 2;
      room_data = realloc(room_data, room_data_cap);
    }
    memcpy(room_data + room_data_len, temp_room, temp_len);

    door_offsets[i] = door_off ? (room_offsets[i] + door_off) : 0;
    room_data_len += temp_len;
  }

  // Add assets
  AssetBuilder_AddAsset(builder, "kDungeonRoom", ASSET_TYPE_UINT8, room_data, room_data_len);
  AssetBuilder_AddAsset(builder, "kDungeonRoomOffs", ASSET_TYPE_UINT16,
                        (uint8_t*)room_offsets, 320*2);
  AssetBuilder_AddAsset(builder, "kDungeonRoomDoorOffs", ASSET_TYPE_UINT16,
                        (uint8_t*)door_offsets, 320*2);

  printf("    kDungeonRoom: %zu bytes from 320 rooms (from ROM)\n", room_data_len);
  printf("    kDungeonRoomOffs: 320 entries (640 bytes)\n");
  printf("    kDungeonRoomDoorOffs: 320 entries (640 bytes)\n");

  free(room_data);
  free(room_offsets);
  free(door_offsets);
  free(temp_room);
  return true;
}

void ExtractDungeonRoomData(AssetBuilder *builder, Rom *rom) {
  // Extract directly from ROM - this is the only supported method
  if (!ExtractDungeonRoomDataFromROM(builder, rom)) {
    LogError("Failed to extract dungeon room data from ROM");
  }
}

// ============================================================================
// Default and Overlay Dungeon Rooms
// ============================================================================

// ROM addresses for default/overlay room pointers
#define ROM_DEFAULT_ROOM_PTRS  0x84EF2F   // 8 entries × 3-byte pointers
#define ROM_OVERLAY_ROOM_PTRS  0x84ECC0   // 19 entries × 3-byte pointers
#define ROM_DEFAULT_ROOM_COUNT 8
#define ROM_OVERLAY_ROOM_COUNT 19

// Read room object data from ROM (no doors) - returns next address after terminator
static uint32_t ReadRoomObjectsFromROM(Rom *rom, uint32_t addr, uint8_t **out, size_t *len, size_t *cap) {
  // Read room objects until 0xFFFF terminator
  // Same format as regular room layer data
  while (1) {
    uint16_t v = Rom_ReadWord(rom, addr);

    // Check for terminator
    if (v == 0xFFFF) {
      // Add terminator to output
      if (*len + 2 > *cap) {
        *cap *= 2;
        *out = realloc(*out, *cap);
      }
      (*out)[(*len)++] = 0xFF;
      (*out)[(*len)++] = 0xFF;
      return addr + 2;
    }

    // Parse object encoding (same as dungeon rooms)
    uint8_t b0 = v & 0xFF;
    uint8_t b1 = (v >> 8) & 0xFF;

    if ((b0 & 0xFC) == 0xFC) {
      // Subtype 1/2: 3 bytes
      uint8_t b2 = Rom_ReadByte(rom, addr + 2);
      if (*len + 3 > *cap) {
        *cap *= 2;
        *out = realloc(*out, *cap);
      }
      (*out)[(*len)++] = b0;
      (*out)[(*len)++] = b1;
      (*out)[(*len)++] = b2;
      addr += 3;
    } else {
      // Subtype 0: 3 bytes
      uint8_t b2 = Rom_ReadByte(rom, addr + 2);
      if (*len + 3 > *cap) {
        *cap *= 2;
        *out = realloc(*out, *cap);
      }
      (*out)[(*len)++] = b0;
      (*out)[(*len)++] = b1;
      (*out)[(*len)++] = b2;
      addr += 3;
    }
  }
}

void ExtractDefaultOverlayRooms(AssetBuilder *builder, Rom *rom) {
  printf("  Extracting default and overlay rooms from ROM...\n");

  // Default rooms: 8 variants
  uint8_t *default_data = malloc(64 * 1024);
  size_t default_len = 0;
  size_t default_cap = 64 * 1024;
  uint16_t *default_offsets = malloc(ROM_DEFAULT_ROOM_COUNT * sizeof(uint16_t));

  for (int i = 0; i < ROM_DEFAULT_ROOM_COUNT; i++) {
    // Read 3-byte pointer
    uint32_t ptr_addr = ROM_DEFAULT_ROOM_PTRS + i * 3;
    uint32_t room_addr = Rom_ReadByte(rom, ptr_addr) |
                         (Rom_ReadByte(rom, ptr_addr + 1) << 8) |
                         (Rom_ReadByte(rom, ptr_addr + 2) << 16);

    default_offsets[i] = default_len;
    ReadRoomObjectsFromROM(rom, room_addr, &default_data, &default_len, &default_cap);
  }

  AssetBuilder_AddAsset(builder, "kDungeonRoomDefault", ASSET_TYPE_UINT8,
                       default_data, default_len);
  AssetBuilder_AddAsset(builder, "kDungeonRoomDefaultOffs", ASSET_TYPE_UINT16,
                       (uint8_t*)default_offsets, ROM_DEFAULT_ROOM_COUNT * sizeof(uint16_t));

  printf("    kDungeonRoomDefault: %zu bytes from %d variants\n", default_len, ROM_DEFAULT_ROOM_COUNT);
  printf("    kDungeonRoomDefaultOffs: %d entries (%zu bytes)\n",
         ROM_DEFAULT_ROOM_COUNT, ROM_DEFAULT_ROOM_COUNT * sizeof(uint16_t));

  free(default_data);
  free(default_offsets);

  // Overlay rooms: 19 variants
  uint8_t *overlay_data = malloc(64 * 1024);
  size_t overlay_len = 0;
  size_t overlay_cap = 64 * 1024;
  uint16_t *overlay_offsets = malloc(ROM_OVERLAY_ROOM_COUNT * sizeof(uint16_t));

  for (int i = 0; i < ROM_OVERLAY_ROOM_COUNT; i++) {
    // Read 3-byte pointer
    uint32_t ptr_addr = ROM_OVERLAY_ROOM_PTRS + i * 3;
    uint32_t room_addr = Rom_ReadByte(rom, ptr_addr) |
                         (Rom_ReadByte(rom, ptr_addr + 1) << 8) |
                         (Rom_ReadByte(rom, ptr_addr + 2) << 16);

    overlay_offsets[i] = overlay_len;
    ReadRoomObjectsFromROM(rom, room_addr, &overlay_data, &overlay_len, &overlay_cap);
  }

  AssetBuilder_AddAsset(builder, "kDungeonRoomOverlay", ASSET_TYPE_UINT8,
                       overlay_data, overlay_len);
  AssetBuilder_AddAsset(builder, "kDungeonRoomOverlayOffs", ASSET_TYPE_UINT16,
                       (uint8_t*)overlay_offsets, ROM_OVERLAY_ROOM_COUNT * sizeof(uint16_t));

  printf("    kDungeonRoomOverlay: %zu bytes from %d variants\n", overlay_len, ROM_OVERLAY_ROOM_COUNT);
  printf("    kDungeonRoomOverlayOffs: %d entries (%zu bytes)\n",
         ROM_OVERLAY_ROOM_COUNT, ROM_OVERLAY_ROOM_COUNT * sizeof(uint16_t));

  free(overlay_data);
  free(overlay_offsets);
}
// ROM addresses for entrance data (133 entries each)
#define ROM_ENT_ROOM      0x82C813
#define ROM_ENT_RELCOORD  0x82C91D  // 8 bytes each
#define ROM_ENT_SCROLLX   0x82CD45
#define ROM_ENT_SCROLLY   0x82CE4F
#define ROM_ENT_PLAYERY   0x82CF59
#define ROM_ENT_PLAYERX   0x82D063
#define ROM_ENT_CAMERAY   0x82D16D
#define ROM_ENT_CAMERAX   0x82D277
#define ROM_ENT_BLOCKSET  0x82D381
#define ROM_ENT_FLOOR     0x82D406
#define ROM_ENT_PALACE    0x82D48B
#define ROM_ENT_DOORWAY   0x82D510
#define ROM_ENT_PLANELAD  0x82D595
#define ROM_ENT_QUADFLAG  0x82D61A
#define ROM_ENT_QUADSTART 0x82D69F
#define ROM_ENT_EXITDOOR  0x82D724
#define ROM_ENT_MUSIC     0x82D82E
#define ROM_ENT_COUNT     133

// ROM addresses for starting point data (7 entries each)
#define ROM_SP_ROOM       0x82DB6E
#define ROM_SP_RELCOORD   0x82DB7C
#define ROM_SP_SCROLLX    0x82DBB4
#define ROM_SP_SCROLLY    0x82DBC2
#define ROM_SP_PLAYERY    0x82DBD0
#define ROM_SP_PLAYERX    0x82DBDE
#define ROM_SP_CAMERAY    0x82DBEC
#define ROM_SP_CAMERAX    0x82DBFA
#define ROM_SP_BLOCKSET   0x82DC08
#define ROM_SP_FLOOR      0x82DC0F
#define ROM_SP_PALACE     0x82DC16
#define ROM_SP_PLANELAD   0x82DC1D
#define ROM_SP_QUADFLAG   0x82DC24
#define ROM_SP_QUADSTART  0x82DC2B
#define ROM_SP_EXITDOOR   0x82DC32
#define ROM_SP_ASSOCENT   0x82DC40
#define ROM_SP_MUSIC      0x82DC4E
#define ROM_SP_COUNT      7

// Helper to read ROM array into buffer
static void ReadRomArray16(Rom *rom, uint32_t addr, uint16_t *out, int count) {
  for (int i = 0; i < count; i++) out[i] = Rom_ReadWord(rom, addr + i * 2);
}
static void ReadRomArray8(Rom *rom, uint32_t addr, uint8_t *out, int count) {
  for (int i = 0; i < count; i++) out[i] = Rom_ReadByte(rom, addr + i);
}

void ExtractEntrancesAndStartingPoints(AssetBuilder *builder, Rom *rom) {
  printf("  Extracting entrances and starting points from ROM...\n");

  // Allocate buffers for entrance data (133 entries)
  uint16_t *e_rooms = malloc(ROM_ENT_COUNT * 2);
  uint8_t *e_rel = malloc(ROM_ENT_COUNT * 8);
  uint16_t *e_sx = malloc(ROM_ENT_COUNT * 2), *e_sy = malloc(ROM_ENT_COUNT * 2);
  uint16_t *e_px = malloc(ROM_ENT_COUNT * 2), *e_py = malloc(ROM_ENT_COUNT * 2);
  uint16_t *e_cx = malloc(ROM_ENT_COUNT * 2), *e_cy = malloc(ROM_ENT_COUNT * 2);
  uint8_t *e_blk = malloc(ROM_ENT_COUNT), *e_dor = malloc(ROM_ENT_COUNT);
  uint8_t *e_bg = malloc(ROM_ENT_COUNT), *e_q1 = malloc(ROM_ENT_COUNT);
  uint8_t *e_q2 = malloc(ROM_ENT_COUNT), *e_mus = malloc(ROM_ENT_COUNT);
  int8_t *e_flr = malloc(ROM_ENT_COUNT), *e_pal = malloc(ROM_ENT_COUNT);
  uint16_t *e_door = malloc(ROM_ENT_COUNT * 2);

  // Read entrance data directly from ROM
  ReadRomArray16(rom, ROM_ENT_ROOM, e_rooms, ROM_ENT_COUNT);
  for (int i = 0; i < ROM_ENT_COUNT; i++)
    for (int j = 0; j < 8; j++)
      e_rel[i*8+j] = Rom_ReadByte(rom, ROM_ENT_RELCOORD + i*8 + j);
  ReadRomArray16(rom, ROM_ENT_SCROLLX, e_sx, ROM_ENT_COUNT);
  ReadRomArray16(rom, ROM_ENT_SCROLLY, e_sy, ROM_ENT_COUNT);
  ReadRomArray16(rom, ROM_ENT_PLAYERX, e_px, ROM_ENT_COUNT);
  ReadRomArray16(rom, ROM_ENT_PLAYERY, e_py, ROM_ENT_COUNT);
  ReadRomArray16(rom, ROM_ENT_CAMERAX, e_cx, ROM_ENT_COUNT);
  ReadRomArray16(rom, ROM_ENT_CAMERAY, e_cy, ROM_ENT_COUNT);
  ReadRomArray8(rom, ROM_ENT_BLOCKSET, e_blk, ROM_ENT_COUNT);
  ReadRomArray8(rom, ROM_ENT_FLOOR, (uint8_t*)e_flr, ROM_ENT_COUNT);
  ReadRomArray8(rom, ROM_ENT_PALACE, (uint8_t*)e_pal, ROM_ENT_COUNT);
  ReadRomArray8(rom, ROM_ENT_DOORWAY, e_dor, ROM_ENT_COUNT);
  ReadRomArray8(rom, ROM_ENT_PLANELAD, e_bg, ROM_ENT_COUNT);
  ReadRomArray8(rom, ROM_ENT_QUADFLAG, e_q1, ROM_ENT_COUNT);
  ReadRomArray8(rom, ROM_ENT_QUADSTART, e_q2, ROM_ENT_COUNT);
  ReadRomArray16(rom, ROM_ENT_EXITDOOR, e_door, ROM_ENT_COUNT);
  ReadRomArray8(rom, ROM_ENT_MUSIC, e_mus, ROM_ENT_COUNT);

  // Add entrance assets
  AssetBuilder_AddAsset(builder, "kEntranceData_rooms", ASSET_TYPE_UINT16, (uint8_t*)e_rooms, ROM_ENT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kEntranceData_relativeCoords", ASSET_TYPE_UINT8, e_rel, ROM_ENT_COUNT*8);
  AssetBuilder_AddAsset(builder, "kEntranceData_scrollX", ASSET_TYPE_UINT16, (uint8_t*)e_sx, ROM_ENT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kEntranceData_scrollY", ASSET_TYPE_UINT16, (uint8_t*)e_sy, ROM_ENT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kEntranceData_playerX", ASSET_TYPE_UINT16, (uint8_t*)e_px, ROM_ENT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kEntranceData_playerY", ASSET_TYPE_UINT16, (uint8_t*)e_py, ROM_ENT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kEntranceData_cameraX", ASSET_TYPE_UINT16, (uint8_t*)e_cx, ROM_ENT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kEntranceData_cameraY", ASSET_TYPE_UINT16, (uint8_t*)e_cy, ROM_ENT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kEntranceData_blockset", ASSET_TYPE_UINT8, e_blk, ROM_ENT_COUNT);
  AssetBuilder_AddAsset(builder, "kEntranceData_floor", ASSET_TYPE_INT8, (uint8_t*)e_flr, ROM_ENT_COUNT);
  AssetBuilder_AddAsset(builder, "kEntranceData_palace", ASSET_TYPE_INT8, (uint8_t*)e_pal, ROM_ENT_COUNT);
  AssetBuilder_AddAsset(builder, "kEntranceData_doorwayOrientation", ASSET_TYPE_UINT8, e_dor, ROM_ENT_COUNT);
  AssetBuilder_AddAsset(builder, "kEntranceData_startingBg", ASSET_TYPE_UINT8, e_bg, ROM_ENT_COUNT);
  AssetBuilder_AddAsset(builder, "kEntranceData_quadrant1", ASSET_TYPE_UINT8, e_q1, ROM_ENT_COUNT);
  AssetBuilder_AddAsset(builder, "kEntranceData_quadrant2", ASSET_TYPE_UINT8, e_q2, ROM_ENT_COUNT);
  AssetBuilder_AddAsset(builder, "kEntranceData_doorSettings", ASSET_TYPE_UINT16, (uint8_t*)e_door, ROM_ENT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kEntranceData_musicTrack", ASSET_TYPE_UINT8, e_mus, ROM_ENT_COUNT);

  free(e_rooms); free(e_rel); free(e_sx); free(e_sy); free(e_px); free(e_py);
  free(e_cx); free(e_cy); free(e_blk); free(e_flr); free(e_pal); free(e_dor);
  free(e_bg); free(e_q1); free(e_q2); free(e_door); free(e_mus);

  printf("    Added 16 entrance assets (%d entries each)\n", ROM_ENT_COUNT);

  // Allocate buffers for starting point data (7 entries)
  uint16_t *s_rooms = malloc(ROM_SP_COUNT * 2);
  uint8_t *s_rel = malloc(ROM_SP_COUNT * 8);
  uint16_t *s_sx = malloc(ROM_SP_COUNT * 2), *s_sy = malloc(ROM_SP_COUNT * 2);
  uint16_t *s_px = malloc(ROM_SP_COUNT * 2), *s_py = malloc(ROM_SP_COUNT * 2);
  uint16_t *s_cx = malloc(ROM_SP_COUNT * 2), *s_cy = malloc(ROM_SP_COUNT * 2);
  uint8_t *s_blk = malloc(ROM_SP_COUNT), *s_dor = malloc(ROM_SP_COUNT);
  uint8_t *s_bg = malloc(ROM_SP_COUNT), *s_q1 = malloc(ROM_SP_COUNT);
  uint8_t *s_q2 = malloc(ROM_SP_COUNT), *s_mus = malloc(ROM_SP_COUNT);
  uint8_t *s_ent = malloc(ROM_SP_COUNT);
  int8_t *s_flr = malloc(ROM_SP_COUNT), *s_pal = malloc(ROM_SP_COUNT);
  uint16_t *s_door = malloc(ROM_SP_COUNT * 2);

  // Read starting point data directly from ROM
  ReadRomArray16(rom, ROM_SP_ROOM, s_rooms, ROM_SP_COUNT);
  for (int i = 0; i < ROM_SP_COUNT; i++)
    for (int j = 0; j < 8; j++)
      s_rel[i*8+j] = Rom_ReadByte(rom, ROM_SP_RELCOORD + i*8 + j);
  ReadRomArray16(rom, ROM_SP_SCROLLX, s_sx, ROM_SP_COUNT);
  ReadRomArray16(rom, ROM_SP_SCROLLY, s_sy, ROM_SP_COUNT);
  ReadRomArray16(rom, ROM_SP_PLAYERX, s_px, ROM_SP_COUNT);
  ReadRomArray16(rom, ROM_SP_PLAYERY, s_py, ROM_SP_COUNT);
  ReadRomArray16(rom, ROM_SP_CAMERAX, s_cx, ROM_SP_COUNT);
  ReadRomArray16(rom, ROM_SP_CAMERAY, s_cy, ROM_SP_COUNT);
  ReadRomArray8(rom, ROM_SP_BLOCKSET, s_blk, ROM_SP_COUNT);
  ReadRomArray8(rom, ROM_SP_FLOOR, (uint8_t*)s_flr, ROM_SP_COUNT);
  ReadRomArray8(rom, ROM_SP_PALACE, (uint8_t*)s_pal, ROM_SP_COUNT);
  memset(s_dor, 0, ROM_SP_COUNT);  // Starting points don't have doorway orientation
  ReadRomArray8(rom, ROM_SP_PLANELAD, s_bg, ROM_SP_COUNT);
  ReadRomArray8(rom, ROM_SP_QUADFLAG, s_q1, ROM_SP_COUNT);
  ReadRomArray8(rom, ROM_SP_QUADSTART, s_q2, ROM_SP_COUNT);
  ReadRomArray16(rom, ROM_SP_EXITDOOR, s_door, ROM_SP_COUNT);
  ReadRomArray16(rom, ROM_SP_ASSOCENT, (uint16_t*)s_ent, ROM_SP_COUNT);  // Actually word values
  ReadRomArray8(rom, ROM_SP_MUSIC, s_mus, ROM_SP_COUNT);

  // Add starting point assets
  AssetBuilder_AddAsset(builder, "kStartingPoint_rooms", ASSET_TYPE_UINT16, (uint8_t*)s_rooms, ROM_SP_COUNT*2);
  AssetBuilder_AddAsset(builder, "kStartingPoint_relativeCoords", ASSET_TYPE_UINT8, s_rel, ROM_SP_COUNT*8);
  AssetBuilder_AddAsset(builder, "kStartingPoint_scrollX", ASSET_TYPE_UINT16, (uint8_t*)s_sx, ROM_SP_COUNT*2);
  AssetBuilder_AddAsset(builder, "kStartingPoint_scrollY", ASSET_TYPE_UINT16, (uint8_t*)s_sy, ROM_SP_COUNT*2);
  AssetBuilder_AddAsset(builder, "kStartingPoint_playerX", ASSET_TYPE_UINT16, (uint8_t*)s_px, ROM_SP_COUNT*2);
  AssetBuilder_AddAsset(builder, "kStartingPoint_playerY", ASSET_TYPE_UINT16, (uint8_t*)s_py, ROM_SP_COUNT*2);
  AssetBuilder_AddAsset(builder, "kStartingPoint_cameraX", ASSET_TYPE_UINT16, (uint8_t*)s_cx, ROM_SP_COUNT*2);
  AssetBuilder_AddAsset(builder, "kStartingPoint_cameraY", ASSET_TYPE_UINT16, (uint8_t*)s_cy, ROM_SP_COUNT*2);
  AssetBuilder_AddAsset(builder, "kStartingPoint_blockset", ASSET_TYPE_UINT8, s_blk, ROM_SP_COUNT);
  AssetBuilder_AddAsset(builder, "kStartingPoint_floor", ASSET_TYPE_INT8, (uint8_t*)s_flr, ROM_SP_COUNT);
  AssetBuilder_AddAsset(builder, "kStartingPoint_palace", ASSET_TYPE_INT8, (uint8_t*)s_pal, ROM_SP_COUNT);
  AssetBuilder_AddAsset(builder, "kStartingPoint_doorwayOrientation", ASSET_TYPE_UINT8, s_dor, ROM_SP_COUNT);
  AssetBuilder_AddAsset(builder, "kStartingPoint_startingBg", ASSET_TYPE_UINT8, s_bg, ROM_SP_COUNT);
  AssetBuilder_AddAsset(builder, "kStartingPoint_quadrant1", ASSET_TYPE_UINT8, s_q1, ROM_SP_COUNT);
  AssetBuilder_AddAsset(builder, "kStartingPoint_quadrant2", ASSET_TYPE_UINT8, s_q2, ROM_SP_COUNT);
  AssetBuilder_AddAsset(builder, "kStartingPoint_doorSettings", ASSET_TYPE_UINT16, (uint8_t*)s_door, ROM_SP_COUNT*2);
  AssetBuilder_AddAsset(builder, "kStartingPoint_entrance", ASSET_TYPE_UINT8, s_ent, ROM_SP_COUNT);
  AssetBuilder_AddAsset(builder, "kStartingPoint_musicTrack", ASSET_TYPE_UINT8, s_mus, ROM_SP_COUNT);

  free(s_rooms); free(s_rel); free(s_sx); free(s_sy); free(s_px); free(s_py);
  free(s_cx); free(s_cy); free(s_blk); free(s_flr); free(s_pal); free(s_dor);
  free(s_bg); free(s_q1); free(s_q2); free(s_door); free(s_ent); free(s_mus);

  printf("    Added 17 starting point assets (%d entries each)\n", ROM_SP_COUNT);
}

// ============================================================================
// Overworld YAML Extraction - Helper Types and Functions
// ============================================================================

// Helper struct for hole sorting (Phase 4)
typedef struct {
  uint8_t entrance_id;
  uint16_t pos;
  uint16_t area;
} Overworld_Hole;

// Check if area is a "head" area (not a sub-area)
static bool Overworld_IsAreaHead(Rom *rom, int i) {
  if (i >= 128) return true;
  uint8_t parent = Rom_ReadByte(rom, 0x82A5EC + (i & 63));
  return parent == (i & 63);
}

// ============================================================================
// Overworld Compressed Data Extraction
// ============================================================================

// Extract kOverworld_Hibytes_Comp and kOverworld_Lobytes_Comp (2 packed assets)
void ExtractOverworldCompressed(Rom *rom, AssetBuilder *builder) {
  printf("  Extracting kOverworld compressed data (160 areas each)...\n");

  // Extract hibytes (compressed tile data high bytes)
  uint8_t **hibytes = malloc(160 * sizeof(uint8_t*));
  uint32_t *hi_sizes = malloc(160 * sizeof(uint32_t));
  if (!hibytes || !hi_sizes) {
    LogError("Failed to allocate overworld hibytes arrays");
    free(hibytes);
    free(hi_sizes);
    return;
  }
  memset(hibytes, 0, 160 * sizeof(uint8_t*));
  for (int i = 0; i < 160; i++) {
    uint32_t addr = Rom_ReadAddr(rom, 0x82F94D + i * 3);
    DecompressedData *decomp = Snes_Decompress(rom, addr, true);
    if (decomp) {
      hi_sizes[i] = decomp->compressed_size;
      hibytes[i] = malloc(hi_sizes[i]);
      if (hibytes[i]) {
        memcpy(hibytes[i], Rom_ReadPtr(rom, addr, hi_sizes[i]), hi_sizes[i]);
      } else {
        hi_sizes[i] = 0;
      }
      Snes_FreeDecompressed(decomp);
    } else {
      hibytes[i] = NULL;
      hi_sizes[i] = 0;
    }
  }
  uint32_t hi_packed_size = 0;
  uint8_t *hi_packed = AssetBuilder_PackArrays(hibytes, hi_sizes, 160, &hi_packed_size);
  if (hi_packed) {
    AssetBuilder_AddAsset(builder, "kOverworld_Hibytes_Comp", ASSET_TYPE_PACKED, hi_packed, hi_packed_size);
    free(hi_packed);
  }
  for (int i = 0; i < 160; i++) free(hibytes[i]);
  free(hibytes);
  free(hi_sizes);

  // Extract lobytes (compressed tile data low bytes)
  uint8_t **lobytes = malloc(160 * sizeof(uint8_t*));
  uint32_t *lo_sizes = malloc(160 * sizeof(uint32_t));
  if (!lobytes || !lo_sizes) {
    LogError("Failed to allocate overworld lobytes arrays");
    free(lobytes);
    free(lo_sizes);
    return;
  }
  memset(lobytes, 0, 160 * sizeof(uint8_t*));
  for (int i = 0; i < 160; i++) {
    uint32_t addr = Rom_ReadAddr(rom, 0x82FB2D + i * 3);
    DecompressedData *decomp = Snes_Decompress(rom, addr, true);
    if (decomp) {
      lo_sizes[i] = decomp->compressed_size;
      lobytes[i] = malloc(lo_sizes[i]);
      if (lobytes[i]) {
        memcpy(lobytes[i], Rom_ReadPtr(rom, addr, lo_sizes[i]), lo_sizes[i]);
      } else {
        lo_sizes[i] = 0;
      }
      Snes_FreeDecompressed(decomp);
    } else {
      lobytes[i] = NULL;
      lo_sizes[i] = 0;
    }
  }
  uint32_t lo_packed_size = 0;
  uint8_t *lo_packed = AssetBuilder_PackArrays(lobytes, lo_sizes, 160, &lo_packed_size);
  if (lo_packed) {
    AssetBuilder_AddAsset(builder, "kOverworld_Lobytes_Comp", ASSET_TYPE_PACKED, lo_packed, lo_packed_size);
    free(lo_packed);
  }
  for (int i = 0; i < 160; i++) free(lobytes[i]);
  free(lobytes);
  free(lo_sizes);

  printf("    Added kOverworld_Hibytes_Comp (%u bytes) and Lobytes_Comp (%u bytes)\n",
         hi_packed_size, lo_packed_size);
}

// ============================================================================
// Overworld YAML Extraction
// ============================================================================

// ============================================================================
// Overworld ROM Addresses
// ============================================================================
#define ROM_OW_MAP_IS_SMALL    0x82F88D   // 192 bytes
#define ROM_OW_GFX             0x80FC9C   // 128 bytes (areas 0-127)
#define ROM_OW_PALETTE         0x80FD1C   // 136 bytes (areas 0-135)
#define ROM_OW_SIGN_TEXT       0x87F51D   // 128 words (areas 0-127)
#define ROM_OW_MUSIC           0x82C303   // 256 bytes (areas 0-63 × 4 phases)
#define ROM_OW_MUSIC2          0x82C403   // 96 bytes (areas 64-159)

// Bird travel (17 entries)
#define ROM_BIRD_SCREEN        0x82EAE5
#define ROM_BIRD_LOAD          0x82EB07
#define ROM_BIRD_SCROLL_Y      0x82EB29
#define ROM_BIRD_SCROLL_X      0x82EB4B
#define ROM_BIRD_POS_Y         0x82EB6D
#define ROM_BIRD_POS_X         0x82EB8F
#define ROM_BIRD_CAMERA_Y      0x82EBB1
#define ROM_BIRD_CAMERA_X      0x82EBD3
#define ROM_BIRD_UNK1          0x82EBF5
#define ROM_BIRD_UNK3          0x82EC17
#define ROM_WHIRLPOOL_AREAS    0x82ECF8   // 8 entries (for indices 9-16)

// Overworld entrances (129 entries)
#define ROM_OW_ENT_AREA        0x9BB96F
#define ROM_OW_ENT_POS         0x9BBA71
#define ROM_OW_ENT_ID          0x9BBB73
#define ROM_OW_ENT_COUNT       129

// Holes (19 entries)
#define ROM_HOLE_POS           0x9BB800
#define ROM_HOLE_AREA          0x9BB826
#define ROM_HOLE_ENT_ID        0x9BB84C
#define ROM_HOLE_COUNT         19

// Exits (79 entries)
#define ROM_EXIT_ROOM          0x82DD8A
#define ROM_EXIT_SCREEN        0x82DE28   // byte
#define ROM_EXIT_LOAD          0x82DE77
#define ROM_EXIT_SCROLL_Y      0x82DF15
#define ROM_EXIT_SCROLL_X      0x82DFB3
#define ROM_EXIT_POS_Y         0x82E051
#define ROM_EXIT_POS_X         0x82E0EF
#define ROM_EXIT_CAMERA_Y      0x82E18D
#define ROM_EXIT_CAMERA_X      0x82E22B
#define ROM_EXIT_UNK1          0x82E2C9   // byte
#define ROM_EXIT_UNK3          0x82E318   // byte
#define ROM_EXIT_DOOR_NORM     0x82E367
#define ROM_EXIT_DOOR_FANCY    0x82E405
#define ROM_EXIT_COUNT         79

// Special exits (16 entries, rooms 0x180-0x18F)
#define ROM_SPECIAL_EXIT_DIR       0x82E801
#define ROM_SPECIAL_EXIT_SPR_GFX   0x82E811
#define ROM_SPECIAL_EXIT_AUX_GFX   0x82E821
#define ROM_SPECIAL_EXIT_PAL_BG    0x82E831
#define ROM_SPECIAL_EXIT_PAL_SPR   0x82E841
#define ROM_SPECIAL_EXIT_TOP       0x82E6E1
#define ROM_SPECIAL_EXIT_BOTTOM    0x82E701
#define ROM_SPECIAL_EXIT_LEFT      0x82E721
#define ROM_SPECIAL_EXIT_RIGHT     0x82E741
#define ROM_SPECIAL_EXIT_LEFT_EDGE 0x82E7E1
#define ROM_SPECIAL_EXIT_UNK4      0x82E761
#define ROM_SPECIAL_EXIT_UNK5      0x82E7A1
#define ROM_SPECIAL_EXIT_UNK6      0x82E781
#define ROM_SPECIAL_EXIT_UNK7      0x82E7C1
#define ROM_SPECIAL_EXIT_COUNT     16

// Overworld items/secrets
#define ROM_OW_ITEMS_PTRS      0x9BC2F9   // 128 words (pointer table)
#define ROM_OW_ITEMS_BASE      0x9B0000

// Overworld sprites
#define ROM_OW_SPRITES_BASE0   0x89C881   // Beginning sprites (areas 0-63)
#define ROM_OW_SPRITES_BASE1   0x89C901   // Zelda sprites (areas 0-63)
#define ROM_OW_SPRITES_BASE2   0x89CA21   // Sword sprites (areas 0-63, also areas 64-127)
#define ROM_OW_SPRITE_GFX      0x80FA41   // 256 bytes (64 areas × 4 stages)
#define ROM_OW_SPRITE_PAL      0x80FB41   // 256 bytes

// Extract overworld data from ROM (~48 assets)
void ExtractOverworldYAML(AssetBuilder *builder, Rom *rom) {
  printf("  Extracting overworld data from ROM (160 areas)...\n");

  // ======================================================================
  // Phase 1: Header data (6 assets) - Read directly from ROM
  // ======================================================================

  // map_is_small - read directly from ROM
  uint8_t *map_is_small = malloc(192);
  for (int i = 0; i < 192; i++) {
    map_is_small[i] = Rom_ReadByte(rom, ROM_OW_MAP_IS_SMALL + i);
  }

  // Read header arrays directly from ROM - they're already in the right format!
  uint8_t *aux_tile_theme = malloc(128);
  uint8_t *bg_palettes = malloc(136);
  uint16_t *sign_text = malloc(128 * 2);
  uint8_t *music_sets = malloc(256);
  uint8_t *music_sets2 = malloc(96);

  for (int i = 0; i < 128; i++) {
    aux_tile_theme[i] = Rom_ReadByte(rom, ROM_OW_GFX + i);
    sign_text[i] = Rom_ReadWord(rom, ROM_OW_SIGN_TEXT + i * 2);
  }
  for (int i = 0; i < 136; i++) {
    bg_palettes[i] = Rom_ReadByte(rom, ROM_OW_PALETTE + i);
  }
  for (int i = 0; i < 256; i++) {
    music_sets[i] = Rom_ReadByte(rom, ROM_OW_MUSIC + i);
  }
  for (int i = 0; i < 96; i++) {
    music_sets2[i] = Rom_ReadByte(rom, ROM_OW_MUSIC2 + i);
  }

  AssetBuilder_AddAsset(builder, "kOverworldMapIsSmall", ASSET_TYPE_UINT8, map_is_small, 192);
  AssetBuilder_AddAsset(builder, "kOverworldAuxTileThemeIndexes", ASSET_TYPE_UINT8, aux_tile_theme, 128);
  AssetBuilder_AddAsset(builder, "kOverworldBgPalettes", ASSET_TYPE_UINT8, bg_palettes, 136);
  AssetBuilder_AddAsset(builder, "kOverworld_SignText", ASSET_TYPE_UINT16, (uint8_t*)sign_text, 128 * 2);
  AssetBuilder_AddAsset(builder, "kOwMusicSets", ASSET_TYPE_UINT8, music_sets, 256);
  AssetBuilder_AddAsset(builder, "kOwMusicSets2", ASSET_TYPE_UINT8, music_sets2, 96);

  free(aux_tile_theme); free(bg_palettes); free(sign_text);
  free(music_sets); free(music_sets2);

  printf("    Phase 1: Added 6 header assets from ROM\n");

  // ======================================================================
  // Phase 2: Travel data (11 assets) - Read directly from ROM
  // ======================================================================

  uint16_t *bird_screen = malloc(17 * 2);
  uint16_t *bird_load = malloc(17 * 2);
  uint16_t *bird_sx = malloc(17 * 2), *bird_sy = malloc(17 * 2);
  uint16_t *bird_px = malloc(17 * 2), *bird_py = malloc(17 * 2);
  uint16_t *bird_cx = malloc(17 * 2), *bird_cy = malloc(17 * 2);
  int8_t *bird_unk1 = malloc(17), *bird_unk3 = malloc(17);
  uint16_t *whirlpool_areas = malloc(8 * 2);

  for (int i = 0; i < 17; i++) {
    bird_screen[i] = Rom_ReadWord(rom, ROM_BIRD_SCREEN + i * 2);
    bird_load[i] = Rom_ReadWord(rom, ROM_BIRD_LOAD + i * 2);
    bird_sy[i] = Rom_ReadWord(rom, ROM_BIRD_SCROLL_Y + i * 2);
    bird_sx[i] = Rom_ReadWord(rom, ROM_BIRD_SCROLL_X + i * 2);
    bird_py[i] = Rom_ReadWord(rom, ROM_BIRD_POS_Y + i * 2);
    bird_px[i] = Rom_ReadWord(rom, ROM_BIRD_POS_X + i * 2);
    bird_cy[i] = Rom_ReadWord(rom, ROM_BIRD_CAMERA_Y + i * 2);
    bird_cx[i] = Rom_ReadWord(rom, ROM_BIRD_CAMERA_X + i * 2);
    // Note: unk1/unk3 are stored every other byte in ROM
    bird_unk1[i] = (int8_t)Rom_ReadByte(rom, ROM_BIRD_UNK1 + i * 2);
    bird_unk3[i] = (int8_t)Rom_ReadByte(rom, ROM_BIRD_UNK3 + i * 2);
  }
  for (int i = 0; i < 8; i++) {
    whirlpool_areas[i] = Rom_ReadWord(rom, ROM_WHIRLPOOL_AREAS + i * 2);
  }

  AssetBuilder_AddAsset(builder, "kBirdTravel_ScreenIndex", ASSET_TYPE_UINT16, (uint8_t*)bird_screen, 17*2);
  AssetBuilder_AddAsset(builder, "kBirdTravel_Map16LoadSrcOff", ASSET_TYPE_UINT16, (uint8_t*)bird_load, 17*2);
  AssetBuilder_AddAsset(builder, "kBirdTravel_ScrollX", ASSET_TYPE_UINT16, (uint8_t*)bird_sx, 17*2);
  AssetBuilder_AddAsset(builder, "kBirdTravel_ScrollY", ASSET_TYPE_UINT16, (uint8_t*)bird_sy, 17*2);
  AssetBuilder_AddAsset(builder, "kBirdTravel_LinkXCoord", ASSET_TYPE_UINT16, (uint8_t*)bird_px, 17*2);
  AssetBuilder_AddAsset(builder, "kBirdTravel_LinkYCoord", ASSET_TYPE_UINT16, (uint8_t*)bird_py, 17*2);
  AssetBuilder_AddAsset(builder, "kBirdTravel_CameraXScroll", ASSET_TYPE_UINT16, (uint8_t*)bird_cx, 17*2);
  AssetBuilder_AddAsset(builder, "kBirdTravel_CameraYScroll", ASSET_TYPE_UINT16, (uint8_t*)bird_cy, 17*2);
  AssetBuilder_AddAsset(builder, "kBirdTravel_Unk1", ASSET_TYPE_INT8, (uint8_t*)bird_unk1, 17);
  AssetBuilder_AddAsset(builder, "kBirdTravel_Unk3", ASSET_TYPE_INT8, (uint8_t*)bird_unk3, 17);
  AssetBuilder_AddAsset(builder, "kWhirlpoolAreas", ASSET_TYPE_UINT16, (uint8_t*)whirlpool_areas, 8*2);

  free(bird_screen); free(bird_load); free(bird_sx); free(bird_sy);
  free(bird_px); free(bird_py); free(bird_cx); free(bird_cy);
  free(bird_unk1); free(bird_unk3); free(whirlpool_areas);

  printf("    Phase 2: Added 11 travel assets from ROM\n");

  // ======================================================================
  // Phase 3: Entrances (3 assets) - Read directly from ROM
  // ======================================================================

  uint16_t *ent_area = malloc(ROM_OW_ENT_COUNT * 2);
  uint16_t *ent_pos = malloc(ROM_OW_ENT_COUNT * 2);
  uint8_t *ent_id = malloc(ROM_OW_ENT_COUNT);

  for (int i = 0; i < ROM_OW_ENT_COUNT; i++) {
    ent_area[i] = Rom_ReadWord(rom, ROM_OW_ENT_AREA + i * 2);
    ent_pos[i] = Rom_ReadWord(rom, ROM_OW_ENT_POS + i * 2);
    ent_id[i] = Rom_ReadByte(rom, ROM_OW_ENT_ID + i);
  }

  AssetBuilder_AddAsset(builder, "kOverworld_Entrance_Area", ASSET_TYPE_UINT16, (uint8_t*)ent_area, ROM_OW_ENT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kOverworld_Entrance_Pos", ASSET_TYPE_UINT16, (uint8_t*)ent_pos, ROM_OW_ENT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kOverworld_Entrance_Id", ASSET_TYPE_UINT8, ent_id, ROM_OW_ENT_COUNT);

  free(ent_area); free(ent_pos); free(ent_id);

  printf("    Phase 3: Added 3 entrance assets (%d entries each)\n", ROM_OW_ENT_COUNT);

  // ======================================================================
  // Phase 4: Holes (3 assets) - Read directly from ROM
  // ======================================================================

  uint16_t *hole_pos = malloc(ROM_HOLE_COUNT * 2);
  uint16_t *hole_area = malloc(ROM_HOLE_COUNT * 2);
  uint8_t *hole_ent_id = malloc(ROM_HOLE_COUNT);

  for (int i = 0; i < ROM_HOLE_COUNT; i++) {
    // Note: Python adds 0x400 to pos
    hole_pos[i] = Rom_ReadWord(rom, ROM_HOLE_POS + i * 2) + 0x400;
    hole_area[i] = Rom_ReadWord(rom, ROM_HOLE_AREA + i * 2);
    hole_ent_id[i] = Rom_ReadByte(rom, ROM_HOLE_ENT_ID + i);
  }

  AssetBuilder_AddAsset(builder, "kFallHole_Area", ASSET_TYPE_UINT16, (uint8_t*)hole_area, ROM_HOLE_COUNT*2);
  AssetBuilder_AddAsset(builder, "kFallHole_Pos", ASSET_TYPE_UINT16, (uint8_t*)hole_pos, ROM_HOLE_COUNT*2);
  AssetBuilder_AddAsset(builder, "kFallHole_Entrances", ASSET_TYPE_UINT8, hole_ent_id, ROM_HOLE_COUNT);

  free(hole_area); free(hole_pos); free(hole_ent_id);

  printf("    Phase 4: Added 3 hole assets (%d entries)\n", ROM_HOLE_COUNT);

  // ======================================================================
  // Phase 5: Exits (22 assets) - Read directly from ROM
  // ======================================================================

  // Regular exits (79 entries)
  uint8_t *exit_screen = malloc(ROM_EXIT_COUNT);
  uint16_t *exit_rooms = malloc(ROM_EXIT_COUNT * 2);
  uint16_t *exit_load = malloc(ROM_EXIT_COUNT * 2);
  uint16_t *exit_sx = malloc(ROM_EXIT_COUNT * 2), *exit_sy = malloc(ROM_EXIT_COUNT * 2);
  uint16_t *exit_px = malloc(ROM_EXIT_COUNT * 2), *exit_py = malloc(ROM_EXIT_COUNT * 2);
  uint16_t *exit_cx = malloc(ROM_EXIT_COUNT * 2), *exit_cy = malloc(ROM_EXIT_COUNT * 2);
  uint16_t *exit_normal_door = malloc(ROM_EXIT_COUNT * 2);
  uint16_t *exit_fancy_door = malloc(ROM_EXIT_COUNT * 2);
  int8_t *exit_unk1 = malloc(ROM_EXIT_COUNT), *exit_unk3 = malloc(ROM_EXIT_COUNT);

  for (int i = 0; i < ROM_EXIT_COUNT; i++) {
    exit_screen[i] = Rom_ReadByte(rom, ROM_EXIT_SCREEN + i);
    exit_rooms[i] = Rom_ReadWord(rom, ROM_EXIT_ROOM + i * 2);
    exit_load[i] = Rom_ReadWord(rom, ROM_EXIT_LOAD + i * 2);
    exit_sy[i] = Rom_ReadWord(rom, ROM_EXIT_SCROLL_Y + i * 2);
    exit_sx[i] = Rom_ReadWord(rom, ROM_EXIT_SCROLL_X + i * 2);
    exit_py[i] = Rom_ReadWord(rom, ROM_EXIT_POS_Y + i * 2);
    exit_px[i] = Rom_ReadWord(rom, ROM_EXIT_POS_X + i * 2);
    exit_cy[i] = Rom_ReadWord(rom, ROM_EXIT_CAMERA_Y + i * 2);
    exit_cx[i] = Rom_ReadWord(rom, ROM_EXIT_CAMERA_X + i * 2);
    exit_unk1[i] = (int8_t)Rom_ReadByte(rom, ROM_EXIT_UNK1 + i);
    exit_unk3[i] = (int8_t)Rom_ReadByte(rom, ROM_EXIT_UNK3 + i);
    exit_normal_door[i] = Rom_ReadWord(rom, ROM_EXIT_DOOR_NORM + i * 2);
    exit_fancy_door[i] = Rom_ReadWord(rom, ROM_EXIT_DOOR_FANCY + i * 2);
  }

  // Special exits (16 entries)
  uint16_t *sp_top = malloc(ROM_SPECIAL_EXIT_COUNT * 2), *sp_bottom = malloc(ROM_SPECIAL_EXIT_COUNT * 2);
  uint16_t *sp_left = malloc(ROM_SPECIAL_EXIT_COUNT * 2), *sp_right = malloc(ROM_SPECIAL_EXIT_COUNT * 2);
  int16_t *sp_tab4 = malloc(ROM_SPECIAL_EXIT_COUNT * 2), *sp_tab5 = malloc(ROM_SPECIAL_EXIT_COUNT * 2);
  int16_t *sp_tab6 = malloc(ROM_SPECIAL_EXIT_COUNT * 2), *sp_tab7 = malloc(ROM_SPECIAL_EXIT_COUNT * 2);
  uint16_t *sp_left_edge = malloc(ROM_SPECIAL_EXIT_COUNT * 2);
  uint8_t *sp_dir = malloc(ROM_SPECIAL_EXIT_COUNT);
  uint8_t *sp_spr_gfx = malloc(ROM_SPECIAL_EXIT_COUNT), *sp_aux_gfx = malloc(ROM_SPECIAL_EXIT_COUNT);
  uint8_t *sp_pal_bg = malloc(ROM_SPECIAL_EXIT_COUNT), *sp_pal_spr = malloc(ROM_SPECIAL_EXIT_COUNT);

  for (int i = 0; i < ROM_SPECIAL_EXIT_COUNT; i++) {
    sp_dir[i] = Rom_ReadByte(rom, ROM_SPECIAL_EXIT_DIR + i);
    sp_spr_gfx[i] = Rom_ReadByte(rom, ROM_SPECIAL_EXIT_SPR_GFX + i);
    sp_aux_gfx[i] = Rom_ReadByte(rom, ROM_SPECIAL_EXIT_AUX_GFX + i);
    sp_pal_bg[i] = Rom_ReadByte(rom, ROM_SPECIAL_EXIT_PAL_BG + i);
    sp_pal_spr[i] = Rom_ReadByte(rom, ROM_SPECIAL_EXIT_PAL_SPR + i);
    sp_top[i] = Rom_ReadWord(rom, ROM_SPECIAL_EXIT_TOP + i * 2);
    sp_bottom[i] = Rom_ReadWord(rom, ROM_SPECIAL_EXIT_BOTTOM + i * 2);
    sp_left[i] = Rom_ReadWord(rom, ROM_SPECIAL_EXIT_LEFT + i * 2);
    sp_right[i] = Rom_ReadWord(rom, ROM_SPECIAL_EXIT_RIGHT + i * 2);
    sp_left_edge[i] = Rom_ReadWord(rom, ROM_SPECIAL_EXIT_LEFT_EDGE + i * 2);
    sp_tab4[i] = (int16_t)Rom_ReadWord(rom, ROM_SPECIAL_EXIT_UNK4 + i * 2);
    sp_tab5[i] = (int16_t)Rom_ReadWord(rom, ROM_SPECIAL_EXIT_UNK5 + i * 2);
    sp_tab6[i] = (int16_t)Rom_ReadWord(rom, ROM_SPECIAL_EXIT_UNK6 + i * 2);
    sp_tab7[i] = (int16_t)Rom_ReadWord(rom, ROM_SPECIAL_EXIT_UNK7 + i * 2);
  }

  AssetBuilder_AddAsset(builder, "kExitData_ScreenIndex", ASSET_TYPE_UINT8, exit_screen, ROM_EXIT_COUNT);
  AssetBuilder_AddAsset(builder, "kExitDataRooms", ASSET_TYPE_UINT16, (uint8_t*)exit_rooms, ROM_EXIT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kExitData_Map16LoadSrcOff", ASSET_TYPE_UINT16, (uint8_t*)exit_load, ROM_EXIT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kExitData_ScrollX", ASSET_TYPE_UINT16, (uint8_t*)exit_sx, ROM_EXIT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kExitData_ScrollY", ASSET_TYPE_UINT16, (uint8_t*)exit_sy, ROM_EXIT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kExitData_XCoord", ASSET_TYPE_UINT16, (uint8_t*)exit_px, ROM_EXIT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kExitData_YCoord", ASSET_TYPE_UINT16, (uint8_t*)exit_py, ROM_EXIT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kExitData_CameraXScroll", ASSET_TYPE_UINT16, (uint8_t*)exit_cx, ROM_EXIT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kExitData_CameraYScroll", ASSET_TYPE_UINT16, (uint8_t*)exit_cy, ROM_EXIT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kExitData_NormalDoor", ASSET_TYPE_UINT16, (uint8_t*)exit_normal_door, ROM_EXIT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kExitData_FancyDoor", ASSET_TYPE_UINT16, (uint8_t*)exit_fancy_door, ROM_EXIT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kExitData_Unk1", ASSET_TYPE_INT8, (uint8_t*)exit_unk1, ROM_EXIT_COUNT);
  AssetBuilder_AddAsset(builder, "kExitData_Unk3", ASSET_TYPE_INT8, (uint8_t*)exit_unk3, ROM_EXIT_COUNT);

  AssetBuilder_AddAsset(builder, "kSpExit_Top", ASSET_TYPE_UINT16, (uint8_t*)sp_top, ROM_SPECIAL_EXIT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kSpExit_Bottom", ASSET_TYPE_UINT16, (uint8_t*)sp_bottom, ROM_SPECIAL_EXIT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kSpExit_Left", ASSET_TYPE_UINT16, (uint8_t*)sp_left, ROM_SPECIAL_EXIT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kSpExit_Right", ASSET_TYPE_UINT16, (uint8_t*)sp_right, ROM_SPECIAL_EXIT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kSpExit_Tab4", ASSET_TYPE_INT16, (uint8_t*)sp_tab4, ROM_SPECIAL_EXIT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kSpExit_Tab5", ASSET_TYPE_INT16, (uint8_t*)sp_tab5, ROM_SPECIAL_EXIT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kSpExit_Tab6", ASSET_TYPE_INT16, (uint8_t*)sp_tab6, ROM_SPECIAL_EXIT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kSpExit_Tab7", ASSET_TYPE_INT16, (uint8_t*)sp_tab7, ROM_SPECIAL_EXIT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kSpExit_LeftEdgeOfMap", ASSET_TYPE_UINT16, (uint8_t*)sp_left_edge, ROM_SPECIAL_EXIT_COUNT*2);
  AssetBuilder_AddAsset(builder, "kSpExit_Dir", ASSET_TYPE_UINT8, sp_dir, ROM_SPECIAL_EXIT_COUNT);
  AssetBuilder_AddAsset(builder, "kSpExit_SprGfx", ASSET_TYPE_UINT8, sp_spr_gfx, ROM_SPECIAL_EXIT_COUNT);
  AssetBuilder_AddAsset(builder, "kSpExit_AuxGfx", ASSET_TYPE_UINT8, sp_aux_gfx, ROM_SPECIAL_EXIT_COUNT);
  AssetBuilder_AddAsset(builder, "kSpExit_PalBg", ASSET_TYPE_UINT8, sp_pal_bg, ROM_SPECIAL_EXIT_COUNT);
  AssetBuilder_AddAsset(builder, "kSpExit_PalSpr", ASSET_TYPE_UINT8, sp_pal_spr, ROM_SPECIAL_EXIT_COUNT);

  free(exit_screen); free(exit_rooms); free(exit_load); free(exit_sx); free(exit_sy);
  free(exit_px); free(exit_py); free(exit_cx); free(exit_cy);
  free(exit_normal_door); free(exit_fancy_door); free(exit_unk1); free(exit_unk3);
  free(sp_top); free(sp_bottom); free(sp_left); free(sp_right);
  free(sp_tab4); free(sp_tab5); free(sp_tab6); free(sp_tab7); free(sp_left_edge);
  free(sp_dir); free(sp_spr_gfx); free(sp_aux_gfx); free(sp_pal_bg); free(sp_pal_spr);

  printf("    Phase 5: Added 22 exit assets from ROM\n");

  // ======================================================================
  // Phase 6: Secrets (2 assets) - Item locations from ROM
  // ROM format: pointer table at 0x9BC2F9, data at 0x9B0000 | ptr
  // Data format: [pos_lo, pos_hi, item_id] triplets, 0xFFFF terminator
  // ======================================================================

  uint16_t *secret_offs = malloc(128 * 2);
  uint8_t *secret_data = malloc(10000);
  int secret_len = 0;

  // Initialize all offsets to 0xFFFF (unset marker)
  for (int i = 0; i < 128; i++) {
    secret_offs[i] = 0xFFFF;
  }

  for (int i = 0; i < 128; i++) {  // Only areas 0-127
    if (!Overworld_IsAreaHead(rom, i)) {
      continue;  // Will be mirrored from area head or set to default
    }

    // Read pointer from table
    uint16_t ptr = Rom_ReadWord(rom, ROM_OW_ITEMS_PTRS + i * 2);
    uint32_t ea = ROM_OW_ITEMS_BASE | ptr;

    // Check if there are any items (first word != 0xFFFF)
    uint16_t first = Rom_ReadWord(rom, ea);
    if (first == 0xFFFF) {
      continue;  // No items - will be set to default
    }

    // Has items - record offset and copy data
    secret_offs[i] = secret_len;

    // Copy item entries until terminator
    while (true) {
      uint16_t pos = Rom_ReadWord(rom, ea);
      if (pos == 0xFFFF) {
        // Add terminator
        secret_data[secret_len++] = 0xff;
        secret_data[secret_len++] = 0xff;
        break;
      }
      uint8_t item_id = Rom_ReadByte(rom, ea + 2);
      secret_data[secret_len++] = pos & 0xff;
      secret_data[secret_len++] = pos >> 8;
      secret_data[secret_len++] = item_id;
      ea += 3;
    }

    // Mirror for big maps
    if (map_is_small[i] == 0) {
      secret_offs[i + 1] = secret_offs[i];
      secret_offs[i + 8] = secret_offs[i];
      secret_offs[i + 9] = secret_offs[i];
    }
  }

  // Fill in default offsets for areas without items
  // Point to an empty entry (just a terminator)
  if (secret_len == 0) {
    // No items at all - add a terminator
    secret_data[secret_len++] = 0xff;
    secret_data[secret_len++] = 0xff;
  }
  uint16_t default_offset = secret_len - 2;  // Point to last 0xFFFF
  for (int i = 0; i < 128; i++) {
    if (secret_offs[i] == 0xFFFF) {  // Was not set
      secret_offs[i] = default_offset;
    }
  }

  AssetBuilder_AddAsset(builder, "kOverworldSecrets_Offs", ASSET_TYPE_UINT16, (uint8_t*)secret_offs, 128*2);
  AssetBuilder_AddAsset(builder, "kOverworldSecrets", ASSET_TYPE_UINT8, secret_data, secret_len);

  free(secret_offs);
  free(secret_data);

  printf("    Phase 6: Added 2 secret assets from ROM (%d bytes of data)\n", secret_len);

  // ======================================================================
  // Phase 7: Sprites (4 assets) - Sprite lists from ROM
  // ROM format: pointer tables at 0x89C881/C901/CA21, data at 0x890000 | ptr
  // Data format: [y, x, type] triplets, 0xFF terminator
  // GFX/Palette: 0x80FA41/FB41 + (area & 63) + stage * 64
  // ======================================================================

  uint16_t *sprite_offs = calloc(144 * 3, 2);  // 3 stages × 144 areas
  uint8_t *sprite_gfx = calloc(256, 1);
  uint8_t *sprite_pal = calloc(256, 1);
  uint8_t *sprite_data = malloc(50000);
  int sprite_len = 0;

  sprite_data[sprite_len++] = 0xff;  // Initial terminator (for empty areas)

  // Read GFX and palette tables from ROM (256 bytes each)
  for (int i = 0; i < 256; i++) {
    sprite_gfx[i] = Rom_ReadByte(rom, ROM_OW_SPRITE_GFX + i);
    sprite_pal[i] = Rom_ReadByte(rom, ROM_OW_SPRITE_PAL + i);
  }

  // Helper: extract sprites from ROM for an area with a given pointer table base
  #define EXTRACT_SPRITES(area, ptr_table_base, stage_idx) do { \
    uint16_t ptr = Rom_ReadWord(rom, (ptr_table_base) + (area) * 2); \
    uint32_t ea = 0x890000 | ptr; \
    /* Check if there are any sprites (first byte != 0xFF) */ \
    if (Rom_ReadByte(rom, ea) != 0xFF) { \
      /* Has sprites - record offset */ \
      sprite_offs[(stage_idx) * 144 + (area)] = sprite_len; \
      /* Copy sprite entries until terminator */ \
      while (true) { \
        uint8_t y = Rom_ReadByte(rom, ea); \
        if (y == 0xFF) { \
          sprite_data[sprite_len++] = 0xff; /* Terminator */ \
          break; \
        } \
        sprite_data[sprite_len++] = y; \
        sprite_data[sprite_len++] = Rom_ReadByte(rom, ea + 1); /* x */ \
        sprite_data[sprite_len++] = Rom_ReadByte(rom, ea + 2); /* type */ \
        ea += 3; \
      } \
    } \
  } while(0)

  // Process Light World areas (0-63) - 3 stages
  for (int i = 0; i < 64; i++) {
    if (!Overworld_IsAreaHead(rom, i)) continue;

    // Stage 0: Beginning sprites (ptr table at 0x89C881)
    EXTRACT_SPRITES(i, ROM_OW_SPRITES_BASE0, 0);
    // Mirror for big maps
    if (map_is_small[i] == 0) {
      sprite_offs[0 * 144 + i + 1] = sprite_offs[0 * 144 + i];
      sprite_offs[0 * 144 + i + 8] = sprite_offs[0 * 144 + i];
      sprite_offs[0 * 144 + i + 9] = sprite_offs[0 * 144 + i];
    }

    // Stage 1: FirstPart sprites (ptr table at 0x89C901)
    EXTRACT_SPRITES(i, ROM_OW_SPRITES_BASE1, 1);
    if (map_is_small[i] == 0) {
      sprite_offs[1 * 144 + i + 1] = sprite_offs[1 * 144 + i];
      sprite_offs[1 * 144 + i + 8] = sprite_offs[1 * 144 + i];
      sprite_offs[1 * 144 + i + 9] = sprite_offs[1 * 144 + i];
    }

    // Stage 2: SecondPart sprites (ptr table at 0x89CA21)
    EXTRACT_SPRITES(i, ROM_OW_SPRITES_BASE2, 2);
    if (map_is_small[i] == 0) {
      sprite_offs[2 * 144 + i + 1] = sprite_offs[2 * 144 + i];
      sprite_offs[2 * 144 + i + 8] = sprite_offs[2 * 144 + i];
      sprite_offs[2 * 144 + i + 9] = sprite_offs[2 * 144 + i];
    }
  }

  // Process Dark World and special areas (64-143) - stages 1 and 2 use the same data
  for (int i = 64; i < 144; i++) {
    if (!Overworld_IsAreaHead(rom, i)) continue;

    // Areas 64-143 use SecondPart sprites for both stage 1 and 2
    EXTRACT_SPRITES(i, ROM_OW_SPRITES_BASE2, 1);
    // Copy same offset to stage 2
    sprite_offs[2 * 144 + i] = sprite_offs[1 * 144 + i];

    if (i < 128 && map_is_small[i] == 0) {
      sprite_offs[1 * 144 + i + 1] = sprite_offs[1 * 144 + i];
      sprite_offs[1 * 144 + i + 8] = sprite_offs[1 * 144 + i];
      sprite_offs[1 * 144 + i + 9] = sprite_offs[1 * 144 + i];
      sprite_offs[2 * 144 + i + 1] = sprite_offs[2 * 144 + i];
      sprite_offs[2 * 144 + i + 8] = sprite_offs[2 * 144 + i];
      sprite_offs[2 * 144 + i + 9] = sprite_offs[2 * 144 + i];
    }
  }

  #undef EXTRACT_SPRITES

  AssetBuilder_AddAsset(builder, "kOverworldSpriteOffs", ASSET_TYPE_UINT16, (uint8_t*)sprite_offs, 144*3*2);
  AssetBuilder_AddAsset(builder, "kOverworldSprites", ASSET_TYPE_UINT8, sprite_data, sprite_len);
  AssetBuilder_AddAsset(builder, "kOverworldSpriteGfx", ASSET_TYPE_UINT8, sprite_gfx, 256);
  AssetBuilder_AddAsset(builder, "kOverworldSpritePalettes", ASSET_TYPE_UINT8, sprite_pal, 256);

  free(sprite_offs);
  free(sprite_data);
  free(sprite_gfx);
  free(sprite_pal);

  printf("    Phase 7: Added 4 sprite assets from ROM (%d bytes of sprite data)\n", sprite_len);

  // ======================================================================
  // Phase 8: ROM-based assets (2 assets)
  // ======================================================================

  uint8_t *map8_attr = Rom_ReadPtr(rom, 0x8E9459, 512);
  uint8_t *tile_attr = Rom_ReadPtr(rom, 0x9bf110, 3824);

  if (map8_attr) AssetBuilder_AddAsset(builder, "kMap8DataToTileAttr", ASSET_TYPE_UINT8, map8_attr, 512);
  if (tile_attr) AssetBuilder_AddAsset(builder, "kSomeTileAttr", ASSET_TYPE_UINT8, tile_attr, 3824);

  printf("    Phase 8: Added 2 ROM-based assets (512 + 3824 bytes)\n");

  // Free map_is_small at the very end after all phases
  free(map_is_small);

  printf("  ✅ Overworld extraction complete: 48 assets from 160 areas\n");
}

// ============================================================================
// YAML Testing
// ============================================================================

void TestYAMLLoading(void) {
  printf("Testing YAML loading...\n");

  const char *test_file = "assets/dungeon/dungeon-0.yaml";
  YamlDoc *doc = LoadAssetYaml(test_file);

  if (!doc) {
    LogError("Failed to load %s: %s", test_file, Yaml_GetLastError());
    printf("❌ YAML test FAILED\n");
    return;
  }

  printf("✅ Loaded %s successfully\n", test_file);

  // Get root node
  YamlNode *root = Yaml_GetRoot(doc);
  if (!root) {
    LogError("Failed to get root node");
    Yaml_Free(doc);
    printf("❌ YAML test FAILED\n");
    return;
  }

  // Get Header section
  YamlNode *header = Yaml_GetMapping(root, "Header");
  if (!header) {
    LogError("Failed to get Header mapping");
    free(root);
    Yaml_Free(doc);
    printf("❌ YAML test FAILED\n");
    return;
  }

  // Read floor1 field
  int floor1 = Yaml_GetInt(header, "floor1", -1);
  printf("  Header.floor1 = %d (expected: 6)\n", floor1);

  // Cleanup YAML
  free(header);
  free(root);
  Yaml_Free(doc);

  // Test lookup tables
  printf("\nTesting lookup tables...\n");

  // Test sprite name lookup
  int idx = FindSpriteIndex("00-Raven");
  printf("  FindSpriteIndex(\"00-Raven\") = %d (expected: 0)\n", idx);
  if (idx != 0) {
    printf("❌ Table test FAILED - Sprite lookup incorrect\n");
    return;
  }

  // Test type0 name lookup
  idx = FindType0Index("00-Ceiling [L-R]");
  printf("  FindType0Index(\"00-Ceiling [L-R]\") = %d (expected: 0)\n", idx);
  if (idx != 0) {
    printf("❌ Table test FAILED - Type0 lookup incorrect\n");
    return;
  }

  // Test music lookup
  idx = FindMusicIndex("Title");
  printf("  FindMusicIndex(\"Title\") = %d (expected: 1)\n", idx);
  if (idx != 1) {
    printf("❌ Table test FAILED - Music lookup incorrect\n");
    return;
  }

  // Test reverse music lookup
  const char *name = GetMusicName(1);
  printf("  GetMusicName(1) = \"%s\" (expected: \"Title\")\n", name ? name : "NULL");
  if (!name || strcmp(name, "Title") != 0) {
    printf("❌ Table test FAILED - Music reverse lookup incorrect\n");
    return;
  }

  if (floor1 == 6) {
    printf("\n✅ ALL TESTS PASSED - YAML parsing and lookup tables work!\n");
  } else {
    printf("\n❌ YAML test FAILED - Got floor1=%d, expected 6\n", floor1);
  }
}

// ROM addresses for sound banks (SNES addresses)
static const uint32_t kSoundBankAddresses[] = {
  0x998000,  // intro
  0x9B8000,  // indoor
  0x9AD380,  // ending
};

// Extract a sound bank directly from ROM
// The ROM contains sound data in loadable format: [len_lo][len_hi][addr_lo][addr_hi][data...]
// repeated until len == 0 (terminator: [0x00][0x00])
static uint8_t* ExtractSoundBankFromROM(Rom *rom, int bank_index, size_t *out_size) {
  if (bank_index < 0 || bank_index >= 3) {
    LogError("Invalid sound bank index: %d", bank_index);
    return NULL;
  }

  uint32_t snes_addr = kSoundBankAddresses[bank_index];

  // First pass: calculate total size
  size_t total_size = 0;
  uint32_t ea = snes_addr;
  int chunk_count = 0;

  while (chunk_count < 256) {  // Safety limit
    uint16_t numbytes = Rom_ReadWord(rom, ea);
    if (numbytes == 0) {
      total_size += 2;  // Terminator
      break;
    }

    // Header (4 bytes) + data
    total_size += 4 + numbytes;
    ea += 4 + numbytes;

    // Handle bank boundary crossing (SNES addressing quirk)
    // If we cross below $8000 in the low word, we need to add $8000
    if ((ea & 0xFFFF) < 0x8000) {
      ea = (ea & 0xFF0000) | ((ea & 0xFFFF) + 0x8000);
    }

    chunk_count++;
  }

  if (chunk_count >= 256) {
    LogError("Sound bank at $%06X seems corrupted (too many chunks)", snes_addr);
    return NULL;
  }

  // Allocate output buffer
  uint8_t *output = malloc(total_size);
  if (!output) {
    LogError("Failed to allocate %zu bytes for sound bank", total_size);
    return NULL;
  }

  // Second pass: copy data
  size_t pos = 0;
  ea = snes_addr;

  while (true) {
    uint16_t numbytes = Rom_ReadWord(rom, ea);
    uint16_t target = Rom_ReadWord(rom, ea + 2);

    // Write header
    output[pos++] = numbytes & 0xFF;
    output[pos++] = (numbytes >> 8) & 0xFF;

    if (numbytes == 0) {
      // Terminator - target address is entry point, not used in output
      break;
    }

    output[pos++] = target & 0xFF;
    output[pos++] = (target >> 8) & 0xFF;

    ea += 4;

    // Copy data bytes
    for (uint16_t i = 0; i < numbytes; i++) {
      output[pos++] = Rom_ReadByte(rom, ea);
      ea++;

      // Handle bank boundary crossing
      if ((ea & 0xFFFF) < 0x8000) {
        ea = (ea & 0xFF0000) | ((ea & 0xFFFF) + 0x8000);
      }
    }
  }

  *out_size = pos;
  return output;
}

// Extract sound banks - uses ROM data directly if embedded assets unavailable
// Returns true on success, false on error
bool ExtractSoundBanks(AssetBuilder *builder, Rom *rom) {
  printf("  Extracting sound banks (intro, indoor, ending)...\n");

  const char *songs[] = {"intro", "indoor", "ending"};

  for (int i = 0; i < 3; i++) {
    const char *song = songs[i];

    size_t size = 0;
    uint8_t *data = ExtractSoundBankFromROM(rom, i, &size);
    if (!data) {
      LogError("Failed to extract sound bank %s from ROM", song);
      return false;
    }

    // Add asset
    char asset_name[64];
    snprintf(asset_name, sizeof(asset_name), "kSoundBank_%s", song);
    AssetBuilder_AddAsset(builder, asset_name, ASSET_TYPE_UINT8, data, size);
    free(data);

    printf("    Added %s (%zu bytes)\n", asset_name, size);
  }

  printf("  Sound banks complete: 3 assets\n");
  return true;
}

// ============================================================================
// Font Extraction from ROM
// ============================================================================

// ROM addresses for font data in different language ROMs
typedef struct {
  const char *lang;
  uint32_t font_addr;     // SNES address of font tile data
  uint32_t width_addr;    // SNES address of font width table
  int width_count;        // Number of entries in width table
} FontRomConfig;

static const FontRomConfig kFontRomConfigs[] = {
  { "us",         0x8E8000, 0x8ECADF, 99 },
  { "en",         0x8E8000, 0x8ECAFF, 102 },
  { "de",         0x0CC6E8, 0x8CDECF, 112 },  // German ROM has font at different address
  { "fr",         0x0CC6E8, 0x8CDEAF, 112 },  // French ROM
  { "fr-c",       0x0CD078, 0x8CE83F, 112 },  // French Canada ROM
  { "es",         0x8E8000, 0x8ECADF, 99 },
  { "pl",         0x8E8000, 0x8ECADF, 99 },
  { "pt",         0x8E8000, 0x8ECADF, 121 },
  { "nl",         0x8E8000, 0x8ECADF, 99 },
  { "sv",         0x8E8000, 0x8ECADF, 99 },
  { "redux",      0x8E8000, 0x8ECADF, 99 },
  { "retrans-kal",0x8E8000, 0x8ECADF, 99 },
};

#define FONT_TILE_SIZE 4096  // 256 chars × 16 bytes

const FontRomConfig* GetFontRomConfig(const char *lang) {
  for (size_t i = 0; i < sizeof(kFontRomConfigs) / sizeof(kFontRomConfigs[0]); i++) {
    if (strcmp(kFontRomConfigs[i].lang, lang) == 0) {
      return &kFontRomConfigs[i];
    }
  }
  return NULL;
}

// Extract font data from ROM and save to binary files
// Creates: font_{lang}.bin (4096 bytes) and fontwidth_{lang}.bin (width_count bytes)
bool ExtractFontFromRom(Rom *rom, const char *lang_code, const char *output_dir) {
  const FontRomConfig *config = GetFontRomConfig(lang_code);
  if (!config) {
    LogError("No font configuration for language '%s'", lang_code);
    return false;
  }

  printf("Extracting font from ROM at 0x%X...\n", config->font_addr);

  // Read font tile data
  uint8_t *font_data = Rom_ReadPtr(rom, config->font_addr, FONT_TILE_SIZE);
  if (!font_data) {
    LogError("Failed to read font data from ROM at 0x%X", config->font_addr);
    return false;
  }

  // Read width table
  uint8_t *width_data = Rom_ReadPtr(rom, config->width_addr, config->width_count);
  if (!width_data) {
    LogError("Failed to read font width table from ROM at 0x%X", config->width_addr);
    return false;
  }

  // Build output filenames
  char font_filename[512], width_filename[512];
  const char *dir = output_dir && output_dir[0] ? output_dir : ".";

  // Replace '-' with '_' in language code for filename
  char lang_clean[16];
  strncpy(lang_clean, lang_code, sizeof(lang_clean) - 1);
  lang_clean[sizeof(lang_clean) - 1] = '\0';
  for (char *p = lang_clean; *p; p++) {
    if (*p == '-') *p = '_';
  }

  snprintf(font_filename, sizeof(font_filename), "%s/font_%s.bin", dir, lang_clean);
  snprintf(width_filename, sizeof(width_filename), "%s/fontwidth_%s.bin", dir, lang_clean);

  // Write font data
  FILE *f = fopen(font_filename, "wb");
  if (!f) {
    LogError("Failed to create %s", font_filename);
    return false;
  }
  fwrite(font_data, 1, FONT_TILE_SIZE, f);
  fclose(f);
  printf("  Wrote %s (%d bytes)\n", font_filename, FONT_TILE_SIZE);

  // Write width data
  f = fopen(width_filename, "wb");
  if (!f) {
    LogError("Failed to create %s", width_filename);
    return false;
  }
  fwrite(width_data, 1, config->width_count, f);
  fclose(f);
  printf("  Wrote %s (%d bytes)\n", width_filename, config->width_count);

  return true;
}

// Extract dialogue assets (pure C implementation - no Python dependency)
// languages_arg: comma-separated list like "de,fr" (US is always included first), or NULL for US only
// Returns true on success, false on error
bool ExtractDialogue(AssetBuilder *builder, Rom *rom, const char *languages_arg) {
  // Call pure C implementation (replaces Python script)
  return ExtractDialogueAssets(builder, rom, languages_arg);
}
