// restool_util.c - Core utilities implementation
#include "restool_util.h"
#include "../platform.h"
#include "../logging.h"
#include "../rom_sha1.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Windows compatibility for POSIX functions
#ifdef _WIN32
  #define strcasecmp _stricmp
#else
  #include <strings.h>  // For strcasecmp on POSIX systems
#endif

// ============================================================================
// SNES Address Conversion
// ============================================================================

uint32_t SnesAddrToRomOffset(uint32_t snes_addr) {
  uint8_t bank = (snes_addr >> 16) & 0xFF;
  uint16_t offset = snes_addr & 0xFFFF;

  // LoROM mapping: $8000-$FFFF in each bank maps to ROM
  if (offset < 0x8000) {
    return 0xFFFFFFFF;  // Invalid address
  }

  // Banks $80-$FF are mirrors of $00-$7F in standard LoROM
  // Mask to 7 bits to handle both ranges
  uint8_t effective_bank = bank & 0x7F;

  return (effective_bank * 0x8000) + (offset - 0x8000);
}

// ============================================================================
// ROM Loading and Access
// ============================================================================

Rom* Rom_Load(const char *path) {
  Rom *rom = calloc(1, sizeof(Rom));
  if (!rom) {
    LogError("Failed to allocate ROM structure");
    return NULL;
  }

  // Read entire ROM file
  rom->data = Platform_ReadWholeFile(path, &rom->size);
  if (!rom->data) {
    LogError("Failed to read ROM file: %s", path);
    free(rom);
    return NULL;
  }

  // Check for SMC header (512 bytes)
  if (rom->size % 1024 == 512) {
    LogInfo("Detected SMC header (512 bytes), skipping it");
    rom->has_smc_header = true;
    // Move data pointer past SMC header
    memmove(rom->data, rom->data + 512, rom->size - 512);
    rom->size -= 512;
  } else {
    rom->has_smc_header = false;
  }

  // Calculate SHA-1 using shared library
  RomSha1_Calculate(rom->data, rom->size, rom->sha1);
  LogInfo("ROM SHA-1: %s", rom->sha1);

  // Identify ROM language from SHA-1
  Rom_IdentifyLanguage(rom);

  // Workaround for Swedish ROM with broken size (has 0x200 header but non-standard total size)
  // Python: if self.language == 'sv' and len(self.ROM) == 0x10083b: self.ROM = self.ROM[0x200:]
  if (rom->language == ROM_LANG_SV && rom->size == 0x10083b) {
    LogInfo("Detected Swedish ROM with broken header, stripping 0x200 bytes");
    memmove(rom->data, rom->data + 0x200, rom->size - 0x200);
    rom->size -= 0x200;
    rom->has_smc_header = true;
  }

  // Validate ROM size (typical SNES ROMs are 1MB, 2MB, or 4MB)
  if (rom->size < 512 * 1024) {
    LogWarn("ROM size seems small: %zu bytes", rom->size);
  } else if (rom->size > 6 * 1024 * 1024) {
    LogWarn("ROM size seems large: %zu bytes", rom->size);
  }

  return rom;
}

void Rom_Free(Rom *rom) {
  if (rom) {
    free(rom->data);
    free(rom);
  }
}

uint8_t Rom_ReadByte(Rom *rom, uint32_t snes_addr) {
  uint32_t offset = SnesAddrToRomOffset(snes_addr);
  if (offset == 0xFFFFFFFF || offset >= rom->size) {
    LogError("Invalid SNES address: $%06X (offset: $%X)", snes_addr, offset);
    return 0;
  }
  return rom->data[offset];
}

uint16_t Rom_ReadWord(Rom *rom, uint32_t snes_addr) {
  uint32_t offset = SnesAddrToRomOffset(snes_addr);
  if (offset == 0xFFFFFFFF || offset + 1 >= rom->size) {
    LogError("Invalid SNES address for word read: $%06X", snes_addr);
    return 0;
  }
  // Little-endian
  return rom->data[offset] | (rom->data[offset + 1] << 8);
}

uint32_t Rom_ReadAddr(Rom *rom, uint32_t snes_addr) {
  uint32_t offset = SnesAddrToRomOffset(snes_addr);
  if (offset == 0xFFFFFFFF || offset + 2 >= rom->size) {
    LogError("Invalid SNES address for addr read: $%06X", snes_addr);
    return 0;
  }
  // 24-bit little-endian
  return rom->data[offset] | (rom->data[offset + 1] << 8) | (rom->data[offset + 2] << 16);
}

uint8_t* Rom_ReadPtr(Rom *rom, uint32_t snes_addr, size_t len) {
  uint32_t offset = SnesAddrToRomOffset(snes_addr);
  if (offset == 0xFFFFFFFF || offset + len > rom->size) {
    LogError("Invalid SNES address or length: $%06X, len=%zu", snes_addr, len);
    return NULL;
  }
  return &rom->data[offset];
}

bool Rom_ValidateSHA1(Rom *rom, const char *expected_sha1) {
  return strcasecmp(rom->sha1, expected_sha1) == 0;
}

