# Zelda3 Asset Extraction Tool - Technical Guide

## Overview

The Zelda3 Asset Extraction Tool (`zelda3_restool`) is a C-based command-line utility for extracting graphics, music, text, and data from The Legend of Zelda: A Link to the Past ROM files. This document provides comprehensive technical details about SNES ROM formats, asset encoding, and the tool's implementation.

**Why C?** The tool replaces a 7,017-line Python toolchain with a single binary (~5-6K LOC) that has zero runtime dependencies, making distribution and cross-platform support simpler.

**Status:** 165/165 assets complete (100%). All assets byte-perfect verified against Python. **Zero Python dependencies.**

---

## Table of Contents

1. [ROM Format & Structure](#rom-format--structure)
2. [SNES Graphics Format](#snes-graphics-format)
3. [Color & Palette Format](#color--palette-format)
4. [Compression Algorithm](#compression-algorithm)
5. [Usage & Examples](#usage--examples)
6. [Supported ROM Variants](#supported-rom-variants)

---

## ROM Format & Structure

### File Structure

**SNES ROM File Formats:**
- `.sfc` - SNES ROM (no header)
- `.smc` - SNES ROM with SMC header (512-byte header prepended)

The tool auto-detects SMC headers and skips them. ROM files are typically 1MB, 2MB, or 4MB.

### LoROM Memory Mapping

SNES uses **bank-switched addressing** to access ROM data. The tool implements LoROM (Low ROM) mapping:

**Address Format:** 24-bit address written as `$BB:AAAA`
- `BB` = Bank number (0x00-0xFF)
- `AAAA` = Offset within bank (0x0000-0xFFFF)

**LoROM Mapping Rules:**
- Banks 0x00-0x7F map ROM data to addresses 0x8000-0xFFFF (32KB per bank)
- Banks 0x80-0xFF are mirrors of 0x00-0x7F (hardware feature, not used for ROM data)
- Addresses 0x0000-0x7FFF in each bank are **not ROM** (used for RAM/hardware registers)

**Address Conversion Formula:**
```
ROM_offset = (bank × 0x8000) + (offset - 0x8000)
```

**Examples:**
```
SNES $00:8000 → ROM offset 0x000000 (start of ROM)
SNES $00:FFFF → ROM offset 0x007FFF (end of bank 0)
SNES $01:8000 → ROM offset 0x008000 (start of bank 1)
SNES $10:8000 → ROM offset 0x080000 (512KB into ROM)
SNES $20:A000 → ROM offset 0x102000 (1MB + 8KB)
```

**Why Bank Switching?** The 65816 CPU has a 24-bit address space but needs to access more than 16MB of ROM. Bank switching allows efficient access while keeping instruction size small.

### ROM Validation

The tool validates ROMs using SHA-1 hashing to ensure correct version:

**Known Variants:**
- 12 official ROMs and translations supported
- SHA-1 calculated after removing SMC header (if present)
- Validation ensures assets are at expected addresses

**Implementation Detail:** SHA-1 calculation must use a **copy** of ROM data, as the RFC 3174 implementation modifies the input buffer during byte-swapping operations.

---

## SNES Graphics Format

### Tile-Based System

SNES graphics are tile-based:
- All graphics composed of 8×8 pixel tiles
- Tiles stored in **planar format** (not chunky/interleaved)
- Multiple bitplanes combine to form pixel color indices

### Planar Encoding

**What is Planar?** Instead of storing all color bits for each pixel together (chunky), planar format stores one bit from ALL pixels in a row, then the next bit for all pixels, etc.

**Chunky (typical):** `RGBRGBRGBRGB...` (all bits for pixel 0, then pixel 1, etc.)
**Planar (SNES):** `RRRRRRRR GGGGGGGG BBBBBBBB` (all R bits, then all G bits, then all B bits)

### Bit Depth Variants

#### 2bpp (2 bits per pixel, 4 colors)

**Used for:** Background tiles
**Tile size:** 16 bytes per 8×8 tile
**Format:**
- Bitplane 0: 8 bytes (bits 0 of all pixels)
- Bitplane 1: 8 bytes (bits 1 of all pixels)

**Layout:**
```
Bytes 0-7:   Bitplane 0 (rows 0-7, bit 0)
Bytes 8-15:  Bitplane 1 (rows 0-7, bit 1)
```

**Decoding:**
```c
// Each row is 2 bytes (one from each bitplane)
for (row = 0; row < 8; row++) {
  plane0_byte = tile[row];           // Bitplane 0
  plane1_byte = tile[row + 8];       // Bitplane 1

  for (col = 0; col < 8; col++) {
    bit0 = (plane0_byte >> (7 - col)) & 1;  // MSB = leftmost
    bit1 = (plane1_byte >> (7 - col)) & 1;

    pixel_index = bit0 | (bit1 << 1);  // 0-3
    output[row * 8 + col] = pixel_index;
  }
}
```

**Why MSB first?** SNES PPU reads bits left-to-right (MSB to LSB) for each row.

#### 3bpp (3 bits per pixel, 8 colors)

**Used for:** Most enemy sprites
**Tile size:** 24 bytes per 8×8 tile
**Format:**
- Bitplanes 0-1: 16 bytes (like 2bpp)
- Bitplane 2: 8 bytes (bits 2 of all pixels)

**Layout:**
```
Bytes 0-7:   Bitplane 0, rows 0-7
Bytes 8-15:  Bitplane 1, rows 0-7
Bytes 16-23: Bitplane 2, rows 0-7
```

**Decoding:**
```c
for (row = 0; row < 8; row++) {
  plane0 = tile[row * 2 + 0];
  plane1 = tile[row * 2 + 1];
  plane2 = tile[row + 16];

  for (col = 0; col < 8; col++) {
    bit0 = (plane0 >> (7 - col)) & 1;
    bit1 = (plane1 >> (7 - col)) & 1;
    bit2 = (plane2 >> (7 - col)) & 1;

    pixel_index = bit0 | (bit1 << 1) | (bit2 << 2);  // 0-7
  }
}
```

#### 4bpp (4 bits per pixel, 16 colors)

**Used for:** Link sprites, detailed graphics
**Tile size:** 32 bytes per 8×8 tile
**Format:**
- Bitplanes 0-1: 16 bytes
- Bitplanes 2-3: 16 bytes

**Layout:**
```
Bytes 0-1:   Bitplanes 0-1, row 0
Bytes 2-3:   Bitplanes 0-1, row 1
...
Bytes 14-15: Bitplanes 0-1, row 7
Bytes 16-17: Bitplanes 2-3, row 0
Bytes 18-19: Bitplanes 2-3, row 1
...
Bytes 30-31: Bitplanes 2-3, row 7
```

**Decoding:**
```c
for (row = 0; row < 8; row++) {
  plane0 = tile[row * 2 + 0];
  plane1 = tile[row * 2 + 1];
  plane2 = tile[row * 2 + 16];
  plane3 = tile[row * 2 + 17];

  for (col = 0; col < 8; col++) {
    bit0 = (plane0 >> (7 - col)) & 1;
    bit1 = (plane1 >> (7 - col)) & 1;
    bit2 = (plane2 >> (7 - col)) & 1;
    bit3 = (plane3 >> (7 - col)) & 1;

    pixel_index = bit0 | (bit1 << 1) | (bit2 << 2) | (bit3 << 3);  // 0-15
  }
}
```

### Sprite Sheet Layout

Tiles are arranged in sprite sheets:
- **Width:** Typically 128 pixels (16 tiles)
- **Height:** Variable (e.g., Link sprites are 448 pixels = 56 rows)
- **Tile order:** Left-to-right, top-to-bottom

**Example - Link Sprites:**
- 896 total tiles (56 rows × 16 columns)
- 4bpp format (32 bytes per tile)
- Total size: 896 × 32 = 28,672 bytes
- ROM address: `$10:8000` (0x080000)

---

## Color & Palette Format

### SNES BGR555 Format

SNES uses 15-bit color (5 bits per channel) in BGR order:

**Format:** `0BBBBBGGGGGRRRRR` (16-bit little-endian word)
- **Bits 0-4:** Red (5 bits, 0-31)
- **Bits 5-9:** Green (5 bits, 0-31)
- **Bits 10-14:** Blue (5 bits, 0-31)
- **Bit 15:** Unused (always 0)

**Examples:**
```
0x0000 = 0000000000000000 = Black (R=0, G=0, B=0)
0x7FFF = 0111111111111111 = White (R=31, G=31, B=31)
0x001F = 0000000000011111 = Red (R=31, G=0, B=0)
0x03E0 = 0000001111100000 = Green (R=0, G=31, B=0)
0x7C00 = 0111110000000000 = Blue (R=0, G=0, B=31)
```

### Conversion to 8-bit RGB

**Naive approach (lossy):**
```c
r8 = r5 << 3;  // Scales 0-31 to 0-248 (loses precision)
```

**Correct approach (bit replication):**
```c
r8 = (r5 << 3) | (r5 >> 2);  // Scales 0-31 to 0-255 accurately
```

**Why bit replication?**
- Ensures `0x1F` (31/31) maps to `0xFF` (255/255)
- Ensures `0x00` (0/31) maps to `0x00` (0/255)
- Intermediate values scale proportionally
- Formula: Copy top 3 bits to fill bottom 3 bits

**Visual Comparison:**
```
5-bit → Naive → Correct
0x00  → 0x00  → 0x00  ✓ Same
0x10  → 0x80  → 0x84  ✓ Correct fills full range
0x1F  → 0xF8  → 0xFF  ✓ Correct reaches max
```

### Palette Organization

- **Palette size:** Typically 16 colors (4bpp), 8 colors (3bpp), or 4 colors (2bpp)
- **Storage:** Array of 16-bit values in ROM
- **Multiple palettes:** Sprites can use different palettes (e.g., Link's green/blue/red tunics)

**Link Sprite Palette Example:**
```c
uint16_t link_palette[16] = {
  0x0000,  // 0:  Transparent
  0x7fff,  // 1:  White (highlights)
  0x237e,  // 2:  Skin tone
  0x11b7,  // 3:  Dark skin
  0x369e,  // 4:  Green (tunic)
  0x14a5,  // 5:  Dark green
  0x01ff,  // 6:  Yellow (hair)
  0x1078,  // 7:  Dark yellow
  0x599d,  // 8:  Pink (face)
  0x3647,  // 9:  Red (eyes)
  0x3b68,  // 10: Brown
  0x0a4a,  // 11: Dark brown
  0x12ef,  // 12: Blue
  0x2a5c,  // 13: Orange
  0x1571,  // 14: Purple
  0x7a18   // 15: Bright yellow
};
```

---

## Compression Algorithm

### SNES LZ77-Style Compression

The game uses a custom compression algorithm similar to LZ77 with 5 command types.

### Command Format

**Command Byte:** `CCCLLLLL`
- **Bits 5-7 (CCC):** Command type (0-7)
- **Bits 0-4 (LLLLL):** Length parameter (0-31)

**Extended Length:**
- If `LLLLL` is 0, read next byte as 8-bit length (0-255)
- Otherwise, use `LLLLL` directly

### Command Types

#### Type 0 (0x00-0x1F): Direct Copy

**Format:** `00LLLLL [data bytes...]`
**Action:** Copy next N bytes literally to output

**Example:**
```
Input:  0x04 'H' 'e' 'l' 'l' 'o'
Output: "Hello"
```

#### Type 1 (0x20-0x3F): RLE Memset

**Format:** `001LLLLL [byte]`
**Action:** Repeat single byte N times

**Example:**
```
Input:  0x28 0xFF  (length=8, byte=0xFF)
Output: 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
```

#### Type 2 (0x40-0x5F): RLE Memset16

**Format:** `010LLLLL [word_lo] [word_hi]`
**Action:** Repeat 16-bit word (little-endian) N times

**Example:**
```
Input:  0x44 0x12 0x34  (length=4, word=0x3412)
Output: 0x12 0x34 0x12 0x34 0x12 0x34 0x12 0x34
```

#### Type 3 (0x60-0x7F): Incremental Fill

**Format:** `011LLLLL [start]`
**Action:** Fill with incrementing bytes starting from `start`

**Example:**
```
Input:  0x65 0x10  (length=5, start=0x10)
Output: 0x10 0x11 0x12 0x13 0x14
```

#### Types 4-7 (0x80-0xFF): LZ77 Back-Reference

**Format:** `1CCLLLLL [offset_lo] [offset_hi]`
**Action:** Copy N bytes from earlier in output buffer

**Offset Calculation:** Based on command type
- Type 4 (0x80-0x9F): Short back-reference
- Type 5 (0xA0-0xBF): Medium back-reference
- Type 6 (0xC0-0xDF): Long back-reference
- Type 7 (0xE0-0xFF): Very long back-reference

**Example (conceptual):**
```
Output so far: "Hello World"
Command: Copy 5 bytes from offset -6
Result: "Hello WorldWorld" (copied "World")
```

### Decompression Algorithm

```c
DecompressedData* Snes_DecompressBuffer(const uint8_t *src, size_t src_len) {
  DecompressedData *output = AllocateOutputBuffer();
  size_t src_pos = 0;
  size_t dst_pos = 0;

  while (src_pos < src_len) {
    uint8_t cmd = src[src_pos++];
    uint8_t cmd_type = (cmd >> 5) & 0x07;
    uint16_t length = cmd & 0x1F;

    // Extended length
    if (length == 0 && src_pos < src_len) {
      length = src[src_pos++];
    }

    switch (cmd_type) {
      case 0:  // Direct copy
        memcpy(&output->data[dst_pos], &src[src_pos], length);
        src_pos += length;
        dst_pos += length;
        break;

      case 1:  // RLE memset
        memset(&output->data[dst_pos], src[src_pos++], length);
        dst_pos += length;
        break;

      case 2:  // RLE memset16
        uint16_t word = src[src_pos] | (src[src_pos + 1] << 8);
        src_pos += 2;
        for (int i = 0; i < length; i++) {
          output->data[dst_pos++] = word & 0xFF;
          output->data[dst_pos++] = word >> 8;
        }
        break;

      case 3:  // Incremental fill
        uint8_t value = src[src_pos++];
        for (int i = 0; i < length; i++) {
          output->data[dst_pos++] = value++;
        }
        break;

      default:  // LZ77 back-reference
        // Implementation varies by exact type
        break;
    }
  }

  return output;
}
```

---

## Usage & Examples

### Basic Usage

```bash
# Show help
./zelda3_restool --help

# Show version
./zelda3_restool --version

# Extract assets (verbose mode for debugging)
./zelda3_restool --verbose --extract-from-rom zelda3.sfc

# Extract with output directory
./zelda3_restool --extract-from-rom zelda3.sfc --output ./assets
```

### Graphics Extraction Examples

```bash
# Extract Link sprites (4bpp, full color)
./zelda3_restool --extract-from-rom zelda3.sfc --extract-graphics
# Output: linksprite.png (128×448 pixels, 896 tiles)

# Extract enemy sprite tileset 0 (3bpp, grayscale)
./zelda3_restool --extract-from-rom zelda3.sfc --extract-enemy-sheet 0
# Output: enemy_0.png (128×32 pixels, 64 tiles)

# Verbose mode for debugging
./zelda3_restool --verbose --extract-from-rom zelda3.sfc --extract-graphics
```

**Link Sprite Extraction Process:**
1. Load ROM (zelda3.sfc)
2. Validate SHA-1 against known variants
3. Read graphics data at SNES address $10:8000 (ROM offset 0x080000)
4. Decode 896 4bpp tiles (28,672 bytes)
5. Apply Link palette (16 colors, BGR555 format)
6. Convert to 32-bit RGBA
7. Write PNG file via stb_image_write

**Enemy Sprite Extraction Process:**
1. Read tileset from kCompSpritePtrs array addresses
2. Decode 64 3bpp tiles (24 bytes per tile = 1,536 bytes)
3. Apply grayscale palette (8 colors: 0, 36, 72, 108, 144, 180, 216, 252)
4. Convert to 32-bit RGBA
5. Write PNG file (verified identical to Python tool output)

### ROM Validation

```bash
$ ./zelda3_restool --extract-from-rom zelda3.sfc
ROM loaded successfully:
  Size: 1048576 bytes (1.00 MB)
  SMC header: no
  SHA-1: 6d4f10a8b10e10dbe624cb23cf03b88bb8252973
  Version: USA (verified)
```

---

## Supported ROM Variants

The tool recognizes 12 ROM variants by SHA-1 hash:

| Region | Language | SHA-1 Hash | Notes |
|--------|----------|------------|-------|
| USA | English | `6d4f10a8b10e10dbe624cb23cf03b88bb8252973` | Most common |
| Germany | German | `2e62494967fb0afdf5da1635607f9641df7c6559` | PAL version |
| France | French | `229364a1b92a05167cd38609b1aa98f7041987cc` | PAL version |
| Canada | French | `c1c6c7f76fff936c534ff11f87a54162fc0aa100` | NTSC version |
| Europe | English | `7c073a222569b9b8e8ca5fcb5dfec3b5e31da895` | PAL version |
| Fan Translation | Spanish | `461fcbd700d1332009c0e85a7a136e2a8e4b111e` | romhacking.net |
| Fan Translation | Polish | `3c4d605eefda1d76f101965138f238476655b11d` | romhacking.net |
| Fan Translation | Portuguese | `d0d09ed41f9c373fe6afdccafbf0da8c88d3d90d` | romhacking.net |
| Fan Hack | English Redux v1 | `b2a07a59e64c498bc1b2f28728f9bf4014c8d582` | romhacking.net |
| Fan Hack | English Redux v2 | `9325c22eb0a2a1f0017157c8b620bc3a605cede1` | romhacking.net |
| Fan Translation | Dutch | `fa8adfdba2697c9a54d583a1284a22ac764c7637` | romhacking.net |
| Fan Translation | Swedish | `43cd3438469b2c3fe879ea2f410b3ef3cb3f1ca4` | romhacking.net |

**Note:** SHA-1 is calculated **after** removing SMC header (if present).

---

## Implementation Status

### Completed (165/165 assets - 100% ✅)
- ✅ ROM loading with SMC header detection
- ✅ SHA-1 validation for all 12 variants
- ✅ SNES LoROM address translation (including $80-$FF extended banks)
- ✅ SNES decompression (all 5 command types, big-endian support)
- ✅ Graphics decoding (2bpp/3bpp/4bpp) - **verified byte-for-byte vs ROM**
- ✅ Palette conversion (BGR555 → RGBA)
- ✅ PNG output (via lodepng)
- ✅ **YAML infrastructure** - Full YAML parsing with libyaml
- ✅ **Asset compilation** (zelda3_assets.dat) - **165 assets, 100% byte-perfect**
- ✅ **Phase 1: Lookup tables** (875 entries) - Music, sprites, palettes, secrets
- ✅ **Phase 2: Map32toMap16** - 1875 entries, 3-layer unpacking, byte-perfect
- ✅ **Phase 3: Link graphics** - 4bpp decoding with lodepng, palette application
- ✅ **Phase 4: Dungeon sprites** - 71 compressed tilesets, byte-perfect
- ✅ **Phase 5: ROM-based assets** - 32 assets (palettes, tilemaps, misc data)
- ✅ **Phase 6: Compressed data** - kOverworld_Hibytes_Comp + Lobytes_Comp (2 packed assets)
- ✅ **Phase 7: Dungeon YAML** (42 assets)
  - kDungeonSecrets (1 asset, byte-perfect)
  - Simple dungeon data (3 assets: TeleMsg, Pits, Chests)
  - Room headers (2 assets: byte-perfect)
  - Room data (3 assets: 3-layer encoding, byte-perfect)
  - Default/Overlay rooms (4 assets: byte-perfect)
  - Entrances & Starting Points (33 assets: indexed extraction, byte-perfect)
- ✅ **Phase 8: Overworld YAML** (48 assets)
  - Header data (6 assets: byte-perfect)
  - Travel data (9 assets: bird travel + whirlpools)
  - Entrances (3 assets: byte-perfect)
  - Holes (3 assets: sorted extraction)
  - Exits (22 assets: regular + special exits)
  - Secrets (2 assets: byte-perfect)
  - Sprites (4 assets: compressed sprite data)
  - ROM-based (2 assets: auxiliary data)
- ✅ **Phase 9: Sound banks** (3 assets) - Intro/indoor/ending music
- ✅ **Phase 10: Dialogue** (3 assets) - Pure C text compression, **zero Python dependency**
  - Font PNG extraction (lodepng, 2bpp encoding)
  - US text compression (alphabet + 87-entry dictionary)
  - Greedy string matching with command encoding
  - Double-packed format (language wrapper)

### Language Support
- **Dialogue:** US language only (can be extended to DE/FR/EN/ES by porting additional language data)
- **Other assets:** Language-agnostic (work with all ROM variants)

### Verification
- All completed assets verified byte-perfect against Python reference
- Comprehensive test scripts for dungeon rooms, entrances, starting points
- MD5 hashing confirms exact match with Python output

---

## References

### SNES Development
- [SNES Dev Wiki](https://snes.nesdev.org/)
- [SNES Memory Map](https://snes.nesdev.org/wiki/Memory_map)
- [SNES Graphics Guide](https://megacatstudios.com/blogs/retro-development/snes-graphics-guide)

### A Link to the Past
- [Disassembly Project](https://github.com/kan-dash/zelda3)
- [ROM Hacking Resources](https://www.romhacking.net/games/221/)
- [Zelda3 C Port](https://github.com/snesrev/zelda3)

### Tools & Libraries
- [stb Libraries](https://github.com/nothings/stb) - Single-header public domain libraries
- [Zelda3 Python Tools](../assets/) - Original Python extraction toolchain

---

**Document Version:** 1.0
**Last Updated:** 2025-11-23
**Maintained by:** zelda3 contributors
