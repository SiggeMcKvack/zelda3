// rom_sha1.h - Shared ROM SHA1 validation for launcher and restool
#ifndef ROM_SHA1_H
#define ROM_SHA1_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Known ROM SHA1 hashes
#define ROM_SHA1_USA    "6d4f10a8b10e10dbe624cb23cf03b88bb8252973"
#define ROM_SHA1_DE     "2e62494967fb0afdf5da1635607f9641df7c6559"
#define ROM_SHA1_FR     "229364a1b92a05167cd38609b1aa98f7041987cc"
#define ROM_SHA1_FR_C   "c1c6c7f76fff936c534ff11f87a54162fc0aa100"
#define ROM_SHA1_EN     "7c073a222569b9b8e8ca5fcb5dfec3b5e31da895"
#define ROM_SHA1_ES     "461fcbd700d1332009c0e85a7a136e2a8e4b111e"
#define ROM_SHA1_PL     "3c4d605eefda1d76f101965138f238476655b11d"
#define ROM_SHA1_PT     "d0d09ed41f9c373fe6afdccafbf0da8c88d3d90d"
#define ROM_SHA1_REDUX1 "b2a07a59e64c498bc1b2f28728f9bf4014c8d582"
#define ROM_SHA1_REDUX2 "9325c22eb0a2a1f0017157c8b620bc3a605cede1"
#define ROM_SHA1_REDUX3 "0db9eb8af889bb07b8172b5b034ffc971540ada0"
#define ROM_SHA1_REDUX4 "92db633bc1fad0d865794eb437ebc81ec076fdc7"
#define ROM_SHA1_REDUX5 "d9786698ea43e5688aeeaaec92502a4bf16c3eb5"
#define ROM_SHA1_NL     "fa8adfdba2697c9a54d583a1284a22ac764c7637"
#define ROM_SHA1_SV     "43cd3438469b2c3fe879ea2f410b3ef3cb3f1ca4"
#define ROM_SHA1_RETRANS_KAL "ebf8d301ffaf5d412a8c3c832c0f0a92cfa2f16b"

// ROM identification result
typedef struct {
  char sha1[41];        // Hex string (40 chars + null)
  char lang_code[16];   // Language code (us, de, fr, etc.)
  char lang_name[64];   // Display name (USA, German, French, etc.)
  bool valid;           // True if SHA1 matches a known ROM
} RomIdentification;

// Calculate SHA1 hash of data buffer
// out_sha1 must be at least 41 bytes
void RomSha1_Calculate(const uint8_t *data, size_t size, char *out_sha1);

// Identify ROM language from SHA1 hash
// Returns true if SHA1 matches a known ROM
bool RomSha1_Identify(const char *sha1, char *out_code, char *out_name);

// Validate ROM file and identify its language
// Handles SMC header detection and stripping
// Returns true if file was read and identified (even if unknown ROM)
bool RomSha1_ValidateFile(const char *path, RomIdentification *out_id);

#endif // ROM_SHA1_H