// ROM language identification table (matching Python's util.py ZELDA3_SHA1)
typedef struct {
  const char *sha1;
  RomLanguage language;
  const char *name;
  const char *code;
} RomLanguageEntry;

static const RomLanguageEntry kRomLanguages[] = {
  { ROM_SHA1_USA,    ROM_LANG_US,    "Legend of Zelda, The - A Link to the Past (USA)",     "us" },
  { ROM_SHA1_DE,     ROM_LANG_DE,    "Legend of Zelda, The - A Link to the Past (Germany)", "de" },
  { ROM_SHA1_FR,     ROM_LANG_FR,    "Legend of Zelda, The - A Link to the Past (France)",  "fr" },
  { ROM_SHA1_FR_C,   ROM_LANG_FR_C,  "Legend of Zelda, The - A Link to the Past (Canada)",  "fr-c" },
  { ROM_SHA1_EN,     ROM_LANG_EN,    "Legend of Zelda, The - A Link to the Past (Europe)",  "en" },
  { ROM_SHA1_ES,     ROM_LANG_ES,    "Spanish - https://www.romhacking.net/translations/2195/", "es" },
  { ROM_SHA1_PL,     ROM_LANG_PL,    "Polish - https://www.romhacking.net/translations/5760/",  "pl" },
  { ROM_SHA1_PT,     ROM_LANG_PT,    "Portuguese - https://www.romhacking.net/translations/6530/", "pt" },
  { ROM_SHA1_REDUX1, ROM_LANG_REDUX, "English Redux - https://www.romhacking.net/translations/6657/", "redux" },
  { ROM_SHA1_REDUX2, ROM_LANG_REDUX, "English Redux - https://www.romhacking.net/hacks/2594/", "redux" },
  { ROM_SHA1_REDUX3, ROM_LANG_REDUX, "English Redux v10.2.3 - https://www.romhacking.net/hacks/2594/", "redux" },
  { ROM_SHA1_REDUX4, ROM_LANG_REDUX, "English Redux v10.2.4 - https://www.romhacking.net/hacks/2594/", "redux" },
  { ROM_SHA1_REDUX5, ROM_LANG_REDUX, "English Redux v10.2.3 - https://www.romhacking.net/hacks/2594/", "redux" },
  { ROM_SHA1_NL,     ROM_LANG_NL,    "Dutch - https://www.romhacking.net/translations/1124/", "nl" },
  { ROM_SHA1_SV,     ROM_LANG_SV,    "Swedish - https://www.romhacking.net/translations/982/", "sv" },
  { ROM_SHA1_RETRANS_KAL, ROM_LANG_RETRANS_KAL, "English Retranslation (Kaleidoscope v1.0) - https://www.romhacking.net/hacks/5526/", "retrans-kal" },
  { NULL, ROM_LANG_UNKNOWN, NULL, NULL }  // Terminator
};

void Rom_IdentifyLanguage(Rom *rom) {
  rom->language = ROM_LANG_UNKNOWN;
  rom->language_name = "Unknown";

  for (const RomLanguageEntry *entry = kRomLanguages; entry->sha1 != NULL; entry++) {
    if (strcasecmp(rom->sha1, entry->sha1) == 0) {
      rom->language = entry->language;
      rom->language_name = entry->name;
      return;
    }
  }
}

const char* Rom_GetLanguageCode(RomLanguage lang) {
  for (const RomLanguageEntry *entry = kRomLanguages; entry->sha1 != NULL; entry++) {
    if (entry->language == lang) {
      return entry->code;
    }
  }
  return "unknown";
}

// ============================================================================
// SNES Decompression
// ============================================================================

