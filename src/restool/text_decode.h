// text_decode.h - Multi-language text decoding for ALTTP dialogue extraction
// Matches Python's text_compression.py decode_strings_generic functionality
#ifndef RESTOOL_TEXT_DECODE_H
#define RESTOOL_TEXT_DECODE_H

#include "types.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Language configuration structure
typedef struct {
  const char **alphabet;
  size_t alphabet_size;
  const char **dictionary;
  size_t dictionary_size;
  const uint8_t *command_lengths;
  const char **command_names;
  size_t command_count;
  uint32_t rom_addrs[3];  // Up to 3 bank addresses
  size_t rom_addr_count;
  uint8_t COMMAND_START;
  uint8_t SWITCH_BANK;
  uint8_t FINISH;
  uint8_t DICT_BASE_DEC;
  uint8_t ESCAPE_CHARACTER;  // 0 means no escape character
  bool has_escape;
} LanguageConfig;

// Decoded string result
typedef struct {
  char *text;           // Decoded text string (malloc'd)
  uint8_t *raw_bytes;   // Raw bytes from ROM (malloc'd)
  size_t raw_len;       // Length of raw bytes
} DecodedString;

// Decoded strings array
typedef struct {
  DecodedString *strings;
  size_t count;
  size_t capacity;
} DecodedStringsArray;

// Get language configuration by code
// Returns NULL if language not supported
const LanguageConfig* TextDecode_GetLanguageConfig(const char *lang_code);

// Get language code from RomLanguage enum
const char* TextDecode_GetLanguageCode(RomLanguage lang);

// Decode all dialogue strings from ROM
// Returns array of decoded strings (caller must free with TextDecode_FreeStrings)
DecodedStringsArray* TextDecode_DecodeStrings(Rom *rom, const char *lang_code);

// Free decoded strings array
void TextDecode_FreeStrings(DecodedStringsArray *strings);

// Write decoded strings to dialogue text file
// Filename format: "dialogue.txt" for US, "dialogue_<lang>.txt" for others
bool TextDecode_WriteDialogueFile(const DecodedStringsArray *strings, const char *lang_code, const char *output_dir);

// Get dialogue filename for language
// Returns malloc'd string (caller must free)
char* TextDecode_GetDialogueFilename(const char *lang_code);

#endif // RESTOOL_TEXT_DECODE_H
