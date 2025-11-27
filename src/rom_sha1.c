// rom_sha1.c - Shared ROM SHA1 validation implementation
#include "rom_sha1.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Windows compatibility for POSIX functions
#ifdef _WIN32
  #define strcasecmp _stricmp
#else
  #include <strings.h>
#endif

// ============================================================================
// SHA-1 Implementation (RFC 3174)
// ============================================================================

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

// ============================================================================
// Public API
// ============================================================================

void RomSha1_Calculate(const uint8_t *data, size_t size, char *out_sha1) {
  SHA1_CTX ctx;
  uint8_t hash[20];

  SHA1_Init(&ctx);
  SHA1_Update(&ctx, data, size);
  SHA1_Final(hash, &ctx);

  for (int i = 0; i < 20; i++) {
    sprintf(out_sha1 + i*2, "%02x", hash[i]);
  }
  out_sha1[40] = '\0';
}

// ROM language lookup table
static const struct {
  const char *sha1;
  const char *code;
  const char *name;
} kRomHashes[] = {
  { ROM_SHA1_USA,    "us",    "USA" },
  { ROM_SHA1_DE,     "de",    "German" },
  { ROM_SHA1_FR,     "fr",    "French" },
  { ROM_SHA1_FR_C,   "fr-c",  "French (Canada)" },
  { ROM_SHA1_EN,     "en",    "European English" },
  { ROM_SHA1_ES,     "es",    "Spanish" },
  { ROM_SHA1_PL,     "pl",    "Polish" },
  { ROM_SHA1_PT,     "pt",    "Portuguese" },
  { ROM_SHA1_REDUX1, "redux", "English Redux" },
  { ROM_SHA1_REDUX2, "redux", "English Redux" },
  { ROM_SHA1_REDUX3, "redux", "English Redux v10.2.3" },
  { ROM_SHA1_REDUX4, "redux", "English Redux v10.2.4" },
  { ROM_SHA1_REDUX5, "redux", "English Redux v10.2.3" },
  { ROM_SHA1_NL,     "nl",    "Dutch" },
  { ROM_SHA1_SV,     "sv",    "Swedish" },
  { ROM_SHA1_RETRANS_KAL, "retrans-kal", "English (Kaleidoscope)" },
  { NULL, NULL, NULL }
};

bool RomSha1_Identify(const char *sha1, char *out_code, char *out_name) {
  for (int i = 0; kRomHashes[i].sha1 != NULL; i++) {
    if (strcasecmp(sha1, kRomHashes[i].sha1) == 0) {
      if (out_code) {
        strncpy(out_code, kRomHashes[i].code, 15);
        out_code[15] = '\0';
      }
      if (out_name) {
        strncpy(out_name, kRomHashes[i].name, 63);
        out_name[63] = '\0';
      }
      return true;
    }
  }

  // Unknown ROM
  if (out_code) strcpy(out_code, "unknown");
  if (out_name) strcpy(out_name, "Unknown ROM");
  return false;
}

bool RomSha1_ValidateFile(const char *path, RomIdentification *out_id) {
  if (!out_id) return false;

  memset(out_id, 0, sizeof(*out_id));
  strcpy(out_id->lang_code, "unknown");
  strcpy(out_id->lang_name, "Unknown ROM");
  out_id->valid = false;

  // Read ROM file
  size_t size;
  uint8_t *data = Platform_ReadWholeFile(path, &size);
  if (!data) {
    strcpy(out_id->lang_name, "Failed to read");
    return false;
  }

  // Check for SMC header (512 bytes)
  uint8_t *rom_data = data;
  size_t rom_size = size;
  if (size % 1024 == 512) {
    rom_data = data + 512;
    rom_size = size - 512;
  }

  // Calculate SHA1
  RomSha1_Calculate(rom_data, rom_size, out_id->sha1);

  // Identify language
  out_id->valid = RomSha1_Identify(out_id->sha1, out_id->lang_code, out_id->lang_name);

  free(data);
  return true;
}
