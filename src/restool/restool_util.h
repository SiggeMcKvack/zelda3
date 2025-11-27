// restool_util.h - Core utilities for ROM reading and SNES compression
#ifndef RESTOOL_UTIL_H
#define RESTOOL_UTIL_H

#include "types.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ============================================================================
// ROM Reading Functions
// ============================================================================

// Load ROM from file
// Automatically detects and handles SMC header (512-byte offset)
// Returns NULL on error
Rom* Rom_Load(const char *path);

// Free ROM memory
void Rom_Free(Rom *rom);

// Read single byte from SNES address (with bank switching)
uint8_t Rom_ReadByte(Rom *rom, uint32_t snes_addr);

// Read 16-bit word from SNES address (little-endian)
uint16_t Rom_ReadWord(Rom *rom, uint32_t snes_addr);

// Read 24-bit address from SNES address (little-endian)
uint32_t Rom_ReadAddr(Rom *rom, uint32_t snes_addr);

// Get pointer to data at SNES address
// Returns NULL if address is invalid or length exceeds bounds
uint8_t* Rom_ReadPtr(Rom *rom, uint32_t snes_addr, size_t len);

// Validate ROM SHA1 hash
bool Rom_ValidateSHA1(Rom *rom, const char *expected_sha1);

// Identify ROM language from SHA1 hash
// Sets rom->language and rom->language_name
void Rom_IdentifyLanguage(Rom *rom);

// Get language code string (e.g., "us", "en", "de")
const char* Rom_GetLanguageCode(RomLanguage lang);

// Convert SNES address to ROM offset (LoROM mapping)
// Returns 0xFFFFFFFF if address is invalid
uint32_t SnesAddrToRomOffset(uint32_t snes_addr);

// ============================================================================
// SNES Compression/Decompression
// ============================================================================

// Decompress SNES data starting at SNES address
// big_endian_offsets: true for most data (default SNES), false for sprite/bg gfx
// Returns NULL on error
DecompressedData* Snes_Decompress(Rom *rom, uint32_t snes_addr, bool big_endian_offsets);

// Decompress SNES data from buffer
DecompressedData* Snes_DecompressBuffer(const uint8_t *src, size_t src_len, bool big_endian_offsets);

// Free decompressed data
void Snes_FreeDecompressed(DecompressedData *data);

// Compress data using SNES compression
// Returns NULL on error
CompressedData* Snes_Compress(const uint8_t *src, size_t src_len);

// Free compressed data
void Snes_FreeCompressed(CompressedData *data);

// ============================================================================
// Utility Functions
// ============================================================================

// SHA-1 functions are now provided by shared rom_sha1.h
// Use RomSha1_Calculate() instead of CalculateSHA1()

#endif // RESTOOL_UTIL_H
