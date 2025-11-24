// text.h - ALTTP text extraction and decoding
#ifndef RESTOOL_TEXT_H
#define RESTOOL_TEXT_H

#include "types.h"
#include <stdint.h>
#include <stdbool.h>

// ALTTP uses custom character encoding
// Character codes map to specific glyphs in the font tileset

// Text control codes (special characters)
#define TEXT_END           0x7F  // End of string
#define TEXT_NEWLINE       0x76  // Line break
#define TEXT_SCROLL        0x77  // Scroll text box
#define TEXT_WAIT          0x78  // Wait for button press
#define TEXT_CHOICE        0x74  // Present choice to player
#define TEXT_SPEED_SLOW    0x79  // Slow text speed
#define TEXT_SPEED_NORMAL  0x7A  // Normal text speed
#define TEXT_SPEED_FAST    0x7B  // Fast text speed

// Text string structure
typedef struct {
  uint16_t offset;    // Offset from text bank start
  uint16_t length;    // Length in bytes (0 = unknown/terminated)
  char *text;         // Decoded ASCII text (malloc'd)
} TextString;

// Text bank structure (collection of strings)
typedef struct {
  uint32_t bank_addr; // SNES address of text bank
  uint32_t count;     // Number of strings
  TextString *strings; // Array of strings
} TextBank;

// Character mapping (SNES code → ASCII)
// Returns ASCII char, or 0 if control code/unmapped
char Text_DecodeChar(uint8_t snes_char);

// Decode SNES text string to ASCII
// Returns malloc'd string (caller must free)
// Handles control codes (newlines, etc.)
char* Text_DecodeString(const uint8_t *data, size_t max_len, size_t *out_len);

// Extract text from address (reads until TEXT_END)
char* Text_Extract(Rom *rom, uint32_t snes_addr, size_t *out_len);

// Extract text bank (multiple strings with pointer table)
TextBank* Text_ExtractBank(Rom *rom, uint32_t ptr_table_addr, uint32_t text_base_addr, uint32_t count);

// Free text bank
void Text_FreeBank(TextBank *bank);

// Common text banks in ALTTP
#define TEXT_BANK_DIALOGUE    0x0E0000  // Main dialogue text
#define TEXT_BANK_MENUS       0x0E8000  // Menu text
#define TEXT_BANK_ITEMS       0x0E9000  // Item names/descriptions
#define TEXT_BANK_LOCATIONS   0x0EA000  // Location names

#endif // RESTOOL_TEXT_H
