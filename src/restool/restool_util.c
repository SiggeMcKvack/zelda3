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
  size_t size;
  uint8_t *data = Platform_ReadWholeFile(path, &size);
  if (!data) {
    LogError("Failed to read ROM file: %s", path);
    return NULL;
  }
  return Rom_LoadFromData(data, size);
}

Rom* Rom_LoadFromData(uint8_t *data, size_t size) {
  Rom *rom = calloc(1, sizeof(Rom));
  if (!rom) {
    LogError("Failed to allocate ROM structure");
    free(data);
    return NULL;
  }
  rom->data = data;
  rom->size = size;

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

bool Rom_IsPatchPath(const char *path) {
  static const char *const kExts[] = { ".ips", ".vcdiff", ".xdelta" };
  size_t n = path ? strlen(path) : 0;
  for (int i = 0; i < 3; i++) {
    size_t e = strlen(kExts[i]);
    if (n >= e && strcasecmp(path + n - e, kExts[i]) == 0)
      return true;
  }
  return false;
}

// Applies an IPS patch to `base`. Patch offsets are reduced by `shift`; data
// that would land before offset 0 is dropped (it targets a copier header).
// Returns a malloc'd buffer, or NULL if the patch is malformed.
static uint8_t *ApplyIps(const uint8_t *base, size_t base_size, const uint8_t *ips,
                         size_t ips_size, size_t shift, size_t *out_size) {
  enum { kMaxRomSize = 16 * 1024 * 1024 };
  if (ips_size < 8 || memcmp(ips, "PATCH", 5) != 0)
    return NULL;
  uint8_t *out = malloc(kMaxRomSize);
  if (!out)
    return NULL;
  memcpy(out, base, base_size);
  size_t size = base_size, i = 5;
  for (;;) {
    if (i + 3 > ips_size)
      goto fail;
    size_t ofs = ips[i] << 16 | ips[i + 1] << 8 | ips[i + 2];
    i += 3;
    if (ofs == 0x454F46)  // "EOF"
      break;
    if (i + 2 > ips_size)
      goto fail;
    size_t len = ips[i] << 8 | ips[i + 1];
    i += 2;
    const uint8_t *src = NULL;
    uint8_t fill = 0;
    if (len == 0) {  // RLE record
      if (i + 3 > ips_size)
        goto fail;
      len = ips[i] << 8 | ips[i + 1];
      fill = ips[i + 2];
      i += 3;
    } else {
      if (i + len > ips_size)
        goto fail;
      src = ips + i;
      i += len;
    }
    size_t skip = ofs < shift ? shift - ofs : 0;
    if (skip >= len)
      continue;
    size_t dst = ofs + skip - shift, n = len - skip;
    if (dst + n > kMaxRomSize)
      goto fail;
    if (dst > size)
      memset(out + size, 0, dst - size);
    if (src)
      memcpy(out + dst, src + skip, n);
    else
      memset(out + dst, fill, n);
    if (dst + n > size)
      size = dst + n;
  }
  if (i + 3 <= ips_size) {  // Optional truncation extension
    size_t trunc = ips[i] << 16 | ips[i + 1] << 8 | ips[i + 2];
    if (trunc >= shift && trunc - shift < size)
      size = trunc - shift;
  }
  *out_size = size;
  return out;
fail:
  free(out);
  return NULL;
}

// --- VCDIFF (RFC 3284, as written by xdelta3) --------------------------------
// Supports the default code table and xdelta3's Adler-32 window checksum; no
// secondary compression or custom code tables.

typedef struct { const uint8_t *p, *end; bool err; } VcdReader;

static uint32_t VcdByte(VcdReader *r) {
  if (r->p >= r->end) { r->err = true; return 0; }
  return *r->p++;
}

static uint32_t VcdInt(VcdReader *r) {
  uint32_t v = 0;
  for (int i = 0; i < 5; i++) {
    uint32_t c = VcdByte(r);
    v = v << 7 | (c & 127);
    if (!(c & 128))
      return v;
  }
  r->err = true;
  return 0;
}

enum { kVcdNoop, kVcdAdd, kVcdRun, kVcdCopy };
typedef struct { uint8_t type, size, mode; } VcdInst;

// RFC 3284 section 5.6 default code table: two instructions per opcode.
static void VcdBuildCodeTable(VcdInst t[256][2]) {
  memset(t, 0, sizeof(VcdInst) * 512);
  int i = 0;
  t[i++][0] = (VcdInst){ kVcdRun, 0, 0 };
  for (int s = 0; s <= 17; s++)
    t[i++][0] = (VcdInst){ kVcdAdd, s, 0 };
  for (int m = 0; m < 9; m++) {
    t[i++][0] = (VcdInst){ kVcdCopy, 0, m };
    for (int s = 4; s <= 18; s++)
      t[i++][0] = (VcdInst){ kVcdCopy, s, m };
  }
  for (int m = 0; m < 9; m++)
    for (int a = 1; a <= 4; a++)
      for (int c = 4; c <= (m < 6 ? 6 : 4); c++, i++) {
        t[i][0] = (VcdInst){ kVcdAdd, a, 0 };
        t[i][1] = (VcdInst){ kVcdCopy, c, m };
      }
  for (int m = 0; m < 9; m++, i++) {
    t[i][0] = (VcdInst){ kVcdCopy, 4, m };
    t[i][1] = (VcdInst){ kVcdAdd, 1, 0 };
  }
}

static uint32_t Adler32(const uint8_t *p, size_t n) {
  uint32_t a = 1, b = 0;
  for (size_t i = 0; i < n; i++) {
    a = (a + p[i]) % 65521;
    b = (b + a) % 65521;
  }
  return b << 16 | a;
}

// Decodes one window's instructions into tgt[0..tlen). seg is the source
// segment (S bytes) that COPY addresses below S refer to.
static bool VcdDecodeWindow(const VcdInst table[256][2], const uint8_t *seg, size_t S,
                            uint8_t *tgt, size_t tlen,
                            VcdReader *data, VcdReader *inst, VcdReader *addr) {
  uint32_t near[4] = { 0 }, same[768] = { 0 };
  int next_near = 0;
  size_t pos = 0;
  while (inst->p < inst->end) {
    uint32_t op = VcdByte(inst);
    for (int h = 0; h < 2; h++) {
      VcdInst in = table[op][h];
      if (in.type == kVcdNoop)
        continue;
      size_t size = in.size ? in.size : VcdInt(inst);
      if (inst->err || pos + size > tlen)
        return false;
      if (in.type == kVcdAdd) {
        if ((size_t)(data->end - data->p) < size)
          return false;
        memcpy(tgt + pos, data->p, size);
        data->p += size;
      } else if (in.type == kVcdRun) {
        memset(tgt + pos, VcdByte(data), size);
      } else {
        size_t here = S + pos, a;
        if (in.mode == 0) a = VcdInt(addr);
        else if (in.mode == 1) a = here - VcdInt(addr);
        else if (in.mode < 6) a = near[in.mode - 2] + VcdInt(addr);
        else a = same[(in.mode - 6) * 256 + VcdByte(addr)];
        near[next_near] = (uint32_t)a;
        next_near = (next_near + 1) & 3;
        same[a % 768] = (uint32_t)a;
        if (addr->err || a >= here)
          return false;
        for (size_t k = 0; k < size; k++, a++)  // byte-wise: may overlap the output
          tgt[pos + k] = a < S ? seg[a] : tgt[a - S];
      }
      pos += size;
      if (data->err)
        return false;
    }
  }
  return pos == tlen;
}

// Applies a VCDIFF patch to `base`. Returns a malloc'd buffer, or NULL if the
// patch is malformed or uses an unsupported feature.
static uint8_t *ApplyVcdiff(const uint8_t *base, size_t base_size, const uint8_t *patch,
                            size_t patch_size, size_t *out_size) {
  enum { kMaxRomSize = 16 * 1024 * 1024 };
  VcdReader r = { patch, patch + patch_size, false };
  if (patch_size < 5 || memcmp(patch, "\xD6\xC3\xC4", 3) != 0)
    return NULL;
  r.p += 4;
  uint32_t hdr = VcdByte(&r);
  if (hdr & 3) {
    LogError("VCDIFF patch uses secondary compression or a custom code table");
    return NULL;
  }
  if (hdr & 4) {  // application header (xdelta3 stores file names here)
    uint32_t n = VcdInt(&r);
    if (r.err || (size_t)(r.end - r.p) < n)
      return NULL;
    r.p += n;
  }
  static VcdInst table[256][2];
  static bool table_built;
  if (!table_built) {
    VcdBuildCodeTable(table);
    table_built = true;
  }
  uint8_t *out = malloc(kMaxRomSize);
  if (!out)
    return NULL;
  size_t size = 0;
  while (r.p < r.end) {
    uint32_t win = VcdByte(&r);
    const uint8_t *seg = NULL;
    size_t seg_len = 0;
    if (win & 3) {
      seg_len = VcdInt(&r);
      size_t seg_pos = VcdInt(&r);
      size_t limit = (win & 1) ? base_size : size;
      if ((win & 3) == 3 || seg_pos > limit || seg_len > limit - seg_pos)
        goto fail;
      seg = ((win & 1) ? base : out) + seg_pos;
    }
    uint32_t delta_len = VcdInt(&r);
    if (r.err || delta_len > (size_t)(r.end - r.p))
      goto fail;
    const uint8_t *delta_end = r.p + delta_len;
    size_t tlen = VcdInt(&r);
    uint32_t delta_ind = VcdByte(&r);
    size_t data_len = VcdInt(&r), inst_len = VcdInt(&r), addr_len = VcdInt(&r);
    uint32_t adler = 0;
    if (win & 4)
      for (int i = 0; i < 4; i++)
        adler = adler << 8 | VcdByte(&r);
    if (r.err || delta_ind != 0 || r.p > delta_end || tlen > kMaxRomSize - size ||
        data_len + inst_len + addr_len != (size_t)(delta_end - r.p))
      goto fail;
    VcdReader data = { r.p, r.p + data_len, false };
    VcdReader inst = { data.end, data.end + inst_len, false };
    VcdReader addr = { inst.end, inst.end + addr_len, false };
    if (!VcdDecodeWindow(table, seg, seg_len, out + size, tlen, &data, &inst, &addr))
      goto fail;
    if ((win & 4) && Adler32(out + size, tlen) != adler) {
      LogError("VCDIFF checksum mismatch: the base ROM does not match the patch");
      goto fail;
    }
    size += tlen;
    r.p = delta_end;
  }
  *out_size = size;
  return out;
fail:
  free(out);
  return NULL;
}

// Returns the ROM in data (taking ownership) if it is a known non-US ROM.
static Rom *KnownTranslatedRom(uint8_t *data, size_t size) {
  Rom *rom = Rom_LoadFromData(data, size);
  if (rom && rom->language != ROM_LANG_UNKNOWN && rom->language != ROM_LANG_US)
    return rom;
  Rom_Free(rom);
  return NULL;
}

Rom* Rom_LoadPatched(const char *patch_path, const char *base_rom_path) {
  Rom *base = base_rom_path ? Rom_Load(base_rom_path) : NULL;
  if (!base || base->language != ROM_LANG_US) {
    LogError("Patch needs the US ROM as base: %s", base_rom_path ? base_rom_path : "(none)");
    Rom_Free(base);
    return NULL;
  }
  size_t patch_size;
  uint8_t *patch = Platform_ReadWholeFile(patch_path, &patch_size);
  if (!patch) {
    LogError("Failed to read patch: %s", patch_path);
    Rom_Free(base);
    return NULL;
  }
  Rom *result = NULL;
  size_t size;
  if (patch_size >= 5 && memcmp(patch, "PATCH", 5) == 0) {
    // IPS patches are made for either the headerless ROM or one with a 512-byte
    // copier header. Try both and keep the result that is a known ROM.
    static const size_t kShifts[] = { 0, 512 };
    for (int k = 0; k < 2 && !result; k++) {
      uint8_t *data = ApplyIps(base->data, base->size, patch, patch_size, kShifts[k], &size);
      if (!data) {
        LogError("Malformed IPS patch: %s", patch_path);
        break;
      }
      result = KnownTranslatedRom(data, size);
    }
  } else if (patch_size >= 3 && memcmp(patch, "\xD6\xC3\xC4", 3) == 0) {
    // VCDIFF checks its source, so only the headerless ROM can match
    uint8_t *data = ApplyVcdiff(base->data, base->size, patch, patch_size, &size);
    if (data)
      result = KnownTranslatedRom(data, size);
    else
      LogError("Malformed or unsupported VCDIFF patch: %s", patch_path);
  } else {
    LogError("Unknown patch format (expected IPS or VCDIFF): %s", patch_path);
  }
  if (!result)
    LogError("Patch does not produce a known translated ROM: %s", patch_path);
  free(patch);
  Rom_Free(base);
  return result;
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
  { ROM_SHA1_REDUX1, ROM_LANG_REDUX, "English Redux - https://www.romhacking.net/translations/6657/", "en-redux" },
  { ROM_SHA1_REDUX2, ROM_LANG_REDUX, "English Redux - https://www.romhacking.net/hacks/2594/", "en-redux" },
  { ROM_SHA1_REDUX3, ROM_LANG_REDUX, "English Redux v10.2.3 - https://www.romhacking.net/hacks/2594/", "en-redux" },
  { ROM_SHA1_REDUX4, ROM_LANG_REDUX, "English Redux v10.2.4 - https://www.romhacking.net/hacks/2594/", "en-redux" },
  { ROM_SHA1_REDUX5, ROM_LANG_REDUX, "English Redux v10.2.3 - https://www.romhacking.net/hacks/2594/", "en-redux" },
  { ROM_SHA1_REDUX6, ROM_LANG_REDUX, "English Retranslation Redux v10.2.3", "en-redux" },
  { ROM_SHA1_REDUX7, ROM_LANG_REDUX, "English Retranslation Redux v10.2.4", "en-redux" },
  { ROM_SHA1_REDUX8, ROM_LANG_REDUX, "English Retranslation 1.01", "en-redux" },
  { ROM_SHA1_REDUX9, ROM_LANG_REDUX, "English Retranslation 1.01 (Translation Quest)", "en-redux" },
  { ROM_SHA1_EN_GN,  ROM_LANG_EN_GN, "English (Gender Neutral)", "en-gn" },
  { ROM_SHA1_NL,     ROM_LANG_NL,    "Dutch - https://www.romhacking.net/translations/1124/", "nl" },
  { ROM_SHA1_SV,     ROM_LANG_SV,    "Swedish - https://www.romhacking.net/translations/982/", "sv" },
  { ROM_SHA1_SV_NOHDR, ROM_LANG_SV,  "Swedish - https://www.romhacking.net/translations/982/", "sv" },
  { ROM_SHA1_RETRANS_KAL, ROM_LANG_RETRANS_KAL, "English Retranslation (Kaleidoscope v1.0) - https://www.romhacking.net/hacks/5526/", "en-kal" },
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
