// text.c - ALTTP text extraction and decoding implementation
#include "text.h"
#include "restool_util.h"
#include "../logging.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ALTTP character encoding table (SNES code → ASCII)
// Based on disassembly: https://github.com/zelda-archive/z3-disassembly
static const char kCharTable[128] = {
  // 0x00-0x0F
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
  // 0x10-0x1F
  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', ' ', ' ', ' ', ' ', ' ', ' ',
  // 0x20-0x2F
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
  // 0x30-0x3F
  'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', ' ', ' ', ' ', ' ', ' ', ' ',
  // 0x40-0x4F
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '!', '?', '-', '.', ',', ' ',
  // 0x50-0x5F
  '>', '<', '(', ')', '\'', '\'', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  // 0x60-0x6F
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  // 0x70-0x7F (control codes)
  ' ', ' ', ' ', ' ', '\n', ' ', '\n', '\n', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '\0'
};

char Text_DecodeChar(uint8_t snes_char) {
  if (snes_char >= 128) return 0;  // Invalid
  return kCharTable[snes_char];
}

char* Text_DecodeString(const uint8_t *data, size_t max_len, size_t *out_len) {
  if (!data || max_len == 0) return NULL;

  // First pass: calculate length
  size_t len = 0;
  for (size_t i = 0; i < max_len; i++) {
    uint8_t c = data[i];
    if (c == TEXT_END) break;
    
    // Count actual output characters (skip some control codes)
    if (c == TEXT_NEWLINE || c == TEXT_SCROLL) {
      len++;  // Convert to \n
    } else if (c < 128 && kCharTable[c] != '\0') {
      len++;
    }
  }

  // Allocate buffer (+1 for null terminator)
  char *str = (char*)malloc(len + 1);
  if (!str) {
    LogError("Failed to allocate text string");
    return NULL;
  }

  // Second pass: decode
  size_t pos = 0;
  for (size_t i = 0; i < max_len && pos < len; i++) {
    uint8_t c = data[i];
    if (c == TEXT_END) break;

    if (c == TEXT_NEWLINE || c == TEXT_SCROLL) {
      str[pos++] = '\n';
    } else if (c < 128) {
      char decoded = kCharTable[c];
      if (decoded != '\0') {
        str[pos++] = decoded;
      }
    }
  }

  str[pos] = '\0';
  if (out_len) *out_len = pos;
  return str;
}

char* Text_Extract(Rom *rom, uint32_t snes_addr, size_t *out_len) {
  if (!rom) return NULL;

  // Read up to 512 bytes (max reasonable text length)
  uint8_t *data = Rom_ReadPtr(rom, snes_addr, 512);
  if (!data) return NULL;

  return Text_DecodeString(data, 512, out_len);
}

TextBank* Text_ExtractBank(Rom *rom, uint32_t ptr_table_addr, uint32_t text_base_addr, uint32_t count) {
  if (!rom || count == 0) return NULL;

  TextBank *bank = (TextBank*)calloc(1, sizeof(TextBank));
  if (!bank) {
    LogError("Failed to allocate TextBank");
    return NULL;
  }

  bank->bank_addr = text_base_addr;
  bank->count = count;
  bank->strings = (TextString*)calloc(count, sizeof(TextString));
  if (!bank->strings) {
    LogError("Failed to allocate TextString array");
    free(bank);
    return NULL;
  }

  // Read pointer table (2 bytes per pointer)
  for (uint32_t i = 0; i < count; i++) {
    uint16_t offset = Rom_ReadWord(rom, ptr_table_addr + i * 2);
    bank->strings[i].offset = offset;

    // Extract text from base + offset
    uint32_t text_addr = text_base_addr + offset;
    size_t len = 0;
    bank->strings[i].text = Text_Extract(rom, text_addr, &len);
    bank->strings[i].length = (uint16_t)len;
  }

  return bank;
}

void Text_FreeBank(TextBank *bank) {
  if (!bank) return;

  if (bank->strings) {
    for (uint32_t i = 0; i < bank->count; i++) {
      if (bank->strings[i].text) {
        free(bank->strings[i].text);
      }
    }
    free(bank->strings);
  }

  free(bank);
}
