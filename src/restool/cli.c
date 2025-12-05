// cli.c - Zelda3 Restool Command Line Interface
// Separated from main.c to allow library-only builds

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

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

// Define STB_IMAGE_IMPLEMENTATION for standalone restool build
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define RESTOOL_VERSION "0.1.0"

// ============================================================================
// Font ROM Extraction
// ============================================================================

// Font configuration for each language
// Format: { lang_code, font_addr, font_size, width_addr, width_count }
// All ROMs store fonts as 2bpp SNES tiles (256 chars × 16 bytes = 4096 bytes)
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

static const FontRomConfig* GetFontRomConfig(const char *lang) {
  for (size_t i = 0; i < sizeof(kFontRomConfigs) / sizeof(kFontRomConfigs[0]); i++) {
    if (strcmp(kFontRomConfigs[i].lang, lang) == 0) {
      return &kFontRomConfigs[i];
    }
  }
  return NULL;
}

// Extract font data from ROM and save to binary files
// Creates: font_{lang}.bin (4096 bytes) and fontwidth_{lang}.bin (width_count bytes)
static bool ExtractFontFromRom(Rom *rom, const char *lang_code, const char *output_dir) {
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
    free(font_data);
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
    free(font_data);
    free(width_data);
    return false;
  }
  fwrite(font_data, 1, FONT_TILE_SIZE, f);
  fclose(f);
  printf("  Wrote %s (%d bytes)\n", font_filename, FONT_TILE_SIZE);

  // Write width data
  f = fopen(width_filename, "wb");
  if (!f) {
    LogError("Failed to create %s", width_filename);
    free(font_data);
    free(width_data);
    return false;
  }
  fwrite(width_data, 1, config->width_count, f);
  fclose(f);
  printf("  Wrote %s (%d bytes)\n", width_filename, config->width_count);

  free(font_data);
  free(width_data);
  return true;
}

// ============================================================================
// CLI Arguments
// ============================================================================

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
  bool sprites_from_png;    // --sprites-from-png: load sprites from PNG instead of ROM
  bool custom_sprites;      // --custom-sprites: use linksprite.png instead of ROM
  bool verbose;
  bool test_yaml;
  bool test_map32;
  bool test_link;
  bool test_dungeon;
  bool help;
  bool version;
} RestoolArgs;

// Forward declarations for test functions (defined in main.c, exported for CLI)
extern void TestMap32ToMap16(void);
extern void TestLinkGraphics(void);
extern void TestDungeonSprites(void);
extern void TestYAMLLoading(void);

static void PrintHelp(void) {
  printf("zelda3-restool - Zelda3 Asset Extraction Tool v%s\n\n", RESTOOL_VERSION);
  printf("USAGE:\n");
  printf("  zelda3-restool [OPTIONS]\n\n");
  printf("OPTIONS:\n");
  printf("  --extract-from-rom <path>   Extract assets from ROM and compile\n");
  printf("  --languages <L1,L2,...>     Include additional languages (comma-separated)\n");
  printf("  --compile                   Compile assets to zelda3_assets.dat\n");
  printf("  --no-compile                Skip compilation (extract only)\n");
  printf("  --sprites-from-png          Load sprite graphics from PNG instead of ROM\n");
  printf("  --custom-sprites            Use linksprite.png for Link graphics instead of ROM\n");
  printf("  --output <dir>              Output directory (default: current)\n");
  printf("  --verbose, -v               Verbose output\n");
  printf("  --help, -h                  Show this help\n");
  printf("  --version                   Show version\n\n");
  printf("EXTRACTION OPTIONS:\n");
  printf("  --extract-dialogue          Extract dialogue strings to text file\n");
  printf("  --extract-graphics          Extract Link sprites (linksprite.png)\n");
  printf("  --extract-enemy-sheet <N>   Extract enemy tileset N (enemy_N.png)\n");
  printf("  --extract-overworld         Extract overworld data (160 areas)\n");
  printf("  --language <code>           Language code for extraction\n\n");
  printf("DEV OPTIONS:\n");
  printf("  --test-yaml                 Test YAML parsing\n");
  printf("  --test-map32                Test Map32toMap16 extraction\n");
  printf("  --test-link                 Test Link graphics extraction\n");
  printf("  --test-dungeon              Test dungeon sprites extraction\n\n");
  printf("SUPPORTED LANGUAGES:\n");
  printf("  us (default), de, fr, fr-c, en, es, pl, pt, nl, sv, redux, retrans-kal\n\n");
  printf("EXAMPLES:\n");
  printf("  # Extract and compile assets (auto-compiles by default)\n");
  printf("  zelda3-restool --extract-from-rom zelda3.sfc\n\n");
  printf("  # Extract and compile with multiple languages\n");
  printf("  zelda3-restool --extract-from-rom zelda3.sfc --languages de,fr\n\n");
  printf("  # Extract only, skip compilation\n");
  printf("  zelda3-restool --extract-from-rom zelda3.sfc --no-compile\n\n");
  printf("  # Extract Link sprites from USA ROM\n");
  printf("  zelda3-restool --extract-from-rom zelda3.sfc --extract-graphics\n\n");
  printf("  # Extract dialogue for German ROM\n");
  printf("  zelda3-restool --extract-from-rom zelda3_de.sfc --extract-dialogue\n\n");
  printf("  # Compile assets from existing extracted files\n");
  printf("  zelda3-restool --compile\n\n");
  printf("  # Use modified sprite graphics from PNG files\n");
  printf("  zelda3-restool --extract-from-rom zelda3.sfc --sprites-from-png\n\n");
}

static void PrintVersion(void) {
  printf("zelda3-restool version %s\n", RESTOOL_VERSION);
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
    } else if (strcmp(argv[i], "--sprites-from-png") == 0) {
      args->sprites_from_png = true;
    } else if (strcmp(argv[i], "--custom-sprites") == 0) {
      args->custom_sprites = true;
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

        // Simple grayscale palette for enemy sprites
        Color rgba_palette[8];
        for (int i = 0; i < 8; i++) {
          uint8_t gray = i * 36;
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

  // Handle --extract-dialogue (separate from extract_mode)
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

    // Also extract font data from the ROM
    // This allows the font to be used when compiling with --languages
    if (!ExtractFontFromRom(rom, lang_code, args.output_dir)) {
      LogError("Failed to extract font (dialogue extraction succeeded)");
      // Continue anyway - dialogue was extracted successfully
    }

    Rom_Free(rom);
    return 0;  // Exit after dialogue extraction
  }

  if (args.compile_mode) {
    // Need ROM to extract data for compilation
    if (!args.rom_path) {
      LogError("Compilation requires ROM file (use --extract-from-rom)");
      return 1;
    }

    // Build output path
    char output_path[512];
    if (args.output_dir && args.output_dir[0]) {
      snprintf(output_path, sizeof(output_path), "%s/zelda3_assets.dat", args.output_dir);
    } else {
      snprintf(output_path, sizeof(output_path), "zelda3_assets.dat");
    }

    // Use the library function for compilation
    RestoolCompileOptions options = {
      .us_rom_path = args.rom_path,
      .output_path = output_path,
      .languages = args.languages,
      .dialogue_dir = NULL,
      .sprites_from_png = args.sprites_from_png,
      .custom_sprites = args.custom_sprites
    };

    int result = Restool_CompileAssetsEx(&options);
    if (result != RESTOOL_OK) {
      // Library function already logged the error
      return 1;
    }
  }

  return 0;
}
