// restool_util.c - Core utilities implementation
#include "restool_util.h"
#include "../platform.h"
#include "../logging.h"
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
// SHA-1 Implementation (for ROM validation)
// ============================================================================

// Simple SHA-1 implementation based on RFC 3174
typedef struct {
  uint32_t state[5];
  uint32_t count[2];
  uint8_t buffer[64];
} SHA1_CTX;

#define SHA1_ROL(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

#define SHA1_BLK0(i) (block->l[i] = (SHA1_ROL(block->l[i],24) & 0xFF00FF00) \
    | (SHA1_ROL(block->l[i],8) & 0x00FF00FF))
#define SHA1_BLK(i) (block->l[i&15] = SHA1_ROL(block->l[(i+13)&15] ^ \
    block->l[(i+8)&15] ^ block->l[(i+2)&15] ^ block->l[i&15],1))

#define SHA1_R0(v,w,x,y,z,i) z+=((w&(x^y))^y)+SHA1_BLK0(i)+0x5A827999+SHA1_ROL(v,5);w=SHA1_ROL(w,30);
#define SHA1_R1(v,w,x,y,z,i) z+=((w&(x^y))^y)+SHA1_BLK(i)+0x5A827999+SHA1_ROL(v,5);w=SHA1_ROL(w,30);
#define SHA1_R2(v,w,x,y,z,i) z+=(w^x^y)+SHA1_BLK(i)+0x6ED9EBA1+SHA1_ROL(v,5);w=SHA1_ROL(w,30);
#define SHA1_R3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+SHA1_BLK(i)+0x8F1BBCDC+SHA1_ROL(v,5);w=SHA1_ROL(w,30);
#define SHA1_R4(v,w,x,y,z,i) z+=(w^x^y)+SHA1_BLK(i)+0xCA62C1D6+SHA1_ROL(v,5);w=SHA1_ROL(w,30);

typedef union {
  uint8_t c[64];
  uint32_t l[16];
} SHA1_WORKSPACE_BLOCK;

static void SHA1_Transform(uint32_t state[5], const uint8_t buffer[64]) {
  uint32_t a, b, c, d, e;
  // Use local workspace to avoid modifying input buffer
  SHA1_WORKSPACE_BLOCK workspace;
  memcpy(&workspace, buffer, 64);
  SHA1_WORKSPACE_BLOCK *block = &workspace;

  a = state[0];
  b = state[1];
  c = state[2];
  d = state[3];
  e = state[4];

  SHA1_R0(a,b,c,d,e, 0); SHA1_R0(e,a,b,c,d, 1); SHA1_R0(d,e,a,b,c, 2); SHA1_R0(c,d,e,a,b, 3);
  SHA1_R0(b,c,d,e,a, 4); SHA1_R0(a,b,c,d,e, 5); SHA1_R0(e,a,b,c,d, 6); SHA1_R0(d,e,a,b,c, 7);
  SHA1_R0(c,d,e,a,b, 8); SHA1_R0(b,c,d,e,a, 9); SHA1_R0(a,b,c,d,e,10); SHA1_R0(e,a,b,c,d,11);
  SHA1_R0(d,e,a,b,c,12); SHA1_R0(c,d,e,a,b,13); SHA1_R0(b,c,d,e,a,14); SHA1_R0(a,b,c,d,e,15);
  SHA1_R1(e,a,b,c,d,16); SHA1_R1(d,e,a,b,c,17); SHA1_R1(c,d,e,a,b,18); SHA1_R1(b,c,d,e,a,19);
  SHA1_R2(a,b,c,d,e,20); SHA1_R2(e,a,b,c,d,21); SHA1_R2(d,e,a,b,c,22); SHA1_R2(c,d,e,a,b,23);
  SHA1_R2(b,c,d,e,a,24); SHA1_R2(a,b,c,d,e,25); SHA1_R2(e,a,b,c,d,26); SHA1_R2(d,e,a,b,c,27);
  SHA1_R2(c,d,e,a,b,28); SHA1_R2(b,c,d,e,a,29); SHA1_R2(a,b,c,d,e,30); SHA1_R2(e,a,b,c,d,31);
  SHA1_R2(d,e,a,b,c,32); SHA1_R2(c,d,e,a,b,33); SHA1_R2(b,c,d,e,a,34); SHA1_R2(a,b,c,d,e,35);
  SHA1_R2(e,a,b,c,d,36); SHA1_R2(d,e,a,b,c,37); SHA1_R2(c,d,e,a,b,38); SHA1_R2(b,c,d,e,a,39);
  SHA1_R3(a,b,c,d,e,40); SHA1_R3(e,a,b,c,d,41); SHA1_R3(d,e,a,b,c,42); SHA1_R3(c,d,e,a,b,43);
  SHA1_R3(b,c,d,e,a,44); SHA1_R3(a,b,c,d,e,45); SHA1_R3(e,a,b,c,d,46); SHA1_R3(d,e,a,b,c,47);
  SHA1_R3(c,d,e,a,b,48); SHA1_R3(b,c,d,e,a,49); SHA1_R3(a,b,c,d,e,50); SHA1_R3(e,a,b,c,d,51);
  SHA1_R3(d,e,a,b,c,52); SHA1_R3(c,d,e,a,b,53); SHA1_R3(b,c,d,e,a,54); SHA1_R3(a,b,c,d,e,55);
  SHA1_R3(e,a,b,c,d,56); SHA1_R3(d,e,a,b,c,57); SHA1_R3(c,d,e,a,b,58); SHA1_R3(b,c,d,e,a,59);
  SHA1_R4(a,b,c,d,e,60); SHA1_R4(e,a,b,c,d,61); SHA1_R4(d,e,a,b,c,62); SHA1_R4(c,d,e,a,b,63);
  SHA1_R4(b,c,d,e,a,64); SHA1_R4(a,b,c,d,e,65); SHA1_R4(e,a,b,c,d,66); SHA1_R4(d,e,a,b,c,67);
  SHA1_R4(c,d,e,a,b,68); SHA1_R4(b,c,d,e,a,69); SHA1_R4(a,b,c,d,e,70); SHA1_R4(e,a,b,c,d,71);
  SHA1_R4(d,e,a,b,c,72); SHA1_R4(c,d,e,a,b,73); SHA1_R4(b,c,d,e,a,74); SHA1_R4(a,b,c,d,e,75);
  SHA1_R4(e,a,b,c,d,76); SHA1_R4(d,e,a,b,c,77); SHA1_R4(c,d,e,a,b,78); SHA1_R4(b,c,d,e,a,79);

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
}

