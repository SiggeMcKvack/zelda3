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

// Third-party
#include "sha256.h"
#include "lodepng.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define RESTOOL_VERSION "0.1.0"

typedef struct {
  const char *rom_path;
  const char *output_dir;
  bool extract_mode;
  bool compile_mode;
  bool no_compile;          // --no-compile flag to skip auto-compilation
  bool extract_dialogue;
  bool extract_graphics;
  bool extract_overworld;
  int extract_enemy_sheet;  // -1 = none, 0-N = specific sheet
  const char *language;
  const char *languages;    // --languages comma-separated list for multi-lang build
  bool verbose;
  bool test_yaml;
  bool test_map32;
  bool test_link;
  bool test_dungeon;
  bool help;
  bool version;
} RestoolArgs;

// ============================================================================
// Graphics Extraction
// ============================================================================

// Extract kSprGfx (108 sprite tilesets) - Python-compatible
void ExtractSpriteGraphics(Rom *rom, AssetBuilder *builder) {
  printf("  Extracting kSprGfx (108 sprite tilesets)...\n");

  uint8_t **arrays = (uint8_t**)malloc(108 * sizeof(uint8_t*));
  uint32_t *sizes = (uint32_t*)malloc(108 * sizeof(uint32_t));

  if (!arrays || !sizes) {
    LogError("Failed to allocate arrays for kSprGfx");
    free(arrays);
    free(sizes);
    return;
  }

  // Extract all 108 sprite tilesets
  for (uint32_t i = 0; i < 108; i++) {
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

void ExtractMap32toMap16(AssetBuilder *builder) {
  printf("  Extracting kMap32ToMap16 (4 assets from text file)...\n");

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
    // Read one line
    const char *line_end = ptr;
    while (line_end < end && *line_end != '\n' && *line_end != '\r')
      line_end++;

    size_t line_len = line_end - ptr;
    if (line_len >= sizeof(line_buf)) line_len = sizeof(line_buf) - 1;
    memcpy(line_buf, ptr, line_len);
    line_buf[line_len] = 0;

    // Skip newlines
    ptr = line_end;
    while (ptr < end && (*ptr == '\n' || *ptr == '\r'))
      ptr++;

    // Parse: "INDEX: v0, v1, v2, v3"
    int index;
    int v0, v1, v2, v3;
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
    LogError("Expected 8872 lines in map32_to_map16.txt, got %d", line_count);
    free(tab);
    return;
  }

  // Allocate result arrays (8872 / 4 = 2218 groups × 6 bytes = 13308 bytes each)
  #define MAP32_SIZE 13308
  uint8_t *res[4];
  for (int j = 0; j < 4; j++) {
    res[j] = malloc(MAP32_SIZE);
    if (!res[j]) {
      LogError("Failed to allocate Map32toMap16 buffer");
      for (int k = 0; k < j; k++) free(res[k]);
      free(tab);
      return;
    }
  }

  // Process in groups of 4 lines, distributing columns to 4 quadrants
  int out_pos = 0;
  for (int i = 0; i < 8872; i += 4) {
    for (int j = 0; j < 4; j++) {
      // Extract column j from lines i, i+1, i+2, i+3
      uint16_t quad[4] = {
        tab[i][j],
        tab[i+1][j],
        tab[i+2][j],
        tab[i+3][j]
      };
      PackMap32Quad(quad, &res[j][out_pos]);
    }
    out_pos += 6;
  }

  // Add assets
  AssetBuilder_AddAsset(builder, "kMap32ToMap16_0", ASSET_TYPE_UINT8, res[0], MAP32_SIZE);
  AssetBuilder_AddAsset(builder, "kMap32ToMap16_1", ASSET_TYPE_UINT8, res[1], MAP32_SIZE);
  AssetBuilder_AddAsset(builder, "kMap32ToMap16_2", ASSET_TYPE_UINT8, res[2], MAP32_SIZE);
  AssetBuilder_AddAsset(builder, "kMap32ToMap16_3", ASSET_TYPE_UINT8, res[3], MAP32_SIZE);

  printf("    Added kMap32ToMap16_0-3 (%d bytes each)\n", MAP32_SIZE);

  // Cleanup
  for (int j = 0; j < 4; j++) free(res[j]);
  free(tab);
}

static void TestMap32ToMap16(void) {
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

void ExtractLinkGraphics(AssetBuilder *builder) {
  printf("  Extracting kLinkGraphics from linksprite.png...\n");

  // Load PNG file from embedded assets or filesystem
  size_t png_size;
  unsigned char *png_data = LoadAssetData("assets/linksprite.png", &png_size);
  if (!png_data) {
    LogError("Failed to read assets/linksprite.png");
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

// Extract font from PNG and encode to SNES 2bpp format
// Returns: font_data (256*16 bytes), font_width (chars_per_lang bytes)
// Caller must free both returned pointers
static bool ExtractDialogueFontFromPNG(const char *lang, uint8_t **out_font_data,
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

  // Load PNG file from embedded assets or filesystem
  char path[256];
  snprintf(path, sizeof(path), "assets/%s", font_filename);

  size_t png_size;
  unsigned char *png_data = LoadAssetData(path, &png_size);
  if (!png_data) {
    LogError("Failed to read %s", path);
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
static char* GetDialogueFilename(const char *lang) {
  char *filename = malloc(64);
  if (strcmp(lang, "us") == 0) {
    snprintf(filename, 64, "assets/dialogue.txt");
  } else {
    // Replace '-' with '_' in language code for filename
    char lang_clean[16];
    strncpy(lang_clean, lang, sizeof(lang_clean) - 1);
    lang_clean[sizeof(lang_clean) - 1] = '\0';
    for (char *p = lang_clean; *p; p++) {
      if (*p == '-') *p = '_';
    }
    snprintf(filename, 64, "assets/dialogue_%s.txt", lang_clean);
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
void ExtractDialogueAssets(AssetBuilder *builder, const char *languages_arg) {
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
          return;
        }

        // Check dialogue file exists
        char *dialogue_filename = GetDialogueFilename(token);
        size_t dummy_size;
        char *test_data = (char *)LoadAssetData(dialogue_filename, &dummy_size);
        if (!test_data) {
          LogError("Dialogue file not found: %s (extract with --extract-dialogue first)", dialogue_filename);
          free(dialogue_filename);
          free(languages_copy);
          return;
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
      return;
    }

    // 2. Extract font
    uint8_t *font_data = NULL, *font_width = NULL;
    size_t font_width_count = 0;

    if (!ExtractDialogueFontFromPNG(lang, &font_data, &font_width, &font_width_count)) {
      LogError("Failed to extract dialogue font for %s", lang);
      for (size_t j = 0; j <= i; j++) free(all_dialogue[j]);
      for (size_t j = 0; j < i; j++) { free(all_fonts[j]); free(all_maps[j]); }
      free(all_dialogue); free(all_dialogue_lens);
      free(all_fonts); free(all_font_lens);
      free(all_maps); free(all_map_lens);
      if (languages_copy) free(languages_copy);
      return;
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
      return;
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
      return;
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
    return;
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
}

static void TestLinkGraphics(void) {
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

static void TestDungeonSprites(void) {
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
// Dungeon Sprites Extraction
// ============================================================================

void ExtractDungeonSprites(AssetBuilder *builder) {
  printf("  Extracting dungeon sprites from 320 rooms...\n");

  // Allocate buffers
  uint16_t *offsets = calloc(320, sizeof(uint16_t));
  uint8_t *data = malloc(64 * 1024);  // 64KB max
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
      LogError("Failed to load %s: %s", filename, Yaml_GetLastError());
      free(offsets);
      free(data);
      return;
    }

    YamlNode *root = Yaml_GetRoot(doc);
    YamlNode *header = Yaml_GetMapping(root, "Header");
    YamlNode *sprites = Yaml_GetMapping(root, "Sprites");

    if (!header || !sprites) {
      Yaml_Free(doc);
      continue;
    }

    // Get sort_sprites from Header
    int sortmode = Yaml_GetInt(header, "sort_sprites", 0);
    int sprite_count = Yaml_GetSequenceLength(sprites);

    // Skip if no sprites and sortmode == 0
    if (sprite_count == 0 && sortmode == 0) {
      Yaml_Free(doc);
      continue;
    }

    // Set offset and append sortmode
    offsets[room] = data_len;
    data[data_len++] = sortmode;

    // Process each sprite
    for (int i = 0; i < sprite_count; i++) {
      YamlNode *sprite = Yaml_GetSequence(sprites, i);
      if (!sprite || Yaml_GetSequenceLength(sprite) < 4) {
        continue;
      }

      // Parse sprite: [x, y, floor, name, optional_drop_type]
      YamlNode *xx_node = Yaml_GetSequence(sprite, 0);
      YamlNode *yy_node = Yaml_GetSequence(sprite, 1);
      YamlNode *floor_node = Yaml_GetSequence(sprite, 2);
      YamlNode *name_node = Yaml_GetSequence(sprite, 3);

      int xx = Yaml_AsInt(xx_node);
      int yy = Yaml_AsInt(yy_node);
      const char *floor_str = Yaml_AsString(floor_node);
      const char *name = Yaml_AsString(name_node);

      // Parse floor: "upper" = 0, "lower" = 1
      int f = (strcmp(floor_str, "lower") == 0) ? 1 : 0;

      // Parse subcode from name (e.g., "6D.3-Rat" -> ss=3, name="6D-Rat")
      int ss = 0;
      char name_buf[64];
      strncpy(name_buf, name, sizeof(name_buf) - 1);
      name_buf[sizeof(name_buf) - 1] = '\0';

      if (strlen(name_buf) > 2 && name_buf[2] == '.') {
        char *dash = strchr(name_buf + 3, '-');
        if (dash) {
          *dash = '\0';
          ss = atoi(name_buf + 3);
          // Reconstruct name: first 2 chars + dash + rest
          snprintf(name_buf, sizeof(name_buf), "%.2s-%s", name, dash + 1);
        }
      }

      // Look up sprite index
      int sprite_idx = FindSpriteIndex(name_buf);
      if (sprite_idx < 0) {
        LogError("Unknown sprite name: %s", name_buf);
        sprite_idx = 0;
      }

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

      // Check for optional drop type (5th element)
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

    // Append 0xff terminator
    data[data_len++] = 0xff;
    rooms_processed++;

    Yaml_Free(doc);
  }

  // Add assets
  AssetBuilder_AddAsset(builder, "kDungeonSprites", ASSET_TYPE_UINT8, data, data_len);
  AssetBuilder_AddAsset(builder, "kDungeonSpriteOffs", ASSET_TYPE_UINT16, (const uint8_t*)offsets, 320*2);

  printf("    Processed %d rooms, encoded %d sprites\n", rooms_processed, sprites_encoded);
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
// Dungeon Secrets Extraction
// ============================================================================

void ExtractDungeonSecrets(AssetBuilder *builder) {
  printf("  Extracting dungeon secrets from 320 rooms...\n");

  // Allocate result array: 320 rooms × 2 bytes offset each = 640 bytes
  uint8_t *result = calloc(640, 1);
  uint8_t *data = malloc(64 * 1024);  // Secret data (64KB max)
  size_t data_len = 0;

  int secrets_found = 0;

  // Process all 320 dungeon rooms
  for (int i = 0; i < 320; i++) {
    char filename[64];
    snprintf(filename, sizeof(filename), "assets/dungeon/dungeon-%d.yaml", i);

    YamlDoc *doc = LoadAssetYaml(filename);
    if (!doc) {
      continue;
    }

    YamlNode *root = Yaml_GetRoot(doc);
    YamlNode *secrets = Yaml_GetMapping(root, "Secrets");

    if (!secrets) {
      Yaml_Free(doc);
      continue;
    }

    int secret_count = Yaml_GetSequenceLength(secrets);

    // If room has secrets, set offset and encode them
    if (secret_count > 0) {
      // Set offset for this room (640 + data_len because offsets are relative to combined buffer)
      uint16_t offset = 640 + data_len;
      result[i * 2 + 0] = offset & 0xff;
      result[i * 2 + 1] = (offset >> 8) & 0xff;

      // Process each secret: [x, y, name]
      for (int j = 0; j < secret_count; j++) {
        YamlNode *secret = Yaml_GetSequence(secrets, j);
        if (!secret || Yaml_GetSequenceLength(secret) < 3) {
          continue;
        }

        // Parse [x, y, name]
        YamlNode *x_node = Yaml_GetSequence(secret, 0);
        YamlNode *y_node = Yaml_GetSequence(secret, 1);
        YamlNode *name_node = Yaml_GetSequence(secret, 2);

        int x = Yaml_AsInt(x_node);
        int y = Yaml_AsInt(y_node);
        const char *name = Yaml_AsString(name_node);

        // Look up secret index
        int secret_idx = FindSecretIndex(name);
        if (secret_idx < 0) {
          LogError("Unknown secret name: %s", name);
          secret_idx = 0;  // Default to "Nothing"
        }

        // Calculate position: (x + y * 64) * 2
        int pos = (x + y * 64) * 2;

        // Encode as 3 bytes: [pos_lo, pos_hi, secret_idx]
        data[data_len++] = pos & 0xff;
        data[data_len++] = (pos >> 8) & 0xff;
        data[data_len++] = secret_idx;

        secrets_found++;
      }

      // Append terminator [0xff, 0xff]
      data[data_len++] = 0xff;
      data[data_len++] = 0xff;
    }

    Yaml_Free(doc);
  }

  // For rooms with no secrets, set offset to point to end of data (640 + len - 2)
  uint16_t empty_offset = (640 + data_len) - 2;
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

  printf("    Processed 320 rooms, found %d secrets\n", secrets_found);
  printf("    kDungeonSecrets: %zu bytes (%d offsets + %zu data)\n",
         640 + data_len, 640, data_len);

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

void ExtractDungeonRoomHeaders(AssetBuilder *builder) {
  printf("  Extracting dungeon room headers from 320 rooms...\n");

  // Room headers data (deduplicated)
  uint8_t *headers = malloc(64 * 1024);
  size_t headers_len = 0;
  size_t headers_cap = 64 * 1024;

  // Offset for each room's header
  uint16_t *header_offsets = malloc(320 * sizeof(uint16_t));

  // Process all 320 dungeon rooms
  for (int i = 0; i < 320; i++) {
    char filename[64];
    snprintf(filename, sizeof(filename), "assets/dungeon/dungeon-%d.yaml", i);

    YamlDoc *doc = LoadAssetYaml(filename);
    if (!doc) {
      continue;
    }

    YamlNode *root = Yaml_GetRoot(doc);
    YamlNode *header_node = Yaml_GetMapping(root, "Header");
    if (!header_node) {
      Yaml_Free(doc);
      continue;
    }

    // Build 14-byte header
    uint8_t header[14];

    // Byte 0: bg2 (3 bits) << 5 | collision (3 bits) << 2 | lights_out (1 bit)
    const char *bg2_str = Yaml_GetString(header_node, "bg2", "");
    const char *collision_str = Yaml_GetString(header_node, "collision", "");
    int lights_out = Yaml_GetInt(header_node, "lights_out", 0);

    int bg2 = FindBg2Index(bg2_str);
    int collision = FindCollisionIndex(collision_str);
    header[0] = (bg2 << 5) | (collision << 2) | lights_out;

    // Bytes 1-3: palette, blockset, enemyblk
    header[1] = Yaml_GetInt(header_node, "palette", 0);
    header[2] = Yaml_GetInt(header_node, "blockset", 0);
    header[3] = Yaml_GetInt(header_node, "enemyblk", 0);

    // Byte 4: effect
    const char *effect_str = Yaml_GetString(header_node, "effect", "");
    header[4] = FindEffectIndex(effect_str);

    // Bytes 5-6: tag0, tag1
    const char *tag0_str = Yaml_GetString(header_node, "tag0", "");
    const char *tag1_str = Yaml_GetString(header_node, "tag1", "");
    header[5] = FindTagIndex(tag0_str);
    header[6] = FindTagIndex(tag1_str);

    // Bytes 7-13: hole/stair destinations
    // Parse [room, direction] arrays
    YamlNode *hole0 = Yaml_GetMapping(header_node, "hole0_dest");
    YamlNode *stair0 = Yaml_GetMapping(header_node, "stair0_dest");
    YamlNode *stair1 = Yaml_GetMapping(header_node, "stair1_dest");
    YamlNode *stair2 = Yaml_GetMapping(header_node, "stair2_dest");
    YamlNode *stair3 = Yaml_GetMapping(header_node, "stair3_dest");

    int hole0_room = 0, hole0_dir = 0;
    int stair0_room = 0, stair0_dir = 0;
    int stair1_room = 0, stair1_dir = 0;
    int stair2_room = 0, stair2_dir = 0;
    int stair3_room = 0, stair3_dir = 0;

    if (hole0 && Yaml_GetSequenceLength(hole0) >= 2) {
      hole0_room = Yaml_AsInt(Yaml_GetSequence(hole0, 0));
      hole0_dir = Yaml_AsInt(Yaml_GetSequence(hole0, 1));
    }
    if (stair0 && Yaml_GetSequenceLength(stair0) >= 2) {
      stair0_room = Yaml_AsInt(Yaml_GetSequence(stair0, 0));
      stair0_dir = Yaml_AsInt(Yaml_GetSequence(stair0, 1));
    }
    if (stair1 && Yaml_GetSequenceLength(stair1) >= 2) {
      stair1_room = Yaml_AsInt(Yaml_GetSequence(stair1, 0));
      stair1_dir = Yaml_AsInt(Yaml_GetSequence(stair1, 1));
    }
    if (stair2 && Yaml_GetSequenceLength(stair2) >= 2) {
      stair2_room = Yaml_AsInt(Yaml_GetSequence(stair2, 0));
      stair2_dir = Yaml_AsInt(Yaml_GetSequence(stair2, 1));
    }
    if (stair3 && Yaml_GetSequenceLength(stair3) >= 2) {
      stair3_room = Yaml_AsInt(Yaml_GetSequence(stair3, 0));
      stair3_dir = Yaml_AsInt(Yaml_GetSequence(stair3, 1));
    }

    header[7] = hole0_dir | (stair0_dir << 2) | (stair1_dir << 4) | (stair2_dir << 6);
    header[8] = stair3_dir;

    header[9] = hole0_room;
    header[10] = stair0_room;
    header[11] = stair1_room;
    header[12] = stair2_room;
    header[13] = stair3_room;

    // Deduplicate and store offset
    header_offsets[i] = AppendScanBytes(&headers, &headers_len, &headers_cap, header, 14);

    Yaml_Free(doc);
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

void ExtractDungeonRoomSimple(AssetBuilder *builder) {
  printf("  Extracting simple dungeon room data from 320 rooms...\n");

  // kDungeonRoomTeleMsg: 320 uint16 values (sign/teleport message IDs)
  uint16_t *sign_texts = calloc(320, sizeof(uint16_t));

  // kDungeonPitsHurtPlayer: Variable-length list of room indices
  uint16_t *pits_rooms = malloc(320 * sizeof(uint16_t));  // Max 320 rooms
  int pits_count = 0;

  // kDungeonRoomChests: Variable-length list (3 bytes per chest)
  uint8_t *chests = malloc(64 * 1024);  // Max reasonable size
  size_t chests_len = 0;
  int chest_count = 0;

  // Process all 320 dungeon rooms
  for (int i = 0; i < 320; i++) {
    char filename[64];
    snprintf(filename, sizeof(filename), "assets/dungeon/dungeon-%d.yaml", i);

    YamlDoc *doc = LoadAssetYaml(filename);
    if (!doc) {
      continue;
    }

    YamlNode *root = Yaml_GetRoot(doc);
    YamlNode *header = Yaml_GetMapping(root, "Header");
    if (!header) {
      Yaml_Free(doc);
      continue;
    }

    // Read tele_msg (sign/teleport message ID)
    sign_texts[i] = Yaml_GetInt(header, "tele_msg", 0);

    // Check if pits hurt player
    bool pits_hurt = Yaml_GetBool(header, "pits_hurt_player", false);
    if (pits_hurt) {
      pits_rooms[pits_count++] = i;
    }

    // Parse Chests array
    YamlNode *chests_node = Yaml_GetMapping(root, "Chests");
    if (chests_node) {
      int num_chests = Yaml_GetSequenceLength(chests_node);
      for (int j = 0; j < num_chests; j++) {
        YamlNode *chest = Yaml_GetSequence(chests_node, j);
        if (!chest) continue;

        // Check if it's a string (ends with '!') or integer
        const char *str = Yaml_AsString(chest);
        if (str && strlen(str) > 0 && str[strlen(str) - 1] == '!') {
          // String with '!' suffix - set 0x80 flag in room_hi
          int item_id = atoi(str);  // Parse "27!" -> 27
          chests[chests_len++] = i & 0xff;
          chests[chests_len++] = (i >> 8) | 0x80;
          chests[chests_len++] = item_id;
          chest_count++;
        } else {
          // Plain integer
          int item_id = Yaml_AsInt(chest);
          chests[chests_len++] = i & 0xff;
          chests[chests_len++] = (i >> 8) & 0xff;
          chests[chests_len++] = item_id;
          chest_count++;
        }
      }
    }

    Yaml_Free(doc);
  }

  // Add assets (Python order: Chests, TeleMsg, PitsHurtPlayer)
  AssetBuilder_AddAsset(builder, "kDungeonRoomChests", ASSET_TYPE_UINT8,
                       chests, chests_len);
  AssetBuilder_AddAsset(builder, "kDungeonRoomTeleMsg", ASSET_TYPE_UINT16,
                       (uint8_t*)sign_texts, 320*2);
  AssetBuilder_AddAsset(builder, "kDungeonPitsHurtPlayer", ASSET_TYPE_UINT16,
                       (uint8_t*)pits_rooms, pits_count*2);

  printf("    kDungeonRoomChests: %d chests (%zu bytes)\n",
         chest_count, chests_len);
  printf("    kDungeonRoomTeleMsg: 320 entries (640 bytes)\n");
  printf("    kDungeonPitsHurtPlayer: %d rooms where pits hurt (%d bytes)\n",
         pits_count, pits_count * 2);

  free(sign_texts);
  free(pits_rooms);
  free(chests);
}

// ============================================================================
// Dungeon Room Data (3-layer object encoding)
// ============================================================================

// Helper to encode a single layer and return door offset (or 0 if no doors)
// always_add_door_marker: if true, add door marker even if no doors (for Layer3)
static uint16_t EncodeRoomLayer(uint8_t **data_ptr, size_t *data_len, size_t *data_cap,
                                YamlNode *layer_objs, YamlNode *layer_doors,
                                bool always_add_door_marker) {
  uint8_t *data = *data_ptr;
  uint16_t door_offset = 0;

  if (!layer_objs) {
    // Empty layer, just add terminator
    if (*data_len + 2 > *data_cap) {
      *data_cap = (*data_len + 2) * 2;
      *data_ptr = realloc(*data_ptr, *data_cap);
      data = *data_ptr;
    }
    data[(*data_len)++] = 0xff;
    data[(*data_len)++] = 0xff;
    return 0;
  }

  // Encode objects
  int num_objs = Yaml_GetSequenceLength(layer_objs);
  for (int i = 0; i < num_objs; i++) {
    YamlNode *obj = Yaml_GetSequence(layer_objs, i);

    int x = Yaml_GetInt(obj, "x", 0);
    int y = Yaml_GetInt(obj, "y", 0);
    const char *name = Yaml_GetString(obj, "n", "");

    // Parse size (e.g., "3*1" → w=3, h=1), default to "0*0"
    const char *size_str = Yaml_GetString(obj, "s", "0*0");
    int w = 0, h = 0;
    if (size_str && strlen(size_str) >= 3) {
      w = size_str[0] - '0';
      h = size_str[2] - '0';
    }

    uint8_t p0, p1, p2;

    // Type0: Standard objects (0x00-0xF7)
    int idx = FindType0Index(name);
    if (idx >= 0) {
      p0 = x * 4 + w;
      p1 = y * 4 + h;
      p2 = idx;
    }
    // Type1: Extended objects (0xF80-0xFFF)
    else if ((idx = FindType1Index(name)) >= 0) {
      p0 = x * 4 + (idx & 3);
      p1 = y * 4 + ((idx >> 2) & 3);
      p2 = (idx >> 4) + 0xf8;
    }
    // Type2: Special objects (0x100-0x140)
    else if ((idx = FindType2Index(name)) >= 0) {
      p0 = 0xfc + ((x >> 4) & 3);
      p1 = ((x << 4) & 0xf0) | ((y >> 2) & 0x0f);
      p2 = idx | ((y << 6) & 0xc0);
    }
    else {
      fprintf(stderr, "Warning: Unknown object '%s' in room layer\n", name);
      continue;
    }

    // Append to buffer
    if (*data_len + 3 > *data_cap) {
      *data_cap = (*data_len + 3) * 2;
      *data_ptr = realloc(*data_ptr, *data_cap);
      data = *data_ptr;
    }
    data[(*data_len)++] = p0;
    data[(*data_len)++] = p1;
    data[(*data_len)++] = p2;
  }

  // Encode doors (if present, or if always_add_door_marker is set)
  int num_doors = layer_doors ? Yaml_GetSequenceLength(layer_doors) : 0;
  if (layer_doors || always_add_door_marker) {
    if (num_doors > 0 || always_add_door_marker) {
      // Door marker
      if (*data_len + 2 > *data_cap) {
        *data_cap = (*data_len + 2) * 2;
        *data_ptr = realloc(*data_ptr, *data_cap);
        data = *data_ptr;
      }
      data[(*data_len)++] = 0xf0;
      data[(*data_len)++] = 0xff;

      door_offset = *data_len;  // Record door offset

      // Encode each door: [dir | pos << 4, type]
      for (int i = 0; i < num_doors; i++) {
        YamlNode *door = Yaml_GetSequence(layer_doors, i);
        int dir = Yaml_GetInt(door, "dir", 0);
        int pos = Yaml_GetInt(door, "pos", 0);
        int type = Yaml_GetInt(door, "type", 0);

        if (*data_len + 2 > *data_cap) {
          *data_cap = (*data_len + 2) * 2;
          *data_ptr = realloc(*data_ptr, *data_cap);
          data = *data_ptr;
        }
        data[(*data_len)++] = dir | (pos << 4);
        data[(*data_len)++] = type;
      }
    }
  }

  // Layer terminator
  if (*data_len + 2 > *data_cap) {
    *data_cap = (*data_len + 2) * 2;
    *data_ptr = realloc(*data_ptr, *data_cap);
    data = *data_ptr;
  }
  data[(*data_len)++] = 0xff;
  data[(*data_len)++] = 0xff;

  return door_offset;
}

void ExtractDungeonRoomData(AssetBuilder *builder) {
  printf("  Extracting dungeon room data (3-layer encoding) from 320 rooms...\n");

  // Main room data buffer (NOT deduplicated, only headers are)
  uint8_t *room_data = malloc(512 * 1024);
  size_t room_data_len = 0;
  size_t room_data_cap = 512 * 1024;

  // Offset arrays
  uint16_t *room_offsets = malloc(320 * sizeof(uint16_t));
  uint16_t *door_offsets = malloc(320 * sizeof(uint16_t));

  // Temporary buffer for single room (before deduplication)
  uint8_t *temp_room = malloc(64 * 1024);
  size_t temp_room_cap = 64 * 1024;

  for (int i = 0; i < 320; i++) {
    char filename[64];
    snprintf(filename, sizeof(filename), "assets/dungeon/dungeon-%d.yaml", i);

    YamlDoc *doc = LoadAssetYaml(filename);
    if (!doc) {
      fprintf(stderr, "Warning: Could not load %s\n", filename);
      continue;
    }

    YamlNode *root = Yaml_GetRoot(doc);
    YamlNode *header = Yaml_GetMapping(root, "Header");

    // Build room data in temp buffer
    size_t temp_len = 0;

    // Byte 0: floor1 + floor2 * 16
    int floor1 = Yaml_GetInt(header, "floor1", 0);
    int floor2 = Yaml_GetInt(header, "floor2", 0);
    temp_room[temp_len++] = floor1 + floor2 * 16;

    // Byte 1: layout * 4 + start_quadrant
    int layout = Yaml_GetInt(header, "layout", 0);
    int start_quadrant = Yaml_GetInt(header, "start_quadrant", 0);
    temp_room[temp_len++] = layout * 4 + start_quadrant;

    // Layer 1
    YamlNode *layer1 = Yaml_GetMapping(root, "Layer1");
    YamlNode *layer1_doors = Yaml_GetMapping(root, "Layer1.doors");
    EncodeRoomLayer(&temp_room, &temp_len, &temp_room_cap, layer1, layer1_doors, false);

    // Layer 2
    YamlNode *layer2 = Yaml_GetMapping(root, "Layer2");
    YamlNode *layer2_doors = Yaml_GetMapping(root, "Layer2.doors");
    EncodeRoomLayer(&temp_room, &temp_len, &temp_room_cap, layer2, layer2_doors, false);

    // Layer 3 (always has door marker, even if no doors)
    YamlNode *layer3 = Yaml_GetMapping(root, "Layer3");
    YamlNode *layer3_doors = Yaml_GetMapping(root, "Layer3.doors");
    uint16_t door_off = EncodeRoomLayer(&temp_room, &temp_len, &temp_room_cap, layer3, layer3_doors, true);

    // Append directly (NO deduplication for room data, only headers are deduplicated)
    room_offsets[i] = room_data_len;

    // Ensure capacity
    if (room_data_len + temp_len > room_data_cap) {
      room_data_cap = (room_data_len + temp_len) * 2;
      room_data = realloc(room_data, room_data_cap);
    }

    // Append room data
    memcpy(room_data + room_data_len, temp_room, temp_len);
    room_data_len += temp_len;

    door_offsets[i] = door_off ? (room_offsets[i] + door_off) : 0;

    Yaml_Free(doc);
  }

  // Add assets
  AssetBuilder_AddAsset(builder, "kDungeonRoom", ASSET_TYPE_UINT8, room_data, room_data_len);
  AssetBuilder_AddAsset(builder, "kDungeonRoomOffs", ASSET_TYPE_UINT16,
                       (uint8_t*)room_offsets, 320*2);
  AssetBuilder_AddAsset(builder, "kDungeonRoomDoorOffs", ASSET_TYPE_UINT16,
                       (uint8_t*)door_offsets, 320*2);

  printf("    kDungeonRoom: %zu bytes from 320 rooms\n", room_data_len);
  printf("    kDungeonRoomOffs: 320 entries (640 bytes)\n");
  printf("    kDungeonRoomDoorOffs: 320 entries (640 bytes)\n");

  free(room_data);
  free(room_offsets);
  free(door_offsets);
  free(temp_room);
}

// ============================================================================
// Default and Overlay Dungeon Rooms
// ============================================================================

void ExtractDefaultOverlayRooms(AssetBuilder *builder) {
  printf("  Extracting default and overlay rooms...\n");

  // Default rooms: 8 variants from default_rooms.yaml
  YamlDoc *default_doc = LoadAssetYaml("assets/dungeon/default_rooms.yaml");
  if (!default_doc) {
    LogError("Failed to load default_rooms.yaml: %s", Yaml_GetLastError());
    return;
  }

  uint8_t *default_data = malloc(64 * 1024);
  size_t default_len = 0;
  size_t default_cap = 64 * 1024;
  uint16_t *default_offsets = malloc(8 * sizeof(uint16_t));

  YamlNode *default_root = Yaml_GetRoot(default_doc);

  for (int i = 0; i < 8; i++) {
    char key[32];
    snprintf(key, sizeof(key), "Default%d", i);
    YamlNode *variant = Yaml_GetMapping(default_root, key);

    if (!variant) {
      LogError("Default room variant %d not found", i);
      continue;
    }

    default_offsets[i] = default_len;
    EncodeRoomLayer(&default_data, &default_len, &default_cap, variant, NULL, false);
  }

  Yaml_Free(default_doc);

  AssetBuilder_AddAsset(builder, "kDungeonRoomDefault", ASSET_TYPE_UINT8,
                       default_data, default_len);
  AssetBuilder_AddAsset(builder, "kDungeonRoomDefaultOffs", ASSET_TYPE_UINT16,
                       (uint8_t*)default_offsets, 8 * sizeof(uint16_t));

  printf("    kDungeonRoomDefault: %zu bytes from 8 variants\n", default_len);
  printf("    kDungeonRoomDefaultOffs: 8 entries (16 bytes)\n");

  free(default_data);
  free(default_offsets);

  // Overlay rooms: 19 variants from overlay_rooms.yaml
  YamlDoc *overlay_doc = LoadAssetYaml("assets/dungeon/overlay_rooms.yaml");
  if (!overlay_doc) {
    LogError("Failed to load overlay_rooms.yaml: %s", Yaml_GetLastError());
    return;
  }

  uint8_t *overlay_data = malloc(64 * 1024);
  size_t overlay_len = 0;
  size_t overlay_cap = 64 * 1024;
  uint16_t *overlay_offsets = malloc(19 * sizeof(uint16_t));

  YamlNode *overlay_root = Yaml_GetRoot(overlay_doc);

  for (int i = 0; i < 19; i++) {
    char key[32];
    snprintf(key, sizeof(key), "Overlay%d", i);
    YamlNode *variant = Yaml_GetMapping(overlay_root, key);

    if (!variant) {
      LogError("Overlay room variant %d not found", i);
      continue;
    }

    overlay_offsets[i] = overlay_len;
    EncodeRoomLayer(&overlay_data, &overlay_len, &overlay_cap, variant, NULL, false);
  }

  Yaml_Free(overlay_doc);

  AssetBuilder_AddAsset(builder, "kDungeonRoomOverlay", ASSET_TYPE_UINT8,
                       overlay_data, overlay_len);
  AssetBuilder_AddAsset(builder, "kDungeonRoomOverlayOffs", ASSET_TYPE_UINT16,
                       (uint8_t*)overlay_offsets, 19 * sizeof(uint16_t));

  printf("    kDungeonRoomOverlay: %zu bytes from 19 variants\n", overlay_len);
  printf("    kDungeonRoomOverlayOffs: 19 entries (38 bytes)\n");

  free(overlay_data);
  free(overlay_offsets);
}
// Extract all entrances and starting points from dungeon YAML files
// Extract entrances and starting points - 33 assets total (indexed by YAML fields)
void ExtractEntrancesAndStartingPoints(AssetBuilder *builder) {
  printf("  Extracting entrances and starting points from 320 rooms...\n");

  typedef struct {
    uint16_t room;
    uint8_t rel_coords[8];
    uint16_t scroll_x, scroll_y, player_x, player_y, camera_x, camera_y;
    uint8_t blockset, doorway_orient, starting_bg, quad1, quad2, music;
    int8_t floor, palace;
    uint16_t door_settings;
    uint8_t assoc_entrance;
  } Entry;

  // Pre-allocate exact sizes based on Python
  Entry *entrances = calloc(133, sizeof(Entry));
  Entry *starting_pts = calloc(7, sizeof(Entry));

  for (int room = 0; room < 320; room++) {
    char path[256];
    snprintf(path, sizeof(path), "assets/dungeon/dungeon-%d.yaml", room);

    YamlDoc *doc = LoadAssetYaml(path);
    if (!doc) continue;

    YamlNode *root = Yaml_GetRoot(doc);
    if (!root) { Yaml_Free(doc); continue; }

    // Process Entrances - index by entrance_index field
    YamlNode *ent_list = Yaml_GetMapping(root, "Entrances");
    if (ent_list) {
      int n = Yaml_GetSequenceLength(ent_list);
      for (int i = 0; i < n; i++) {
        YamlNode *e = Yaml_GetSequence(ent_list, i);
        if (!e) continue;

        int idx = Yaml_GetInt(e, "entrance_index", -1);
        if (idx < 0 || idx >= 133) continue;

        Entry *ent = &entrances[idx];
        ent->room = room;

        YamlNode *scroll = Yaml_GetMapping(e, "scroll_xy");
        YamlNode *player = Yaml_GetMapping(e, "player_xy");
        YamlNode *camera = Yaml_GetMapping(e, "camera_xy");
        YamlNode *quads = Yaml_GetMapping(e, "quadrants");
        YamlNode *exit = Yaml_GetMapping(e, "house_exit_door");

        int base_x = ((room & 0xf) << 9), base_y = ((room & 0x1f0) << 5);

        YamlNode *sx = scroll ? Yaml_GetSequence(scroll, 0) : NULL;
        YamlNode *sy = scroll ? Yaml_GetSequence(scroll, 1) : NULL;
        YamlNode *px = player ? Yaml_GetSequence(player, 0) : NULL;
        YamlNode *py = player ? Yaml_GetSequence(player, 1) : NULL;
        YamlNode *cx = camera ? Yaml_GetSequence(camera, 0) : NULL;
        YamlNode *cy = camera ? Yaml_GetSequence(camera, 1) : NULL;

        ent->scroll_x = (sx ? Yaml_AsInt(sx) : 0) + base_x;
        ent->scroll_y = (sy ? Yaml_AsInt(sy) : 0) + base_y;
        ent->player_x = (px ? Yaml_AsInt(px) : 0) + base_x;
        ent->player_y = (py ? Yaml_AsInt(py) : 0) + base_y;
        ent->camera_x = cx ? Yaml_AsInt(cx) : 0;
        ent->camera_y = cy ? Yaml_AsInt(cy) : 0;

        ent->blockset = Yaml_GetInt(e, "blockset", 0);
        ent->floor = Yaml_GetInt(e, "floor", 0);

        const char *palace = Yaml_GetString(e, "palace", "None");
        int pidx = FindPalaceIndex(palace);
        ent->palace = (pidx == 0) ? -1 : (pidx - 1) * 2;

        ent->doorway_orient = Yaml_GetInt(e, "doorway_orientation", 0);
        ent->starting_bg = Yaml_GetInt(e, "plane", 0) + Yaml_GetInt(e, "ladder_level", 0) * 16;

        YamlNode *q0 = quads ? Yaml_GetSequence(quads, 0) : NULL;
        YamlNode *q1 = quads ? Yaml_GetSequence(quads, 1) : NULL;
        YamlNode *q2 = quads ? Yaml_GetSequence(quads, 2) : NULL;
        const char *q0s = q0 ? Yaml_AsString(q0) : "single_x";
        const char *q1s = q1 ? Yaml_AsString(q1) : "single_y";
        const char *q2s = q2 ? Yaml_AsString(q2) : "upper_left";

        ent->quad1 = (strcmp(q0s, "double_x") == 0 ? 1 : 0) * 0x20 + (strcmp(q1s, "double_y") == 0 ? 1 : 0) * 0x02;
        ent->quad2 = strcmp(q2s, "lower_left") == 0 ? 2 : strcmp(q2s, "upper_right") == 0 ? 16 : strcmp(q2s, "lower_right") == 0 ? 18 : 0;

        YamlNode *et = exit ? Yaml_GetSequence(exit, 0) : NULL;
        const char *etype = et ? Yaml_AsString(et) : "none";
        if (strcmp(etype, "none_0xffff") == 0) ent->door_settings = 0xffff;
        else if (strcmp(etype, "none") == 0) ent->door_settings = 0;
        else {
          YamlNode *ep = exit ? Yaml_GetSequence(exit, 1) : NULL;
          YamlNode *ed = exit ? Yaml_GetSequence(exit, 2) : NULL;
          uint16_t tval = strcmp(etype, "bombable") == 0 ? 1 : 0;
          ent->door_settings = (tval << 15) | ((ep ? Yaml_AsInt(ep) : 0) << 1) | ((ed ? Yaml_AsInt(ed) : 0) << 7);
        }

        ent->music = FindMusicIndex(Yaml_GetString(e, "music", "None"));

        // Relative coords
        int pxi = px ? Yaml_AsInt(px) : 0, pyi = py ? Yaml_AsInt(py) : 0;
        int bx = (room & 0xf) * 2, by = (room >> 4) * 2;
        int ym = (pyi & 0x100) >> 8, xm = (pxi & 0x100) >> 8;
        int qqq = (room >= 242 && strcmp(q0s, "single_x") == 0) ? xm : 0;
        int coords[8] = {by+ym, by, by+ym, by+1, bx+xm, bx+qqq, bx+xm, bx+qqq+1};

        YamlNode *repair = Yaml_GetMapping(e, "repair_scroll_bounds");
        if (repair) {
          int rlen = Yaml_GetSequenceLength(repair);
          for (int ri = 0; ri < 8 && ri < rlen; ri++) {
            YamlNode *rn = Yaml_GetSequence(repair, ri);
            if (rn) coords[ri] += Yaml_AsInt(rn);
          }
        }
        for (int ci = 0; ci < 8; ci++) ent->rel_coords[ci] = coords[ci];
      }
    }

    // Process StartingPoints - index by starting_point_index field
    YamlNode *sp_list = Yaml_GetMapping(root, "StartingPoints");
    if (sp_list) {
      int n = Yaml_GetSequenceLength(sp_list);
      for (int i = 0; i < n; i++) {
        YamlNode *s = Yaml_GetSequence(sp_list, i);
        if (!s) continue;

        int idx = Yaml_GetInt(s, "starting_point_index", -1);
        if (idx < 0 || idx >= 7) continue;

        Entry *ent = &starting_pts[idx];
        ent->room = room;

        YamlNode *scroll = Yaml_GetMapping(s, "scroll_xy");
        YamlNode *player = Yaml_GetMapping(s, "player_xy");
        YamlNode *camera = Yaml_GetMapping(s, "camera_xy");
        YamlNode *quads = Yaml_GetMapping(s, "quadrants");
        YamlNode *exit = Yaml_GetMapping(s, "house_exit_door");

        int base_x = ((room & 0xf) << 9), base_y = ((room & 0x1f0) << 5);

        YamlNode *sx = scroll ? Yaml_GetSequence(scroll, 0) : NULL;
        YamlNode *sy = scroll ? Yaml_GetSequence(scroll, 1) : NULL;
        YamlNode *px = player ? Yaml_GetSequence(player, 0) : NULL;
        YamlNode *py = player ? Yaml_GetSequence(player, 1) : NULL;
        YamlNode *cx = camera ? Yaml_GetSequence(camera, 0) : NULL;
        YamlNode *cy = camera ? Yaml_GetSequence(camera, 1) : NULL;

        ent->scroll_x = (sx ? Yaml_AsInt(sx) : 0) + base_x;
        ent->scroll_y = (sy ? Yaml_AsInt(sy) : 0) + base_y;
        ent->player_x = (px ? Yaml_AsInt(px) : 0) + base_x;
        ent->player_y = (py ? Yaml_AsInt(py) : 0) + base_y;
        ent->camera_x = cx ? Yaml_AsInt(cx) : 0;
        ent->camera_y = cy ? Yaml_AsInt(cy) : 0;

        ent->blockset = Yaml_GetInt(s, "blockset", 0);
        ent->floor = Yaml_GetInt(s, "floor", 0);

        const char *palace = Yaml_GetString(s, "palace", "None");
        int pidx = FindPalaceIndex(palace);
        ent->palace = (pidx == 0) ? -1 : (pidx - 1) * 2;

        ent->doorway_orient = Yaml_GetInt(s, "doorway_orientation", 0);
        ent->starting_bg = Yaml_GetInt(s, "plane", 0) + Yaml_GetInt(s, "ladder_level", 0) * 16;

        YamlNode *q0 = quads ? Yaml_GetSequence(quads, 0) : NULL;
        YamlNode *q1 = quads ? Yaml_GetSequence(quads, 1) : NULL;
        YamlNode *q2 = quads ? Yaml_GetSequence(quads, 2) : NULL;
        const char *q0s = q0 ? Yaml_AsString(q0) : "single_x";
        const char *q1s = q1 ? Yaml_AsString(q1) : "single_y";
        const char *q2s = q2 ? Yaml_AsString(q2) : "upper_left";

        ent->quad1 = (strcmp(q0s, "double_x") == 0 ? 1 : 0) * 0x20 + (strcmp(q1s, "double_y") == 0 ? 1 : 0) * 0x02;
        ent->quad2 = strcmp(q2s, "lower_left") == 0 ? 2 : strcmp(q2s, "upper_right") == 0 ? 16 : strcmp(q2s, "lower_right") == 0 ? 18 : 0;

        YamlNode *et = exit ? Yaml_GetSequence(exit, 0) : NULL;
        const char *etype = et ? Yaml_AsString(et) : "none";
        if (strcmp(etype, "none_0xffff") == 0) ent->door_settings = 0xffff;
        else if (strcmp(etype, "none") == 0) ent->door_settings = 0;
        else {
          YamlNode *ep = exit ? Yaml_GetSequence(exit, 1) : NULL;
          YamlNode *ed = exit ? Yaml_GetSequence(exit, 2) : NULL;
          uint16_t tval = strcmp(etype, "bombable") == 0 ? 1 : 0;
          ent->door_settings = (tval << 15) | ((ep ? Yaml_AsInt(ep) : 0) << 1) | ((ed ? Yaml_AsInt(ed) : 0) << 7);
        }

        ent->music = FindMusicIndex(Yaml_GetString(s, "music", "None"));
        ent->assoc_entrance = Yaml_GetInt(s, "associated_entrance_index", 0);

        // Relative coords
        int pxi = px ? Yaml_AsInt(px) : 0, pyi = py ? Yaml_AsInt(py) : 0;
        int bx = (room & 0xf) * 2, by = (room >> 4) * 2;
        int ym = (pyi & 0x100) >> 8, xm = (pxi & 0x100) >> 8;
        int qqq = (room >= 242 && strcmp(q0s, "single_x") == 0) ? xm : 0;
        int coords[8] = {by+ym, by, by+ym, by+1, bx+xm, bx+qqq, bx+xm, bx+qqq+1};

        YamlNode *repair = Yaml_GetMapping(s, "repair_scroll_bounds");
        if (repair) {
          int rlen = Yaml_GetSequenceLength(repair);
          for (int ri = 0; ri < 8 && ri < rlen; ri++) {
            YamlNode *rn = Yaml_GetSequence(repair, ri);
            if (rn) coords[ri] += Yaml_AsInt(rn);
          }
        }
        for (int ci = 0; ci < 8; ci++) ent->rel_coords[ci] = coords[ci];
      }
    }

    Yaml_Free(doc);
  }

  printf("    Extracted 133 entrances, 7 starting points (indexed)\n");

  // Build entrance assets - 16 assets, 133 entries each
  uint16_t *e_rooms = malloc(133 * 2);
  uint8_t *e_rel = malloc(133 * 8);
  uint16_t *e_sx = malloc(133 * 2), *e_sy = malloc(133 * 2);
  uint16_t *e_px = malloc(133 * 2), *e_py = malloc(133 * 2);
  uint16_t *e_cx = malloc(133 * 2), *e_cy = malloc(133 * 2);
  uint8_t *e_blk = malloc(133), *e_dor = malloc(133), *e_bg = malloc(133);
  uint8_t *e_q1 = malloc(133), *e_q2 = malloc(133), *e_mus = malloc(133);
  int8_t *e_flr = malloc(133), *e_pal = malloc(133);
  uint16_t *e_door = malloc(133 * 2);

  for (int i = 0; i < 133; i++) {
    e_rooms[i] = entrances[i].room;
    memcpy(&e_rel[i*8], entrances[i].rel_coords, 8);
    e_sx[i] = entrances[i].scroll_x; e_sy[i] = entrances[i].scroll_y;
    e_px[i] = entrances[i].player_x; e_py[i] = entrances[i].player_y;
    e_cx[i] = entrances[i].camera_x; e_cy[i] = entrances[i].camera_y;
    e_blk[i] = entrances[i].blockset; e_flr[i] = entrances[i].floor;
    e_pal[i] = entrances[i].palace; e_dor[i] = entrances[i].doorway_orient;
    e_bg[i] = entrances[i].starting_bg; e_q1[i] = entrances[i].quad1;
    e_q2[i] = entrances[i].quad2; e_door[i] = entrances[i].door_settings;
    e_mus[i] = entrances[i].music;
  }
  
  AssetBuilder_AddAsset(builder, "kEntranceData_rooms", ASSET_TYPE_UINT16, (uint8_t*)e_rooms, 133*2);
  AssetBuilder_AddAsset(builder, "kEntranceData_relativeCoords", ASSET_TYPE_UINT8, e_rel, 133*8);
  AssetBuilder_AddAsset(builder, "kEntranceData_scrollX", ASSET_TYPE_UINT16, (uint8_t*)e_sx, 133*2);
  AssetBuilder_AddAsset(builder, "kEntranceData_scrollY", ASSET_TYPE_UINT16, (uint8_t*)e_sy, 133*2);
  AssetBuilder_AddAsset(builder, "kEntranceData_playerX", ASSET_TYPE_UINT16, (uint8_t*)e_px, 133*2);
  AssetBuilder_AddAsset(builder, "kEntranceData_playerY", ASSET_TYPE_UINT16, (uint8_t*)e_py, 133*2);
  AssetBuilder_AddAsset(builder, "kEntranceData_cameraX", ASSET_TYPE_UINT16, (uint8_t*)e_cx, 133*2);
  AssetBuilder_AddAsset(builder, "kEntranceData_cameraY", ASSET_TYPE_UINT16, (uint8_t*)e_cy, 133*2);
  AssetBuilder_AddAsset(builder, "kEntranceData_blockset", ASSET_TYPE_UINT8, e_blk, 133);
  AssetBuilder_AddAsset(builder, "kEntranceData_floor", ASSET_TYPE_INT8, (uint8_t*)e_flr, 133);
  AssetBuilder_AddAsset(builder, "kEntranceData_palace", ASSET_TYPE_INT8, (uint8_t*)e_pal, 133);
  AssetBuilder_AddAsset(builder, "kEntranceData_doorwayOrientation", ASSET_TYPE_UINT8, e_dor, 133);
  AssetBuilder_AddAsset(builder, "kEntranceData_startingBg", ASSET_TYPE_UINT8, e_bg, 133);
  AssetBuilder_AddAsset(builder, "kEntranceData_quadrant1", ASSET_TYPE_UINT8, e_q1, 133);
  AssetBuilder_AddAsset(builder, "kEntranceData_quadrant2", ASSET_TYPE_UINT8, e_q2, 133);
  AssetBuilder_AddAsset(builder, "kEntranceData_doorSettings", ASSET_TYPE_UINT16, (uint8_t*)e_door, 133*2);
  AssetBuilder_AddAsset(builder, "kEntranceData_musicTrack", ASSET_TYPE_UINT8, e_mus, 133);
  
  free(e_rooms); free(e_rel); free(e_sx); free(e_sy); free(e_px); free(e_py);
  free(e_cx); free(e_cy); free(e_blk); free(e_flr); free(e_pal); free(e_dor);
  free(e_bg); free(e_q1); free(e_q2); free(e_door); free(e_mus);

  printf("    Added 16 entrance assets (133 entries each)\n");

  // Build starting point assets - 17 assets, 7 entries each
  uint16_t *s_rooms = malloc(7 * 2);
  uint8_t *s_rel = malloc(7 * 8);
  uint16_t *s_sx = malloc(7 * 2), *s_sy = malloc(7 * 2);
  uint16_t *s_px = malloc(7 * 2), *s_py = malloc(7 * 2);
  uint16_t *s_cx = malloc(7 * 2), *s_cy = malloc(7 * 2);
  uint8_t *s_blk = malloc(7), *s_dor = malloc(7), *s_bg = malloc(7);
  uint8_t *s_q1 = malloc(7), *s_q2 = malloc(7), *s_mus = malloc(7);
  uint8_t *s_ent = malloc(7);
  int8_t *s_flr = malloc(7), *s_pal = malloc(7);
  uint16_t *s_door = malloc(7 * 2);

  for (int i = 0; i < 7; i++) {
    s_rooms[i] = starting_pts[i].room;
    memcpy(&s_rel[i*8], starting_pts[i].rel_coords, 8);
    s_sx[i] = starting_pts[i].scroll_x; s_sy[i] = starting_pts[i].scroll_y;
    s_px[i] = starting_pts[i].player_x; s_py[i] = starting_pts[i].player_y;
    s_cx[i] = starting_pts[i].camera_x; s_cy[i] = starting_pts[i].camera_y;
    s_blk[i] = starting_pts[i].blockset; s_flr[i] = starting_pts[i].floor;
    s_pal[i] = starting_pts[i].palace; s_dor[i] = starting_pts[i].doorway_orient;
    s_bg[i] = starting_pts[i].starting_bg; s_q1[i] = starting_pts[i].quad1;
    s_q2[i] = starting_pts[i].quad2; s_door[i] = starting_pts[i].door_settings;
    s_ent[i] = starting_pts[i].assoc_entrance; s_mus[i] = starting_pts[i].music;
  }
  
  AssetBuilder_AddAsset(builder, "kStartingPoint_rooms", ASSET_TYPE_UINT16, (uint8_t*)s_rooms, 7*2);
  AssetBuilder_AddAsset(builder, "kStartingPoint_relativeCoords", ASSET_TYPE_UINT8, s_rel, 7*8);
  AssetBuilder_AddAsset(builder, "kStartingPoint_scrollX", ASSET_TYPE_UINT16, (uint8_t*)s_sx, 7*2);
  AssetBuilder_AddAsset(builder, "kStartingPoint_scrollY", ASSET_TYPE_UINT16, (uint8_t*)s_sy, 7*2);
  AssetBuilder_AddAsset(builder, "kStartingPoint_playerX", ASSET_TYPE_UINT16, (uint8_t*)s_px, 7*2);
  AssetBuilder_AddAsset(builder, "kStartingPoint_playerY", ASSET_TYPE_UINT16, (uint8_t*)s_py, 7*2);
  AssetBuilder_AddAsset(builder, "kStartingPoint_cameraX", ASSET_TYPE_UINT16, (uint8_t*)s_cx, 7*2);
  AssetBuilder_AddAsset(builder, "kStartingPoint_cameraY", ASSET_TYPE_UINT16, (uint8_t*)s_cy, 7*2);
  AssetBuilder_AddAsset(builder, "kStartingPoint_blockset", ASSET_TYPE_UINT8, s_blk, 7);
  AssetBuilder_AddAsset(builder, "kStartingPoint_floor", ASSET_TYPE_INT8, (uint8_t*)s_flr, 7);
  AssetBuilder_AddAsset(builder, "kStartingPoint_palace", ASSET_TYPE_INT8, (uint8_t*)s_pal, 7);
  AssetBuilder_AddAsset(builder, "kStartingPoint_doorwayOrientation", ASSET_TYPE_UINT8, s_dor, 7);
  AssetBuilder_AddAsset(builder, "kStartingPoint_startingBg", ASSET_TYPE_UINT8, s_bg, 7);
  AssetBuilder_AddAsset(builder, "kStartingPoint_quadrant1", ASSET_TYPE_UINT8, s_q1, 7);
  AssetBuilder_AddAsset(builder, "kStartingPoint_quadrant2", ASSET_TYPE_UINT8, s_q2, 7);
  AssetBuilder_AddAsset(builder, "kStartingPoint_doorSettings", ASSET_TYPE_UINT16, (uint8_t*)s_door, 7*2);
  AssetBuilder_AddAsset(builder, "kStartingPoint_entrance", ASSET_TYPE_UINT8, s_ent, 7);
  AssetBuilder_AddAsset(builder, "kStartingPoint_musicTrack", ASSET_TYPE_UINT8, s_mus, 7);
  
  free(s_rooms); free(s_rel); free(s_sx); free(s_sy); free(s_px); free(s_py);
  free(s_cx); free(s_cy); free(s_blk); free(s_flr); free(s_pal); free(s_dor);
  free(s_bg); free(s_q1); free(s_q2); free(s_door); free(s_ent); free(s_mus);

  printf("    Added 17 starting point assets (7 entries each)\n");

  free(entrances);
  free(starting_pts);
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

// Pack music and ambient sound into single byte
static uint8_t Overworld_GetMusicByte(YamlNode *h, const char *tag) {
  YamlNode *music_node = Yaml_GetMapping(h, "music");
  YamlNode *ambient_node = Yaml_GetMapping(h, "ambient");
  if (!music_node || !ambient_node) return 0;

  const char *music = Yaml_GetString(music_node, tag, "None");
  const char *ambient = Yaml_GetString(ambient_node, tag, "None");

  return FindMusicIndex(music) | (FindAmbientSoundIndex(ambient) << 4);
}

// Write with mirroring for big maps (uint8)
static void Overworld_Awrite(uint8_t *arr, uint8_t *map_is_small, int area, int key, uint8_t value) {
  arr[key] = value;
  if (area < 128 && map_is_small[area] == 0) {
    arr[key + 1] = value;
    arr[key + 8] = value;
    arr[key + 9] = value;
  }
}

// Write with mirroring for big maps (uint16)
static void Overworld_Awrite16(uint16_t *arr, uint8_t *map_is_small, int area, int key, uint16_t value) {
  arr[key] = value;
  if (area < 128 && map_is_small[area] == 0) {
    arr[key + 1] = value;
    arr[key + 8] = value;
    arr[key + 9] = value;
  }
}

// Helper for sprite stage processing (Phase 7)
static void Overworld_ProcessSpriteStage(Rom *rom, uint8_t *map_is_small,
                                         uint16_t *sprite_offs, uint8_t *sprite_gfx, uint8_t *sprite_pal,
                                         uint8_t *sprite_data, int *sprite_len,
                                         int start, int end, const char *stage_name,
                                         int *stage_idxs, int num_stages, int info_stage) {
  for (int i = start; i < end; i++) {
    if (!Overworld_IsAreaHead(rom, i)) continue;

    char path[256];
    snprintf(path, sizeof(path), "assets/overworld/overworld-%d.yaml", i);
    YamlDoc *doc = LoadAssetYaml(path);
    if (!doc) continue;

    YamlNode *root = Yaml_GetRoot(doc);
    if (!root) { Yaml_Free(doc); continue; }

    YamlNode *stage = Yaml_GetMapping(root, stage_name);
    if (!stage) { Yaml_Free(doc); continue; }

    // Info (gfx + palette)
    YamlNode *info = Yaml_GetMapping(stage, "info");
    if (info && i < 128) {
      int gfx = Yaml_GetInt(info, "gfx", 0);
      int pal = Yaml_GetInt(info, "palette", 0);
      int idx = (i & 63) + info_stage * 64;
      sprite_gfx[idx] = gfx;
      sprite_pal[idx] = pal;

      if (map_is_small[i] == 0) {
        sprite_gfx[idx + 1] = gfx;
        sprite_gfx[idx + 8] = gfx;
        sprite_gfx[idx + 9] = gfx;
        sprite_pal[idx + 1] = pal;
        sprite_pal[idx + 8] = pal;
        sprite_pal[idx + 9] = pal;
      }
    }

    // Sprites list
    YamlNode *sprites = Yaml_GetMapping(stage, "sprites");
    if (sprites && Yaml_GetSequenceLength(sprites) > 0) {
      int n = Yaml_GetSequenceLength(sprites);

      // Set offset for all stages this sprite list applies to
      for (int si = 0; si < num_stages; si++) {
        sprite_offs[stage_idxs[si] * 144 + i] = *sprite_len;
      }

      for (int si = 0; si < n; si++) {
        YamlNode *s = Yaml_GetSequence(sprites, si);
        if (!s || Yaml_GetSequenceLength(s) < 3) continue;

        int x = Yaml_AsInt(Yaml_GetSequence(s, 0));
        int y = Yaml_AsInt(Yaml_GetSequence(s, 1));
        const char *sprite_name = Yaml_AsString(Yaml_GetSequence(s, 2));
        int sprite_id = FindSpriteIndex(sprite_name);

        sprite_data[(*sprite_len)++] = y;
        sprite_data[(*sprite_len)++] = x;
        sprite_data[(*sprite_len)++] = sprite_id;
      }

      sprite_data[(*sprite_len)++] = 0xff;  // Terminator
    }

    Yaml_Free(doc);
  }
}

// ============================================================================
// Overworld YAML Extraction
// ============================================================================

// Extract overworld data from 160 YAML files (~48 assets)
void ExtractOverworldYAML(AssetBuilder *builder, Rom *rom) {
  printf("  Extracting overworld YAML data (160 areas)...\n");

  // Allocate map_is_small FIRST - needed by multiple phases
  uint8_t *map_is_small = calloc(192, 1);

  // ======================================================================
  // Phase 1: Header data (6 assets)
  // ======================================================================

  uint8_t *aux_tile_theme = calloc(128, 1);
  uint8_t *bg_palettes = calloc(136, 1);
  uint16_t *sign_text = calloc(128, 2);
  uint8_t *music_sets = calloc(256, 1);
  uint8_t *music_sets2 = calloc(96, 1);

  int area_count = 0;
  for (int i = 0; i < 160; i++) {
    if (!Overworld_IsAreaHead(rom, i)) continue;

    char path[256];
    snprintf(path, sizeof(path), "assets/overworld/overworld-%d.yaml", i);

    YamlDoc *doc = LoadAssetYaml(path);
    if (!doc) continue;

    YamlNode *root = Yaml_GetRoot(doc);
    if (!root) { Yaml_Free(doc); continue; }

    YamlNode *header = Yaml_GetMapping(root, "Header");
    if (!header) { Yaml_Free(doc); continue; }

    area_count++;

    const char *size = Yaml_GetString(header, "size", "small");
    map_is_small[i] = (strcmp(size, "small") == 0) ? 1 : 0;

    // Python checks: if i < len(array)
    if (i < 128) {  // aux_tile_theme and sign_text are size 128
      Overworld_Awrite(aux_tile_theme, map_is_small, i, i, Yaml_GetInt(header, "gfx", 0));
      Overworld_Awrite16(sign_text, map_is_small, i, i, Yaml_GetInt(header, "sign_text", 0));
    }
    if (i < 136) {  // bg_palettes is size 136
      Overworld_Awrite(bg_palettes, map_is_small, i, i, Yaml_GetInt(header, "palette", 0));
    }

    if (i < 64) {
      uint8_t mb = Overworld_GetMusicByte(header, "beginning");
      uint8_t mz = Overworld_GetMusicByte(header, "zelda");
      uint8_t ms = Overworld_GetMusicByte(header, "sword");
      uint8_t ma = Overworld_GetMusicByte(header, "agahnim");

      music_sets[i] = mb; music_sets[i + 64] = mz;
      music_sets[i + 128] = ms; music_sets[i + 192] = ma;

      if (map_is_small[i] == 0) {
        music_sets[i + 1] = mb; music_sets[i + 8] = mb; music_sets[i + 9] = mb;
        music_sets[i + 65] = mz; music_sets[i + 72] = mz; music_sets[i + 73] = mz;
        music_sets[i + 129] = ms; music_sets[i + 136] = ms; music_sets[i + 137] = ms;
        music_sets[i + 193] = ma; music_sets[i + 200] = ma; music_sets[i + 201] = ma;
      }
    } else if (i >= 64 && i < 160) {
      uint8_t ma = Overworld_GetMusicByte(header, "agahnim");
      music_sets2[i - 64] = ma;
      // Python: awrite checks 'if area < 128' before expanding for large areas
      // So areas 128-159 never expand to adjacent indices
      if (i < 128 && map_is_small[i] == 0) {
        music_sets2[i - 63] = ma;
        music_sets2[i - 56] = ma;
        music_sets2[i - 55] = ma;
      }
    }

    Yaml_Free(doc);
  }

  AssetBuilder_AddAsset(builder, "kOverworldMapIsSmall", ASSET_TYPE_UINT8, map_is_small, 192);
  AssetBuilder_AddAsset(builder, "kOverworldAuxTileThemeIndexes", ASSET_TYPE_UINT8, aux_tile_theme, 128);
  AssetBuilder_AddAsset(builder, "kOverworldBgPalettes", ASSET_TYPE_UINT8, bg_palettes, 136);
  AssetBuilder_AddAsset(builder, "kOverworld_SignText", ASSET_TYPE_UINT16, (uint8_t*)sign_text, 128 * 2);
  AssetBuilder_AddAsset(builder, "kOwMusicSets", ASSET_TYPE_UINT8, music_sets, 256);
  AssetBuilder_AddAsset(builder, "kOwMusicSets2", ASSET_TYPE_UINT8, music_sets2, 96);

  free(aux_tile_theme); free(bg_palettes); free(sign_text);
  free(music_sets); free(music_sets2);

  printf("    Phase 1: Added 6 header assets from %d areas\n", area_count);

  // ======================================================================
  // Phase 2: Travel data (9 assets) - Bird travel + whirlpools
  // ======================================================================

  uint16_t *bird_screen = calloc(17, 2);
  uint16_t *bird_load = calloc(17, 2);
  uint16_t *bird_sx = calloc(17, 2), *bird_sy = calloc(17, 2);
  uint16_t *bird_px = calloc(17, 2), *bird_py = calloc(17, 2);
  uint16_t *bird_cx = calloc(17, 2), *bird_cy = calloc(17, 2);
  int8_t *bird_unk1 = calloc(17, 1), *bird_unk3 = calloc(17, 1);
  uint16_t *whirlpool_areas = calloc(8, 2);

  int next_whirlpool_id = 0;

  printf("    Processing travel data from areas...\n");
  for (int i = 0; i < 160; i++) {
    if (!Overworld_IsAreaHead(rom, i)) continue;

    char path[256];
    snprintf(path, sizeof(path), "assets/overworld/overworld-%d.yaml", i);
    YamlDoc *doc = LoadAssetYaml(path);
    if (!doc) continue;

    YamlNode *root = Yaml_GetRoot(doc);
    if (!root) { Yaml_Free(doc); continue; }

    YamlNode *travel = Yaml_GetMapping(root, "Travel");
    if (travel) {
      int n = Yaml_GetSequenceLength(travel);
      for (int ti = 0; ti < n; ti++) {
        YamlNode *t = Yaml_GetSequence(travel, ti);
        if (!t) continue;

        int j;
        if (Yaml_HasKey(t, "bird_travel_id")) {
          j = Yaml_GetInt(t, "bird_travel_id", 0);
        } else {
          // Whirlpool
          whirlpool_areas[next_whirlpool_id] = Yaml_GetInt(t, "whirlpool_src_area", 0);
          j = next_whirlpool_id + 9;
          next_whirlpool_id++;
        }

        int base_x = (i & 7) << 9;
        int base_y = (i & 56) << 6;

        bird_screen[j] = i;

        // get_loadoffs calculation
        YamlNode *scroll = Yaml_GetMapping(t, "scroll_xy");
        YamlNode *load = Yaml_GetMapping(t, "load_xy");
        int sx = scroll ? Yaml_AsInt(Yaml_GetSequence(scroll, 0)) : 0;
        int sy = scroll ? Yaml_AsInt(Yaml_GetSequence(scroll, 1)) : 0;
        int lx = load ? Yaml_AsInt(Yaml_GetSequence(load, 0)) : 0;
        int ly = load ? Yaml_AsInt(Yaml_GetSequence(load, 1)) : 0;
        int x = (sx >> 4) + lx;
        int y = (sy >> 4) + ly;
        bird_load[j] = ((y & 0x3f) << 7) | ((x & 0x3f) << 1);

        bird_sx[j] = sx + base_x;
        bird_sy[j] = sy + base_y;

        YamlNode *xy = Yaml_GetMapping(t, "xy");
        int px = xy ? Yaml_AsInt(Yaml_GetSequence(xy, 0)) : 0;
        int py = xy ? Yaml_AsInt(Yaml_GetSequence(xy, 1)) : 0;
        bird_px[j] = px + base_x;
        bird_py[j] = py + base_y;

        YamlNode *camera = Yaml_GetMapping(t, "camera_xy");
        int cx = camera ? Yaml_AsInt(Yaml_GetSequence(camera, 0)) : 0;
        int cy = camera ? Yaml_AsInt(Yaml_GetSequence(camera, 1)) : 0;
        bird_cx[j] = cx + base_x;
        bird_cy[j] = cy + base_y;

        YamlNode *unk = Yaml_GetMapping(t, "unk");
        bird_unk1[j] = unk ? Yaml_AsInt(Yaml_GetSequence(unk, 0)) : 0;
        bird_unk3[j] = unk ? Yaml_AsInt(Yaml_GetSequence(unk, 1)) : 0;
      }
    }

    Yaml_Free(doc);
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

  printf("    Phase 2: Added 9 travel assets (17 bird travel + 8 whirlpools)\n");

  // ======================================================================
  // Phase 3: Entrances (3 assets) - Overworld entrances indexed by 'index' field
  // ======================================================================

  uint16_t *ent_area = calloc(129, 2);
  uint16_t *ent_pos = calloc(129, 2);
  uint8_t *ent_id = calloc(129, 1);

  printf("    Processing entrances from areas...\n");
  for (int i = 0; i < 160; i++) {
    if (!Overworld_IsAreaHead(rom, i)) continue;

    char path[256];
    snprintf(path, sizeof(path), "assets/overworld/overworld-%d.yaml", i);
    YamlDoc *doc = LoadAssetYaml(path);
    if (!doc) continue;

    YamlNode *root = Yaml_GetRoot(doc);
    if (!root) { Yaml_Free(doc); continue; }

    YamlNode *entrances = Yaml_GetMapping(root, "Entrances");
    if (entrances) {
      int n = Yaml_GetSequenceLength(entrances);
      for (int ei = 0; ei < n; ei++) {
        YamlNode *e = Yaml_GetSequence(entrances, ei);
        if (!e) continue;

        int j = Yaml_GetInt(e, "index", -1);
        if (j < 0 || j >= 129) continue;

        ent_area[j] = i;
        ent_id[j] = Yaml_GetInt(e, "entrance_id", 0);

        int x = Yaml_GetInt(e, "x", 0);
        int y = Yaml_GetInt(e, "y", 0);
        ent_pos[j] = (x << 1) | (y << 7);
      }
    }

    Yaml_Free(doc);
  }

  AssetBuilder_AddAsset(builder, "kOverworld_Entrance_Area", ASSET_TYPE_UINT16, (uint8_t*)ent_area, 129*2);
  AssetBuilder_AddAsset(builder, "kOverworld_Entrance_Pos", ASSET_TYPE_UINT16, (uint8_t*)ent_pos, 129*2);
  AssetBuilder_AddAsset(builder, "kOverworld_Entrance_Id", ASSET_TYPE_UINT8, ent_id, 129);

  free(ent_area); free(ent_pos); free(ent_id);

  printf("    Phase 3: Added 3 entrance assets (129 entries each)\n");

  // ======================================================================
  // Phase 4: Holes (3 assets) - Fall holes sorted by entrance_id
  // ======================================================================

  Overworld_Hole *holes = malloc(100 * sizeof(Overworld_Hole));
  int hole_count = 0;

  printf("    Processing holes from areas...\n");
  for (int i = 0; i < 160; i++) {
    if (!Overworld_IsAreaHead(rom, i)) continue;

    char path[256];
    snprintf(path, sizeof(path), "assets/overworld/overworld-%d.yaml", i);
    YamlDoc *doc = LoadAssetYaml(path);
    if (!doc) continue;

    YamlNode *root = Yaml_GetRoot(doc);
    if (!root) { Yaml_Free(doc); continue; }

    YamlNode *holes_list = Yaml_GetMapping(root, "Holes");
    if (holes_list) {
      int n = Yaml_GetSequenceLength(holes_list);
      for (int hi = 0; hi < n; hi++) {
        YamlNode *h = Yaml_GetSequence(holes_list, hi);
        if (!h) continue;

        int x = Yaml_GetInt(h, "x", 0);
        int y = Yaml_GetInt(h, "y", 0);
        uint8_t eid = Yaml_GetInt(h, "entrance_id", 0);

        holes[hole_count].entrance_id = eid;
        holes[hole_count].pos = (x << 1) | (((y - 8) & 0x3f) << 7);
        holes[hole_count].area = i;
        hole_count++;
      }
    }

    Yaml_Free(doc);
  }

  // Sort holes by (entrance_id, pos, area) - matching Python's tuple sort
  for (int i = 0; i < hole_count - 1; i++) {
    for (int j = i + 1; j < hole_count; j++) {
      bool should_swap = false;
      if (holes[j].entrance_id < holes[i].entrance_id) {
        should_swap = true;
      } else if (holes[j].entrance_id == holes[i].entrance_id) {
        if (holes[j].pos < holes[i].pos) {
          should_swap = true;
        } else if (holes[j].pos == holes[i].pos) {
          if (holes[j].area < holes[i].area) {
            should_swap = true;
          }
        }
      }
      if (should_swap) {
        Overworld_Hole temp = holes[i];
        holes[i] = holes[j];
        holes[j] = temp;
      }
    }
  }

  uint16_t *hole_area = malloc(19 * 2);
  uint16_t *hole_pos = malloc(19 * 2);
  uint8_t *hole_ent = malloc(19);

  for (int i = 0; i < 19 && i < hole_count; i++) {
    hole_area[i] = holes[i].area;
    hole_pos[i] = holes[i].pos;
    hole_ent[i] = holes[i].entrance_id;
  }

  AssetBuilder_AddAsset(builder, "kFallHole_Area", ASSET_TYPE_UINT16, (uint8_t*)hole_area, 19*2);
  AssetBuilder_AddAsset(builder, "kFallHole_Pos", ASSET_TYPE_UINT16, (uint8_t*)hole_pos, 19*2);
  AssetBuilder_AddAsset(builder, "kFallHole_Entrances", ASSET_TYPE_UINT8, hole_ent, 19);

  free(holes); free(hole_area); free(hole_pos); free(hole_ent);

  printf("    Phase 4: Added 3 hole assets (%d holes, sorted)\n", hole_count);

  // ======================================================================
  // Phase 5: Exits (22 assets) - Regular exits + Special exits
  // ======================================================================

  uint8_t *exit_screen = calloc(79, 1);
  uint16_t *exit_rooms = calloc(79, 2);
  uint16_t *exit_load = calloc(79, 2);
  uint16_t *exit_sx = calloc(79, 2), *exit_sy = calloc(79, 2);
  uint16_t *exit_px = calloc(79, 2), *exit_py = calloc(79, 2);
  uint16_t *exit_cx = calloc(79, 2), *exit_cy = calloc(79, 2);
  uint16_t *exit_normal_door = calloc(79, 2);
  uint16_t *exit_fancy_door = calloc(79, 2);
  int8_t *exit_unk1 = calloc(79, 1), *exit_unk3 = calloc(79, 1);

  // Special exits (16 entries)
  uint16_t *sp_top = calloc(16, 2), *sp_bottom = calloc(16, 2);
  uint16_t *sp_left = calloc(16, 2), *sp_right = calloc(16, 2);
  int16_t *sp_tab4 = calloc(16, 2), *sp_tab5 = calloc(16, 2);
  int16_t *sp_tab6 = calloc(16, 2), *sp_tab7 = calloc(16, 2);
  uint16_t *sp_left_edge = calloc(16, 2);
  uint8_t *sp_dir = calloc(16, 1);
  uint8_t *sp_spr_gfx = calloc(16, 1), *sp_aux_gfx = calloc(16, 1);
  uint8_t *sp_pal_bg = calloc(16, 1), *sp_pal_spr = calloc(16, 1);

  printf("    Processing exits from areas...\n");
  for (int i = 0; i < 160; i++) {
    if (!Overworld_IsAreaHead(rom, i)) continue;

    char path[256];
    snprintf(path, sizeof(path), "assets/overworld/overworld-%d.yaml", i);
    YamlDoc *doc = LoadAssetYaml(path);
    if (!doc) continue;

    YamlNode *root = Yaml_GetRoot(doc);
    if (!root) { Yaml_Free(doc); continue; }

    YamlNode *exits = Yaml_GetMapping(root, "Exits");
    if (exits) {
      int n = Yaml_GetSequenceLength(exits);
      for (int ei = 0; ei < n; ei++) {
        YamlNode *e = Yaml_GetSequence(exits, ei);
        if (!e) continue;

        int j = Yaml_GetInt(e, "index", -1);
        if (j < 0 || j >= 79) continue;

        int base_x = (i & 7) << 9;
        int base_y = (i & 56) << 6;

        exit_screen[j] = i;
        exit_rooms[j] = Yaml_GetInt(e, "room", 0);

        // get_loadoffs calculation
        YamlNode *scroll = Yaml_GetMapping(e, "scroll_xy");
        YamlNode *load = Yaml_GetMapping(e, "load_xy");
        int sx = scroll ? Yaml_AsInt(Yaml_GetSequence(scroll, 0)) : 0;
        int sy = scroll ? Yaml_AsInt(Yaml_GetSequence(scroll, 1)) : 0;
        int lx = load ? Yaml_AsInt(Yaml_GetSequence(load, 0)) : 0;
        int ly = load ? Yaml_AsInt(Yaml_GetSequence(load, 1)) : 0;
        int x = (sx >> 4) + lx;
        int y = (sy >> 4) + ly;
        exit_load[j] = ((y & 0x3f) << 7) | ((x & 0x3f) << 1);

        exit_sx[j] = sx + base_x;
        exit_sy[j] = sy + base_y;

        YamlNode *xy = Yaml_GetMapping(e, "xy");
        int px = xy ? Yaml_AsInt(Yaml_GetSequence(xy, 0)) : 0;
        int py = xy ? Yaml_AsInt(Yaml_GetSequence(xy, 1)) : 0;
        exit_px[j] = px + base_x;
        exit_py[j] = py + base_y;

        YamlNode *camera = Yaml_GetMapping(e, "camera_xy");
        int cx = camera ? Yaml_AsInt(Yaml_GetSequence(camera, 0)) : 0;
        int cy = camera ? Yaml_AsInt(Yaml_GetSequence(camera, 1)) : 0;
        exit_cx[j] = cx + base_x;
        exit_cy[j] = cy + base_y;

        YamlNode *unk = Yaml_GetMapping(e, "unk");
        exit_unk1[j] = unk ? Yaml_AsInt(Yaml_GetSequence(unk, 0)) : 0;
        exit_unk3[j] = unk ? Yaml_AsInt(Yaml_GetSequence(unk, 1)) : 0;

        // Door processing
        YamlNode *door = Yaml_GetMapping(e, "door");
        if (door && Yaml_GetSequenceLength(door) >= 3) {
          YamlNode *dtype = Yaml_GetSequence(door, 0);
          YamlNode *dpos = Yaml_GetSequence(door, 1);
          YamlNode *ddir = Yaml_GetSequence(door, 2);

          const char *type = dtype ? Yaml_AsString(dtype) : "none";
          int pos = dpos ? Yaml_AsInt(dpos) : 0;
          int dir = ddir ? Yaml_AsInt(ddir) : 0;

          if (strcmp(type, "bombable") == 0 || strcmp(type, "wooden") == 0) {
            exit_normal_door[j] = (pos << 1) | (dir << 7) | (strcmp(type, "bombable") == 0 ? 0x8000 : 0);
          } else if (strcmp(type, "palace") == 0 || strcmp(type, "sanctuary") == 0) {
            exit_fancy_door[j] = (pos << 1) | (dir << 7) | (strcmp(type, "palace") == 0 ? 0x8000 : 0);
          }
        }

        // Special exit processing
        YamlNode *se = Yaml_GetMapping(e, "special_exit");
        if (se) {
          int room = exit_rooms[j];
          if (room >= 0x180 && room < 0x190) {
            int sp_idx = room - 0x180;
            sp_dir[sp_idx] = Yaml_GetInt(se, "dir", 0) * 2;
            sp_spr_gfx[sp_idx] = Yaml_GetInt(se, "spr_gfx", 0);
            sp_aux_gfx[sp_idx] = Yaml_GetInt(se, "aux_gfx", 0);
            sp_pal_bg[sp_idx] = Yaml_GetInt(se, "pal_bg", 0);
            sp_pal_spr[sp_idx] = Yaml_GetInt(se, "pal_spr", 0);
            sp_top[sp_idx] = Yaml_GetInt(se, "top", 0);
            sp_bottom[sp_idx] = Yaml_GetInt(se, "bottom", 0);
            sp_left[sp_idx] = Yaml_GetInt(se, "left", 0);
            sp_right[sp_idx] = Yaml_GetInt(se, "right", 0);
            sp_left_edge[sp_idx] = Yaml_GetInt(se, "left_edge_of_map", 0);
            sp_tab4[sp_idx] = Yaml_GetInt(se, "unk4", 0);
            sp_tab5[sp_idx] = Yaml_GetInt(se, "unk5", 0);
            sp_tab6[sp_idx] = Yaml_GetInt(se, "unk6", 0);
            sp_tab7[sp_idx] = Yaml_GetInt(se, "unk7", 0);
          }
        }
      }
    }

    Yaml_Free(doc);
  }

  AssetBuilder_AddAsset(builder, "kExitData_ScreenIndex", ASSET_TYPE_UINT8, exit_screen, 79);
  AssetBuilder_AddAsset(builder, "kExitDataRooms", ASSET_TYPE_UINT16, (uint8_t*)exit_rooms, 79*2);
  AssetBuilder_AddAsset(builder, "kExitData_Map16LoadSrcOff", ASSET_TYPE_UINT16, (uint8_t*)exit_load, 79*2);
  AssetBuilder_AddAsset(builder, "kExitData_ScrollX", ASSET_TYPE_UINT16, (uint8_t*)exit_sx, 79*2);
  AssetBuilder_AddAsset(builder, "kExitData_ScrollY", ASSET_TYPE_UINT16, (uint8_t*)exit_sy, 79*2);
  AssetBuilder_AddAsset(builder, "kExitData_XCoord", ASSET_TYPE_UINT16, (uint8_t*)exit_px, 79*2);
  AssetBuilder_AddAsset(builder, "kExitData_YCoord", ASSET_TYPE_UINT16, (uint8_t*)exit_py, 79*2);
  AssetBuilder_AddAsset(builder, "kExitData_CameraXScroll", ASSET_TYPE_UINT16, (uint8_t*)exit_cx, 79*2);
  AssetBuilder_AddAsset(builder, "kExitData_CameraYScroll", ASSET_TYPE_UINT16, (uint8_t*)exit_cy, 79*2);
  AssetBuilder_AddAsset(builder, "kExitData_NormalDoor", ASSET_TYPE_UINT16, (uint8_t*)exit_normal_door, 79*2);
  AssetBuilder_AddAsset(builder, "kExitData_FancyDoor", ASSET_TYPE_UINT16, (uint8_t*)exit_fancy_door, 79*2);
  AssetBuilder_AddAsset(builder, "kExitData_Unk1", ASSET_TYPE_INT8, (uint8_t*)exit_unk1, 79);
  AssetBuilder_AddAsset(builder, "kExitData_Unk3", ASSET_TYPE_INT8, (uint8_t*)exit_unk3, 79);

  AssetBuilder_AddAsset(builder, "kSpExit_Top", ASSET_TYPE_UINT16, (uint8_t*)sp_top, 16*2);
  AssetBuilder_AddAsset(builder, "kSpExit_Bottom", ASSET_TYPE_UINT16, (uint8_t*)sp_bottom, 16*2);
  AssetBuilder_AddAsset(builder, "kSpExit_Left", ASSET_TYPE_UINT16, (uint8_t*)sp_left, 16*2);
  AssetBuilder_AddAsset(builder, "kSpExit_Right", ASSET_TYPE_UINT16, (uint8_t*)sp_right, 16*2);
  AssetBuilder_AddAsset(builder, "kSpExit_Tab4", ASSET_TYPE_INT16, (uint8_t*)sp_tab4, 16*2);
  AssetBuilder_AddAsset(builder, "kSpExit_Tab5", ASSET_TYPE_INT16, (uint8_t*)sp_tab5, 16*2);
  AssetBuilder_AddAsset(builder, "kSpExit_Tab6", ASSET_TYPE_INT16, (uint8_t*)sp_tab6, 16*2);
  AssetBuilder_AddAsset(builder, "kSpExit_Tab7", ASSET_TYPE_INT16, (uint8_t*)sp_tab7, 16*2);
  AssetBuilder_AddAsset(builder, "kSpExit_LeftEdgeOfMap", ASSET_TYPE_UINT16, (uint8_t*)sp_left_edge, 16*2);
  AssetBuilder_AddAsset(builder, "kSpExit_Dir", ASSET_TYPE_UINT8, sp_dir, 16);
  AssetBuilder_AddAsset(builder, "kSpExit_SprGfx", ASSET_TYPE_UINT8, sp_spr_gfx, 16);
  AssetBuilder_AddAsset(builder, "kSpExit_AuxGfx", ASSET_TYPE_UINT8, sp_aux_gfx, 16);
  AssetBuilder_AddAsset(builder, "kSpExit_PalBg", ASSET_TYPE_UINT8, sp_pal_bg, 16);
  AssetBuilder_AddAsset(builder, "kSpExit_PalSpr", ASSET_TYPE_UINT8, sp_pal_spr, 16);

  free(exit_screen); free(exit_rooms); free(exit_load); free(exit_sx); free(exit_sy);
  free(exit_px); free(exit_py); free(exit_cx); free(exit_cy);
  free(exit_normal_door); free(exit_fancy_door); free(exit_unk1); free(exit_unk3);
  free(sp_top); free(sp_bottom); free(sp_left); free(sp_right);
  free(sp_tab4); free(sp_tab5); free(sp_tab6); free(sp_tab7); free(sp_left_edge);
  free(sp_dir); free(sp_spr_gfx); free(sp_aux_gfx); free(sp_pal_bg); free(sp_pal_spr);

  printf("    Phase 5: Added 22 exit assets (79 regular + 16 special)\n");

  // ======================================================================
  // Phase 6: Secrets (2 assets) - Item locations with terminators
  // ======================================================================

  uint16_t *secret_offs = malloc(128 * 2);
  uint8_t *secret_data = malloc(10000);
  int secret_len = 0;

  // Initialize all offsets to 0xFFFF (unset marker)
  for (int i = 0; i < 128; i++) {
    secret_offs[i] = 0xFFFF;
  }

  printf("    Processing secrets from areas...\n");
  for (int i = 0; i < 128; i++) {  // Only light world (0-127)
    if (!Overworld_IsAreaHead(rom, i)) {
      // Leave as 0xFFFF (will be set to default_offset later)
      continue;
    }

    char path[256];
    snprintf(path, sizeof(path), "assets/overworld/overworld-%d.yaml", i);
    YamlDoc *doc = LoadAssetYaml(path);
    if (!doc) {
      // Leave as 0 (will be set to default_offset later)
      continue;
    }

    YamlNode *root = Yaml_GetRoot(doc);
    if (!root) {
      Yaml_Free(doc);
      // Leave as 0 (will be set to default_offset later)
      continue;
    }

    YamlNode *items = Yaml_GetMapping(root, "Items");
    int has_items = items && Yaml_GetSequenceLength(items) > 0;

    if (has_items) {
      secret_offs[i] = secret_len;
      int n = Yaml_GetSequenceLength(items);

      for (int ii = 0; ii < n; ii++) {
        YamlNode *item = Yaml_GetSequence(items, ii);
        if (!item || Yaml_GetSequenceLength(item) < 3) continue;

        int x = Yaml_AsInt(Yaml_GetSequence(item, 0));
        int y = Yaml_AsInt(Yaml_GetSequence(item, 1));
        const char *item_name = Yaml_AsString(Yaml_GetSequence(item, 2));

        uint16_t pos = (x << 1) | (y << 7);
        uint8_t item_id = FindSecretIndex(item_name);

        secret_data[secret_len++] = pos & 0xff;
        secret_data[secret_len++] = pos >> 8;
        secret_data[secret_len++] = item_id;
      }

      // Terminator
      secret_data[secret_len++] = 0xff;
      secret_data[secret_len++] = 0xff;

      // Mirror for big maps
      if (map_is_small[i] == 0) {
        secret_offs[i + 1] = secret_offs[i];
        secret_offs[i + 8] = secret_offs[i];
        secret_offs[i + 9] = secret_offs[i];
      }
    }
    // else: leave as 0 (will be set to default_offset later)

    Yaml_Free(doc);
  }

  // Fill in default offsets for areas without items
  uint16_t default_offset = secret_len - 2;  // Point to last 0xFFFF
  for (int i = 0; i < 128; i++) {
    if (secret_offs[i] == 0xFFFF) {  // Was not set (no items or mirrored)
      secret_offs[i] = default_offset;
    }
  }

  AssetBuilder_AddAsset(builder, "kOverworldSecrets_Offs", ASSET_TYPE_UINT16, (uint8_t*)secret_offs, 128*2);
  AssetBuilder_AddAsset(builder, "kOverworldSecrets", ASSET_TYPE_UINT8, secret_data, secret_len);

  free(secret_offs);
  free(secret_data);

  printf("    Phase 6: Added 2 secret assets (%d bytes of data)\n", secret_len);

  // ======================================================================
  // Phase 7: Sprites (4 assets) - Sprite lists for game stages
  // ======================================================================

  uint16_t *sprite_offs = calloc(144 * 3, 2);  // 3 stages × 144 areas
  uint8_t *sprite_gfx = calloc(256, 1);
  uint8_t *sprite_pal = calloc(256, 1);
  uint8_t *sprite_data = malloc(50000);
  int sprite_len = 0;

  sprite_data[sprite_len++] = 0xff;  // Initial terminator

  printf("    Processing sprites from areas...\n");

  // Process 4 sprite stages
  int stage0[] = {0};
  int stage1[] = {1};
  int stage2[] = {2};
  int stage12[] = {1, 2};

  Overworld_ProcessSpriteStage(rom, map_is_small, sprite_offs, sprite_gfx, sprite_pal,
                                sprite_data, &sprite_len, 0, 64, "Sprites.Beginning", stage0, 1, 0);
  Overworld_ProcessSpriteStage(rom, map_is_small, sprite_offs, sprite_gfx, sprite_pal,
                                sprite_data, &sprite_len, 0, 64, "Sprites.FirstPart", stage1, 1, 1);
  Overworld_ProcessSpriteStage(rom, map_is_small, sprite_offs, sprite_gfx, sprite_pal,
                                sprite_data, &sprite_len, 0, 64, "Sprites.SecondPart", stage2, 1, 2);
  Overworld_ProcessSpriteStage(rom, map_is_small, sprite_offs, sprite_gfx, sprite_pal,
                                sprite_data, &sprite_len, 64, 144, "Sprites", stage12, 2, 3);

  AssetBuilder_AddAsset(builder, "kOverworldSpriteOffs", ASSET_TYPE_UINT16, (uint8_t*)sprite_offs, 144*3*2);
  AssetBuilder_AddAsset(builder, "kOverworldSprites", ASSET_TYPE_UINT8, sprite_data, sprite_len);
  AssetBuilder_AddAsset(builder, "kOverworldSpriteGfx", ASSET_TYPE_UINT8, sprite_gfx, 256);
  AssetBuilder_AddAsset(builder, "kOverworldSpritePalettes", ASSET_TYPE_UINT8, sprite_pal, 256);

  free(sprite_offs);
  free(sprite_data);
  free(sprite_gfx);
  free(sprite_pal);

  printf("    Phase 7: Added 4 sprite assets (%d bytes of sprite data)\n", sprite_len);

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

static void TestYAMLLoading(void) {
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

static void PrintHelp(void) {
  printf("zelda3_restool - Zelda3 Asset Extraction Tool v%s\n\n", RESTOOL_VERSION);
  printf("USAGE:\n");
  printf("  zelda3_restool [OPTIONS]\n\n");
  printf("OPTIONS:\n");
  printf("  --extract-from-rom <path>   Extract assets from ROM and compile (default)\n");
  printf("  --no-compile                Skip asset compilation (extract only)\n");
  printf("  --compile                   Compile assets to zelda3_assets.dat\n");
  printf("  --languages <L1,L2,...>     Include additional languages (comma-separated)\n");
  printf("                              Requires dialogue_<lang>.txt files to exist\n");
  printf("  --extract-dialogue          Extract dialogue strings (separate mode)\n");
  printf("  --language <code>           Language for dialogue extraction\n");
  printf("  --extract-graphics          Extract Link sprites (linksprite.png)\n");
  printf("  --extract-enemy-sheet <N>   Extract enemy tileset N (enemy_N.png)\n");
  printf("  --extract-overworld         Extract overworld data (160 areas)\n");
  printf("  --output <dir>              Output directory (default: current)\n");
  printf("  --verbose, -v               Verbose output\n");
  printf("  --test-yaml                 Test YAML parsing (dev only)\n");
  printf("  --test-map32                Test Map32toMap16 extraction vs Python\n");
  printf("  --test-link                 Test Link graphics extraction vs Python\n");
  printf("  --test-dungeon              Test dungeon sprites extraction vs Python\n");
  printf("  --help, -h                  Show this help\n");
  printf("  --version                   Show version\n\n");
  printf("SUPPORTED LANGUAGES:\n");
  printf("  us (default), de, fr, fr-c, en, es, pl, pt, nl, sv, redux, retrans-kal\n\n");
  printf("EXAMPLES:\n");
  printf("  # Extract and compile assets (auto-compiles by default)\n");
  printf("  zelda3_restool --extract-from-rom zelda3.sfc\n\n");
  printf("  # Extract and compile with multiple languages\n");
  printf("  zelda3_restool --extract-from-rom zelda3.sfc --languages de,fr\n\n");
  printf("  # Extract only, skip compilation\n");
  printf("  zelda3_restool --extract-from-rom zelda3.sfc --no-compile\n\n");
  printf("  # Extract Link sprites from USA ROM\n");
  printf("  zelda3_restool --extract-from-rom zelda3.sfc --extract-graphics\n\n");
  printf("  # Extract dialogue for German ROM\n");
  printf("  zelda3_restool --extract-from-rom zelda3_de.sfc --extract-dialogue\n\n");
  printf("  # Compile assets from existing extracted files\n");
  printf("  zelda3_restool --compile\n\n");
}

static void PrintVersion(void) {
  printf("zelda3_restool version %s\n", RESTOOL_VERSION);
  printf("Built: %s %s\n", __DATE__, __TIME__);
}

static bool ParseArgs(int argc, char **argv, RestoolArgs *args) {
  memset(args, 0, sizeof(RestoolArgs));
  args->extract_enemy_sheet = -1;  // Initialize to "none"

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--extract-from-rom") == 0) {
      if (i + 1 >= argc) {
        LogError("--extract-from-rom requires a path argument");
        return false;
      }
      args->extract_mode = true;
      args->rom_path = argv[++i];
    } else if (strcmp(argv[i], "--extract-graphics") == 0) {
      args->extract_graphics = true;
    } else if (strcmp(argv[i], "--extract-enemy-sheet") == 0) {
      if (i + 1 >= argc) {
        LogError("--extract-enemy-sheet requires a sheet number");
        return false;
      }
      args->extract_enemy_sheet = atoi(argv[++i]);
      if (args->extract_enemy_sheet < 0 || args->extract_enemy_sheet > 255) {
        LogError("Invalid enemy sheet number (must be 0-255)");
        return false;
      }
    } else if (strcmp(argv[i], "--extract-overworld") == 0) {
      args->extract_overworld = true;
    } else if (strcmp(argv[i], "--compile") == 0) {
      args->compile_mode = true;
    } else if (strcmp(argv[i], "--extract-dialogue") == 0) {
      args->extract_dialogue = true;
    } else if (strcmp(argv[i], "--language") == 0) {
      if (i + 1 >= argc) {
        LogError("--language requires a language code");
        return false;
      }
      args->language = argv[++i];
    } else if (strcmp(argv[i], "--output") == 0) {
      if (i + 1 >= argc) {
        LogError("--output requires a directory path");
        return false;
      }
      args->output_dir = argv[++i];
    } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
      args->verbose = true;
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      args->help = true;
    } else if (strcmp(argv[i], "--version") == 0) {
      args->version = true;
    } else if (strcmp(argv[i], "--test-yaml") == 0) {
      args->test_yaml = true;
    } else if (strcmp(argv[i], "--test-map32") == 0) {
      args->test_map32 = true;
    } else if (strcmp(argv[i], "--test-link") == 0) {
      args->test_link = true;
    } else if (strcmp(argv[i], "--test-dungeon") == 0) {
      args->test_dungeon = true;
    } else if (strcmp(argv[i], "--no-compile") == 0) {
      args->no_compile = true;
    } else if (strcmp(argv[i], "--languages") == 0) {
      if (i + 1 >= argc) {
        LogError("--languages requires a comma-separated list of language codes");
        return false;
      }
      args->languages = argv[++i];
    } else {
      LogError("Unknown option: %s", argv[i]);
      return false;
    }
  }

  // Auto-enable compilation when extracting from ROM (unless --no-compile)
  if (args->extract_mode && !args->no_compile && !args->extract_dialogue &&
      !args->extract_graphics && !args->extract_overworld && args->extract_enemy_sheet < 0) {
    args->compile_mode = true;
  }

  // Validation
  if (!args->help && !args->version && !args->test_yaml && !args->test_map32 && !args->test_link && !args->test_dungeon) {
    if (!args->extract_mode && !args->compile_mode) {
      LogError("Must specify --extract-from-rom or --compile");
      return false;
    }
    if (args->extract_mode && !args->rom_path) {
      LogError("--extract-from-rom requires a ROM path");
      return false;
    }
  }

  return true;
}

// Extract sound banks using pure C music compiler
// Returns true on success, false on error
bool ExtractSoundBanks(AssetBuilder *builder) {
  printf("  Extracting sound banks (intro, indoor, ending)...\n");

  const char *songs[] = {"intro", "indoor", "ending"};

  for (int i = 0; i < 3; i++) {
    const char *song = songs[i];

    uint8_t *data = NULL;
    size_t size = 0;

    // Use pure C music compiler (no Python dependency)
    if (!MusicCompiler_CompileSoundBank(song, "assets", &data, &size)) {
      LogError("Failed to compile sound bank %s", song);
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

// Extract dialogue assets (pure C implementation - no Python dependency)
// languages_arg: comma-separated list like "de,fr" (US is always included first), or NULL for US only
// Returns true on success, false on error
bool ExtractDialogue(AssetBuilder *builder, const char *languages_arg) {
  // Call pure C implementation (replaces Python script)
  ExtractDialogueAssets(builder, languages_arg);
  return true;
}

int main(int argc, char **argv) {
  RestoolArgs args;

  // Initialize logging
  InitializeLogging();

  if (!ParseArgs(argc, argv, &args)) {
    fprintf(stderr, "Use --help for usage information\n");
    return 1;
  }

  if (args.help) {
    PrintHelp();
    return 0;
  }

  if (args.version) {
    PrintVersion();
    return 0;
  }

  if (args.test_yaml) {
    TestYAMLLoading();
    return 0;
  }

  if (args.test_map32) {
    TestMap32ToMap16();
    return 0;
  }

  if (args.test_link) {
    TestLinkGraphics();
    return 0;
  }

  if (args.test_dungeon) {
    TestDungeonSprites();
    return 0;
  }

  // Test third-party dependencies
  if (args.verbose) {
    printf("Testing SHA-256...\n");
    const char *test_data = "Hello, World!";
    uint8_t hash[32];
    sha256((const uint8_t *)test_data, strlen(test_data), hash);
    printf("SHA-256 test: ");
    for (int i = 0; i < 32; i++) {
      printf("%02x", hash[i]);
    }
    printf("\n");
  }

  if (args.extract_mode) {
    printf("Loading ROM: %s\n", args.rom_path);

    Rom *rom = Rom_Load(args.rom_path);
    if (!rom) {
      LogError("Failed to load ROM");
      return 1;
    }

    printf("ROM loaded successfully:\n");
    printf("  Size: %zu bytes (%.2f MB)\n", rom->size, rom->size / (1024.0 * 1024.0));
    printf("  SMC header: %s\n", rom->has_smc_header ? "yes" : "no");
    printf("  SHA-1: %s\n", rom->sha1);

    // Show ROM identification (using language detection from Rom_Load)
    if (rom->language != ROM_LANG_UNKNOWN) {
      printf("  Identified ROM as: %s - \"%s\"\n",
             Rom_GetLanguageCode(rom->language), rom->language_name);
    } else {
      printf("  Version: Unknown (unsupported ROM)\n");
    }

    // Test reading some values
    if (args.verbose) {
      printf("\nTesting ROM access:\n");
      printf("  Byte at $00:8000: 0x%02X\n", Rom_ReadByte(rom, 0x008000));
      printf("  Word at $00:8000: 0x%04X\n", Rom_ReadWord(rom, 0x008000));
      printf("  Addr at $00:8000: 0x%06X\n", Rom_ReadAddr(rom, 0x008000));
    }

    // Extract Link sprites (4bpp)
    if (args.extract_graphics) {
      if (args.verbose) printf("\nExtracting Link sprites...\n");

      // Link sprite palette (from Python tool)
      uint16_t link_palette[] = {
        0x0000, 0x7fff, 0x237e, 0x11b7, 0x369e, 0x14a5, 0x01ff, 0x1078,
        0x599d, 0x3647, 0x3b68, 0x0a4a, 0x12ef, 0x2a5c, 0x1571, 0x7a18
      };
      Color rgba_palette[16];
      SnesPaletteToRGBA(link_palette, 16, rgba_palette);

      // Extract Link sprites (448 pixels tall = 56 tiles high, 16 tiles wide = 128px)
      // 56 tiles * 16 tiles = 896 tiles total
      // At 32 bytes per 4bpp tile = 28672 bytes
      uint8_t *link_gfx = Rom_ReadPtr(rom, 0x108000, 896 * 32);
      if (link_gfx) {
        TileData *tiles = DecodeTileset4bpp(link_gfx, 896, 16);
        if (tiles) {
          if (WritePNG_Indexed("linksprite.png", tiles->width, tiles->height,
                               tiles->pixels, rgba_palette, 16)) {
            printf("Extracted: linksprite.png (%dx%d)\n", tiles->width, tiles->height);
          }
          FreeTileData(tiles);
        }
      } else {
        LogError("Failed to read Link sprite data");
      }
    }

    // Extract enemy sprite tileset (3bpp)
    if (args.extract_enemy_sheet >= 0) {
      if (args.verbose) printf("\nExtracting enemy tileset %d...\n", args.extract_enemy_sheet);

      // Enemy sprite addresses (from Python tool's kCompSpritePtrs)
      static const uint32_t kCompSpritePtrs[] = {
        0x10f000, 0x10f600, 0x10fc00, 0x118200, 0x118800, 0x118e00, 0x119400, 0x119a00,
        0x11a000, 0x11a600, 0x11ac00, 0x11b200
      };

      // Only support uncompressed tilesets (0-11) for now
      if (args.extract_enemy_sheet < 12) {
        uint32_t snes_addr = kCompSpritePtrs[args.extract_enemy_sheet];

        // Simple grayscale palette for enemy sprites (matches Python tool: i * 36)
        Color rgba_palette[8];
        for (int i = 0; i < 8; i++) {
          uint8_t gray = i * 36;  // 0, 36, 72, 108, 144, 180, 216, 252
          rgba_palette[i].r = gray;
          rgba_palette[i].g = gray;
          rgba_palette[i].b = gray;
          rgba_palette[i].a = 255;
        }

        // Enemy tilesets are 128x32 (16x4 tiles = 64 tiles, 24 bytes per 3bpp tile)
        uint8_t *enemy_gfx = Rom_ReadPtr(rom, snes_addr, 64 * 24);
        if (enemy_gfx) {
          TileData *tiles = DecodeTileset3bpp(enemy_gfx, 64, 16);
          if (tiles) {
            char filename[64];
            snprintf(filename, sizeof(filename), "enemy_%d.png", args.extract_enemy_sheet);
            if (WritePNG_Indexed(filename, tiles->width, tiles->height,
                                 tiles->pixels, rgba_palette, 8)) {
              printf("Extracted: %s (%dx%d)\n", filename, tiles->width, tiles->height);
            }
            FreeTileData(tiles);
          }
        } else {
          LogError("Failed to read enemy sprite data");
        }
      } else {
        LogError("Compressed enemy tilesets not yet supported (use 0-11)");
      }
    }

    // Extract overworld data (160 areas)
    if (args.extract_overworld) {
      if (args.verbose) printf("\nExtracting overworld data...\n");

      OverworldArea **areas = Overworld_ExtractAll(rom);
      if (areas) {
        // Print summary of first few areas as a test
        printf("\nOverworld extraction results:\n");
        for (int i = 0; i < 10 && i < OVERWORLD_AREA_COUNT; i++) {
          if (areas[i]) {
            printf("  Area %3d: size=%s gfx=%02X pal=%02X music=%02X sprites=%d items=%d\n",
                   areas[i]->area_id,
                   areas[i]->size == AREA_SIZE_SMALL ? "16x16" : "32x32",
                   areas[i]->gfx_id,
                   areas[i]->palette_id,
                   areas[i]->music_track,
                   areas[i]->sprite_count,
                   areas[i]->item_count);
          }
        }
        printf("  ... (extracted %d total areas)\n", OVERWORLD_AREA_COUNT);

        Overworld_FreeAll(areas, OVERWORLD_AREA_COUNT);
      } else {
        LogError("Failed to extract overworld data");
      }
    }

    Rom_Free(rom);
    printf("\nExtraction complete\n");
  }

  // Handle --extract-dialogue (separate from extract_mode, matching Python behavior)
  if (args.extract_dialogue) {
    if (!args.rom_path) {
      LogError("--extract-dialogue requires ROM file (use --extract-from-rom)");
      return 1;
    }

    Rom *rom = Rom_Load(args.rom_path);
    if (!rom) {
      return 1;
    }

    // Get language code from ROM
    const char *lang_code = TextDecode_GetLanguageCode(rom->language);
    if (!lang_code) {
      LogError("Unknown ROM language");
      Rom_Free(rom);
      return 1;
    }

    printf("Identified ROM as: %s - \"%s\"\n", lang_code, rom->language_name);

    // Check if language is supported for text decoding
    const LanguageConfig *config = TextDecode_GetLanguageConfig(lang_code);
    if (!config) {
      LogError("Language '%s' not yet supported for dialogue extraction", lang_code);
      LogError("Supported languages: us, en, de, fr, fr-c");
      Rom_Free(rom);
      return 1;
    }

    // Decode dialogue strings
    printf("Extracting dialogue strings...\n");
    DecodedStringsArray *strings = TextDecode_DecodeStrings(rom, lang_code);
    if (!strings) {
      LogError("Failed to decode dialogue strings");
      Rom_Free(rom);
      return 1;
    }

    printf("Decoded %zu dialogue strings\n", strings->count);

    // Write to file
    if (!TextDecode_WriteDialogueFile(strings, lang_code, args.output_dir)) {
      LogError("Failed to write dialogue file");
      TextDecode_FreeStrings(strings);
      Rom_Free(rom);
      return 1;
    }

    TextDecode_FreeStrings(strings);
    Rom_Free(rom);
    return 0;  // Exit after dialogue extraction (matching Python behavior)
  }

  if (args.compile_mode) {
    printf("Compiling assets to zelda3_assets.dat...\n");

    // Need ROM to extract data for compilation
    if (!args.rom_path) {
      LogError("Compilation requires ROM file (use --extract-from-rom)");
      return 1;
    }

    Rom *rom = Rom_Load(args.rom_path);
    if (!rom) {
      return 1;
    }

    // Check for US ROM (matching Python's behavior in util.py)
    // Python only accepts US ROM for --extract-from-rom, other ROMs only work with --extract-dialogue
    if (rom->language != ROM_LANG_US) {
      fprintf(stderr, "\nROM with hash %s not supported.\n\n", rom->sha1);
      fprintf(stderr, "Expected %s.\n", ROM_SHA1_USA);
      fprintf(stderr, "Please verify your ROM is \"Legend of Zelda, The - A Link to the Past (USA)\"\n");
      if (rom->language != ROM_LANG_UNKNOWN) {
        fprintf(stderr, "\nDetected ROM: %s - \"%s\"\n",
                Rom_GetLanguageCode(rom->language), rom->language_name);
        fprintf(stderr, "\nNote: Non-US ROMs can only be used with --extract-dialogue for text extraction.\n");
      }
      Rom_Free(rom);
      return 1;
    }
    printf("Identified ROM as: %s - \"%s\"\n", Rom_GetLanguageCode(rom->language), rom->language_name);

    // Create asset builder
    AssetBuilder *builder = AssetBuilder_Create();
    if (!builder) {
      LogError("Failed to create asset builder");
      Rom_Free(rom);
      return 1;
    }

    // ========================================================================
    // EXTRACTION ORDER MATCHES Python's print_all() - DO NOT REORDER!
    // ========================================================================

    // 1. print_sound_banks() - 3 assets (0-2)
    if (!ExtractSoundBanks(builder)) {
      LogError("Failed to extract sound banks");
      AssetBuilder_Free(builder);
      Rom_Free(rom);
      return 1;
    }

    // 2. print_dungeon_rooms() - Dungeon room data
    // Python order: room data → headers → simple data → entrances → starting points → default → overlay → secrets → misc ROM
    ExtractDungeonRoomData(builder);           // 3 assets: kDungeonRoom, kDungeonRoomOffs, kDungeonRoomDoorOffs
    ExtractDungeonRoomHeaders(builder);        // 2 assets: kDungeonRoomHeaders, kDungeonRoomHeadersOffs
    ExtractDungeonRoomSimple(builder);         // 3 assets: kDungeonRoomChests, kDungeonRoomTeleMsg, kDungeonPitsHurtPlayer
    ExtractEntrancesAndStartingPoints(builder); // 35 assets total (17 entrance + 18 starting point)
    ExtractDefaultOverlayRooms(builder);       // 4 assets: default + overlay rooms
    ExtractDungeonSecrets(builder);            // 1 asset: kDungeonSecrets

    // Misc dungeon ROM assets (5 assets)
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

    // 3. print_enemy_damage_data() - 1 asset
    printf("  Extracting kEnemyDamageData (decompressed)...\n");
    DecompressedData *enemy_dmg = Snes_Decompress(rom, 0x83e800, true);
    if (enemy_dmg) {
      size_t enemy_dmg_size = enemy_dmg->size;
      AssetBuilder_AddAsset(builder, "kEnemyDamageData", ASSET_TYPE_UINT8,
                            enemy_dmg->data, enemy_dmg->size);
      Snes_FreeDecompressed(enemy_dmg);
      printf("    Added kEnemyDamageData (%zu bytes)\n", enemy_dmg_size);
    }

    // 4. print_link_graphics() - 1 asset
    ExtractLinkGraphics(builder);

    // 5. print_dungeon_sprites() - 2 assets
    ExtractDungeonSprites(builder);

    // 6. print_map32_to_map16() - 4 assets
    ExtractMap32toMap16(builder);

    // 7. print_images() - Sprite and background graphics
    ExtractSpriteGraphics(rom, builder);
    ExtractBackgroundGraphics(rom, builder);

    // 8. print_misc() - Misc ROM assets (~28 assets)
    ExtractMiscAssets(rom, builder);

    // 9. print_dialogue() - 3 assets (kDialogue, kDialogueFont, kDialogueMap)
    // Pass languages arg to support multi-language builds
    if (!ExtractDialogue(builder, args.languages)) {
      LogError("Failed to extract dialogue");
      AssetBuilder_Free(builder);
      Rom_Free(rom);
      return 1;
    }

    // 10. print_dungeon_map() - 2 packed assets
    ExtractDungeonMap(rom, builder);

    // 11. print_tilemaps() - 6 assets
    ExtractTilemaps(rom, builder);

    // 12. print_overworld() - Compressed overworld data (2 packed assets)
    printf("  Extracting kOverworld compressed data (160 areas each)...\n");
    uint8_t **hibytes = malloc(160 * sizeof(uint8_t*));
    uint32_t *hi_sizes = malloc(160 * sizeof(uint32_t));
    if (!hibytes || !hi_sizes) {
      LogError("Failed to allocate overworld hibytes arrays");
      free(hibytes);
      free(hi_sizes);
      AssetBuilder_Free(builder);
      Rom_Free(rom);
      return 1;
    }
    memset(hibytes, 0, 160 * sizeof(uint8_t*));  // Initialize to NULL for safe cleanup
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
    uint32_t hi_packed_size;
    uint8_t *hi_packed = AssetBuilder_PackArrays(hibytes, hi_sizes, 160, &hi_packed_size);
    if (hi_packed) {
      AssetBuilder_AddAsset(builder, "kOverworld_Hibytes_Comp", ASSET_TYPE_PACKED, hi_packed, hi_packed_size);
      free(hi_packed);
    }
    for (int i = 0; i < 160; i++) free(hibytes[i]);
    free(hibytes);
    free(hi_sizes);

    uint8_t **lobytes = malloc(160 * sizeof(uint8_t*));
    uint32_t *lo_sizes = malloc(160 * sizeof(uint32_t));
    if (!lobytes || !lo_sizes) {
      LogError("Failed to allocate overworld lobytes arrays");
      free(lobytes);
      free(lo_sizes);
      AssetBuilder_Free(builder);
      Rom_Free(rom);
      return 1;
    }
    memset(lobytes, 0, 160 * sizeof(uint8_t*));  // Initialize to NULL for safe cleanup
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

    // 13. print_overworld_tables() - Overworld YAML data (48 assets from 160 files)
    ExtractOverworldYAML(builder, rom);

    // NOTE: ExtractRomBasedAssets removed - all ROM assets are now added inline in correct order

    printf("  Total: %u assets extracted in Python order\n", builder->asset_count);

    // Write to file (use output_dir if specified)
    char output_path[512];
    if (args.output_dir && args.output_dir[0]) {
      snprintf(output_path, sizeof(output_path), "%s/zelda3_assets.dat", args.output_dir);
    } else {
      snprintf(output_path, sizeof(output_path), "zelda3_assets.dat");
    }

    if (!AssetBuilder_WriteToFile(builder, output_path)) {
      LogError("Failed to write assets file");
      AssetBuilder_Free(builder);
      Rom_Free(rom);
      return 1;
    }

    printf("Successfully compiled assets to %s\n", output_path);
    AssetBuilder_Free(builder);
    Rom_Free(rom);
  }

  return 0;
}
