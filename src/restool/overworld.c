// overworld.c - Overworld data extraction implementation

#include "overworld.h"
#include "restool_util.h"
#include "../logging.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ROM addresses for overworld data (from Python tool)
#define ADDR_AREA_SIZE_FLAGS  0x82F88D  // 192 bytes: small (1) or big (0) flags
#define ADDR_AREA_GFX         0x80FC9C  // 128 bytes: graphics tileset IDs
#define ADDR_AREA_PALETTE     0x80FD1C  // 136 bytes: palette IDs
#define ADDR_SIGN_TEXT        0x87F51D  // 128 words: sign text pointers
#define ADDR_MUSIC_BEGIN      0x82C303  // Music track IDs (4 stages: beginning, zelda, sword, agahnim)
#define ADDR_AMBIENT_BEGIN    0x82C383  // Ambient SFX IDs
#define ADDR_SPRITE_PTRS_BEGIN 0x89C881 // Sprite data pointers (beginning stage)
#define ADDR_ITEM_PTRS        0x9BC2F9  // Secret item pointers

OverworldArea* Overworld_ExtractArea(Rom *rom, uint8_t area_id) {
  if (!rom || area_id >= OVERWORLD_AREA_COUNT) {
    LogError("Invalid arguments to Overworld_ExtractArea");
    return NULL;
  }

  OverworldArea *area = (OverworldArea*)calloc(1, sizeof(OverworldArea));
  if (!area) {
    LogError("Failed to allocate OverworldArea");
    return NULL;
  }

  area->area_id = area_id;

  // Read area size flag (small=1, big=0)
  uint8_t *size_flags = Rom_ReadPtr(rom, ADDR_AREA_SIZE_FLAGS, 192);
  if (size_flags && area_id < 192) {
    area->size = size_flags[area_id] ? AREA_SIZE_SMALL : AREA_SIZE_BIG;
  } else {
    area->size = AREA_SIZE_BIG;  // Default
  }

  // Read graphics tileset ID (areas 0-127 only)
  if (area_id < 128) {
    area->gfx_id = Rom_ReadByte(rom, ADDR_AREA_GFX + area_id);
  } else {
    area->gfx_id = 0xFF;  // Invalid
  }

  // Read palette ID (areas 0-135 only)
  if (area_id < 136) {
    area->palette_id = Rom_ReadByte(rom, ADDR_AREA_PALETTE + area_id);
  } else {
    area->palette_id = 0xFF;  // Invalid
  }

  // Read sign text pointer (areas 0-127 only)
  if (area_id < 128) {
    area->sign_text_addr = Rom_ReadWord(rom, ADDR_SIGN_TEXT + area_id * 2);
  } else {
    area->sign_text_addr = 0xFFFF;  // No sign
  }

  // Read music track (simplified: just read beginning stage for now)
  // TODO: Handle multiple game stages (beginning, zelda, sword, agahnim)
  if (area_id < 64) {
    area->music_track = Rom_ReadByte(rom, ADDR_MUSIC_BEGIN + area_id);
    area->ambient_sfx = Rom_ReadByte(rom, ADDR_AMBIENT_BEGIN + area_id);
  } else if (area_id < 160) {
    // Dark world areas (64-159) use agahnim stage offset
    area->music_track = Rom_ReadByte(rom, ADDR_MUSIC_BEGIN + 192 + (area_id - 64));
    area->ambient_sfx = Rom_ReadByte(rom, ADDR_AMBIENT_BEGIN + 192 + (area_id - 64));
  }

  // Extract sprites (using Agahnim stage for populated data)
  // NOTE: Game has multiple stages (beginning, zelda, sword, agahnim) with different sprite placements
  // Using Agahnim (late game) stage as it has the most complete sprite data
  // TODO: Support extracting all game stages
  area->sprite_count = 0;
  if (area_id < 64) {
    // Agahnim stage sprite pointer table
    #define ADDR_SPRITE_PTRS_AGAHNIM 0x89CA21

    uint16_t sprite_ptr_offset = Rom_ReadWord(rom, ADDR_SPRITE_PTRS_AGAHNIM + area_id * 2);
    uint32_t sprite_data_addr = 0x890000 + sprite_ptr_offset;

    // Read sprite entries until 0xFF terminator
    for (int i = 0; i < MAX_AREA_SPRITES; i++) {
      uint8_t y = Rom_ReadByte(rom, sprite_data_addr + i * 3);
      if (y == 0xFF) break;  // End marker

      uint8_t x = Rom_ReadByte(rom, sprite_data_addr + i * 3 + 1);
      uint8_t sprite_id = Rom_ReadByte(rom, sprite_data_addr + i * 3 + 2);

      area->sprites[area->sprite_count].x = x;
      area->sprites[area->sprite_count].y = y;
      area->sprites[area->sprite_count].sprite_id = sprite_id;
      area->sprite_count++;
    }
  }

  // Extract secret items (areas 0-127 only)
  area->item_count = 0;
  if (area_id < 128) {
    uint16_t item_ptr_offset = Rom_ReadWord(rom, ADDR_ITEM_PTRS + area_id * 2);
    uint32_t item_data_addr = 0x9B0000 + item_ptr_offset;

    // Read item entries until 0xFFFF terminator
    for (int i = 0; i < MAX_AREA_ITEMS; i++) {
      uint16_t pos = Rom_ReadWord(rom, item_data_addr + i * 3);
      if (pos == 0xFFFF) break;  // End marker

      uint8_t item_id = Rom_ReadByte(rom, item_data_addr + i * 3 + 2);

      area->items[area->item_count].pos = pos;
      area->items[area->item_count].item_id = item_id;
      area->item_count++;
    }
  }

  return area;
}

OverworldArea** Overworld_ExtractAll(Rom *rom) {
  if (!rom) {
    LogError("Invalid ROM pointer");
    return NULL;
  }

  OverworldArea **areas = (OverworldArea**)calloc(OVERWORLD_AREA_COUNT, sizeof(OverworldArea*));
  if (!areas) {
    LogError("Failed to allocate overworld areas array");
    return NULL;
  }

  for (int i = 0; i < OVERWORLD_AREA_COUNT; i++) {
    areas[i] = Overworld_ExtractArea(rom, i);
    if (!areas[i]) {
      LogWarn("Failed to extract overworld area %d", i);
      // Continue with other areas
    }
  }

  return areas;
}

void Overworld_FreeArea(OverworldArea *area) {
  if (area) {
    free(area);
  }
}

void Overworld_FreeAll(OverworldArea **areas, int count) {
  if (!areas) return;

  for (int i = 0; i < count; i++) {
    Overworld_FreeArea(areas[i]);
  }

  free(areas);
}