static void SHA1_Init(SHA1_CTX *ctx) {
  ctx->state[0] = 0x67452301;
  ctx->state[1] = 0xEFCDAB89;
  ctx->state[2] = 0x98BADCFE;
  ctx->state[3] = 0x10325476;
  ctx->state[4] = 0xC3D2E1F0;
  ctx->count[0] = ctx->count[1] = 0;
}

static void SHA1_Update(SHA1_CTX *ctx, const uint8_t *data, size_t len) {
  size_t i, j;

  j = (ctx->count[0] >> 3) & 63;
  if ((ctx->count[0] += len << 3) < (len << 3))
    ctx->count[1]++;
  ctx->count[1] += (len >> 29);

  if ((j + len) > 63) {
    memcpy(&ctx->buffer[j], data, (i = 64-j));
    SHA1_Transform(ctx->state, ctx->buffer);
    for ( ; i + 63 < len; i += 64) {
      SHA1_Transform(ctx->state, &data[i]);
    }
    j = 0;
  } else {
    i = 0;
  }
  memcpy(&ctx->buffer[j], &data[i], len - i);
}

static void SHA1_Final(uint8_t digest[20], SHA1_CTX *ctx) {
  uint32_t i;
  uint8_t finalcount[8];

  for (i = 0; i < 8; i++) {
    finalcount[i] = (uint8_t)((ctx->count[(i >= 4 ? 0 : 1)] >> ((3-(i & 3)) * 8) ) & 255);
  }

  SHA1_Update(ctx, (const uint8_t *)"\200", 1);
  while ((ctx->count[0] & 504) != 448) {
    SHA1_Update(ctx, (const uint8_t *)"\0", 1);
  }
  SHA1_Update(ctx, finalcount, 8);

  for (i = 0; i < 20; i++) {
    digest[i] = (uint8_t)((ctx->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);
  }
}

void SHA1ToHex(const uint8_t hash[20], char *out_hex) {
  for (int i = 0; i < 20; i++) {
    sprintf(out_hex + i*2, "%02x", hash[i]);
  }
  out_hex[40] = '\0';
}

void CalculateSHA1(const uint8_t *data, size_t len, char *out_hex) {
  SHA1_CTX ctx;
  uint8_t hash[20];

  SHA1_Init(&ctx);
  SHA1_Update(&ctx, data, len);
  SHA1_Final(hash, &ctx);
  SHA1ToHex(hash, out_hex);
}

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

  // Calculate SHA-1 (no longer needs buffer copy since SHA1_Transform uses local workspace)
  CalculateSHA1(rom->data, rom->size, rom->sha1);
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
