// overworld.h - Overworld data extraction
// Extracts map data, sprites, exits, entrances for 160 overworld areas

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "types.h"

#define OVERWORLD_AREA_COUNT 160
#define MAX_AREA_SPRITES 64
#define MAX_AREA_ITEMS 16
#define MAX_AREA_ENTRANCES 16

// Overworld area size
typedef enum {
  AREA_SIZE_SMALL,  // 16x16 tiles
  AREA_SIZE_BIG,    // 32x32 tiles
} AreaSize;

// Sprite placement in overworld area
typedef struct {
  uint8_t x;          // X coordinate
  uint8_t y;          // Y coordinate
  uint8_t sprite_id;  // Sprite type ID
} AreaSprite;

// Secret item (under bush, rock, etc.)
typedef struct {
  uint16_t pos;       // Position (Y << 8 | X)
  uint8_t item_id;    // Item type
} AreaItem;

// Overworld area data
typedef struct {
  uint8_t area_id;          // 0-159

  // Graphics & presentation
  AreaSize size;            // Small (16x16) or big (32x32)
  uint8_t gfx_id;           // Graphics tileset ID
  uint8_t palette_id;       // Palette ID

  // Audio
  uint8_t music_track;      // Background music
  uint8_t ambient_sfx;      // Ambient sound effects

  // Sign text
  uint16_t sign_text_addr;  // Pointer to sign text (if any)

  // Sprites (enemies, NPCs)
  uint8_t sprite_count;
  AreaSprite sprites[MAX_AREA_SPRITES];

  // Secret items
  uint8_t item_count;
  AreaItem items[MAX_AREA_ITEMS];

  // TODO: Add more fields as needed
  // - Map data (compressed tilemap)
  // - Exits (screen transitions)
  // - Entrances (where Link appears)
} OverworldArea;

// Extract overworld area data from ROM
OverworldArea* Overworld_ExtractArea(Rom *rom, uint8_t area_id);

// Extract all 160 overworld areas
OverworldArea** Overworld_ExtractAll(Rom *rom);

// Free overworld area data
void Overworld_FreeArea(OverworldArea *area);
void Overworld_FreeAll(OverworldArea **areas, int count);