DecompressedData* Snes_DecompressBuffer(const uint8_t *src, size_t src_len, bool big_endian_offsets) {
  DecompressedData *result = calloc(1, sizeof(DecompressedData));
  if (!result) {
    LogError("Failed to allocate decompression result");
    return NULL;
  }

  // Initial capacity
  result->capacity = 65536;  // 64 KB should be enough for most assets
  result->data = malloc(result->capacity);
  if (!result->data) {
    LogError("Failed to allocate decompression buffer");
    free(result);
    return NULL;
  }

  size_t src_pos = 0;
  result->size = 0;

  while (src_pos < src_len) {
    uint8_t cmd_byte = src[src_pos++];

    // Check for terminator (0xFF)
    if (cmd_byte == 0xFF) {
      result->compressed_size = src_pos;
      break;
    }

    // Check if we need to grow buffer
    if (result->size + 65536 > result->capacity) {
      result->capacity *= 2;
      uint8_t *new_data = realloc(result->data, result->capacity);
      if (!new_data) {
        LogError("Failed to grow decompression buffer");
        Snes_FreeDecompressed(result);
        return NULL;
      }
      result->data = new_data;
    }

    // Parse command byte (matching Python's logic)
    uint8_t cmd;
    uint16_t length;

    if ((cmd_byte & 0xE0) != 0xE0) {
      // Normal format: top 3 bits = command, bottom 5 bits = length
      length = cmd_byte & 0x1F;
      cmd = cmd_byte & 0xE0;
    } else {
      // Extended format (when top 3 bits are 111): bits 2-4 = command, bits 0-1 + next byte = length
      cmd = (cmd_byte << 3) & 0xE0;
      if (src_pos >= src_len) {
        LogError("Extended format missing length byte");
        Snes_FreeDecompressed(result);
        return NULL;
      }
      length = ((cmd_byte & 0x03) << 8) | src[src_pos++];
    }

    // Length is incremented by 1 (Python: lx += 1)
    length++;

    // Convert cmd bits (0x00, 0x20, 0x40, 0x60, 0x80+) to cmd_type (0-4)
    uint8_t cmd_type;
    if (cmd == 0x00) {
      cmd_type = 0;  // Literal
    } else if (cmd & 0x80) {
      cmd_type = 4;  // Copy from buffer
    } else if ((cmd & 0x40) == 0) {
      cmd_type = 1;  // Memset
    } else if ((cmd & 0x20) == 0) {
      cmd_type = 2;  // Memset16
    } else {
      cmd_type = 3;  // Increment
    }

    switch (cmd_type) {
      case 0: {  // Direct copy
        if (src_pos + length > src_len) {
          LogError("Direct copy exceeds source bounds");
          Snes_FreeDecompressed(result);
          return NULL;
        }
        memcpy(&result->data[result->size], &src[src_pos], length);
        result->size += length;
        src_pos += length;
        break;
      }

      case 1: {  // RLE memset (repeat byte)
        if (src_pos >= src_len) {
          LogError("RLE memset missing byte");
          Snes_FreeDecompressed(result);
          return NULL;
        }
        uint8_t byte = src[src_pos++];
        memset(&result->data[result->size], byte, length);
        result->size += length;
        break;
      }

      case 2: {  // RLE memset16 (repeat word)
        if (src_pos + 1 >= src_len) {
          LogError("RLE memset16 missing word");
          Snes_FreeDecompressed(result);
          return NULL;
        }
        uint8_t lo = src[src_pos++];
        uint8_t hi = src[src_pos++];
        // Python: writes 'length' bytes alternating lo/hi, handles odd lengths
        while (length > 0) {
          result->data[result->size++] = lo;
          if (length == 1) break;  // Odd length - only write lo byte
          result->data[result->size++] = hi;
          length -= 2;
        }
        break;
      }

      case 3: {  // Incremental fill
        if (src_pos >= src_len) {
          LogError("Incremental fill missing start byte");
          Snes_FreeDecompressed(result);
          return NULL;
        }
        uint8_t byte = src[src_pos++];
        for (uint16_t i = 0; i < length; i++) {
          result->data[result->size++] = byte++;
        }
        break;
      }

      default: {  // Copy from buffer (absolute index reference)
        if (src_pos + 1 >= src_len) {
          LogError("Copy from buffer missing offset");
          Snes_FreeDecompressed(result);
          return NULL;
        }
        // Read offset with correct endianness
        uint16_t offset;
        if (big_endian_offsets) {
          // Big-endian: most SNES data (Python offset_is_be=True)
          offset = src[src_pos++] << 8;
          offset |= src[src_pos++];
        } else {
          // Little-endian: sprite/bg graphics (Python offset_is_be=False)
          offset = src[src_pos++];
          offset |= src[src_pos++] << 8;
        }

        if (offset >= result->size) {
          LogError("Copy from buffer: offset too large (offset=%u, size=%zu)", offset, result->size);
          Snes_FreeDecompressed(result);
          return NULL;
        }

        // Copy from absolute index in output buffer (Python uses: result.append(result[offs]))
        size_t copy_from = offset;
        for (uint16_t i = 0; i < length; i++) {
          result->data[result->size++] = result->data[copy_from++];
        }
        break;
      }
    }
  }

  // Shrink to fit
  uint8_t *final_data = realloc(result->data, result->size);
  if (final_data) {
    result->data = final_data;
    result->capacity = result->size;
  }

  // Set compressed size if not already set (by terminator)
  if (result->compressed_size == 0) {
    result->compressed_size = src_pos;
  }

  LogInfo("Decompressed %zu bytes to %zu bytes", result->compressed_size, result->size);
  return result;
}

DecompressedData* Snes_Decompress(Rom *rom, uint32_t snes_addr, bool big_endian_offsets) {
  // For now, decompress up to 64KB from this address
  // TODO: Proper end-of-stream detection
  uint8_t *src = Rom_ReadPtr(rom, snes_addr, 65536);
  if (!src) {
    return NULL;
  }

  return Snes_DecompressBuffer(src, 65536, big_endian_offsets);
}

void Snes_FreeDecompressed(DecompressedData *data) {
  if (data) {
    free(data->data);
    free(data);
  }
}

// ============================================================================
// SNES Compression (TODO: Implement in next phase)
// ============================================================================

CompressedData* Snes_Compress(const uint8_t *src, size_t src_len) {
  (void)src;
  (void)src_len;
  // TODO: Implement compression
  LogWarn("SNES compression not yet implemented");
  return NULL;
}

void Snes_FreeCompressed(CompressedData *data) {
  if (data) {
    free(data->data);
    free(data);
  }
}
