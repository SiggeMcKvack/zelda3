# Zelda3 Asset Extraction Tool (restool)

Developer documentation for the C-based asset extraction tool.

## Overview

`zelda3_restool` is a standalone command-line tool for extracting assets from The Legend of Zelda: A Link to the Past ROM files. It replaces the Python-based extraction toolchain with a single binary that has zero runtime dependencies.

**Key Features:**
- ROM loading with SMC header auto-detection
- Support for 12 known ROM variants (USA, Germany, France, etc.)
- SNES LoROM bank-switched address translation
- SNES compression/decompression
- Graphics extraction (2bpp/3bpp/4bpp planar tiles)
- Palette conversion (15-bit BGR555 → 32-bit RGBA)
- PNG output via stb_image_write

**Status:** 100% complete - All 165 assets extracted, byte-perfect verified. Zero Python dependencies.

## Usage

```bash
# Build the tool
mkdir build && cd build
cmake .. && cmake --build .

# Extract Link sprites (4bpp, 128x448px)
./src/restool/zelda3_restool --extract-from-rom zelda3.sfc --extract-graphics

# Extract enemy sprite tileset 0 (3bpp, 128x32px)
./src/restool/zelda3_restool --extract-from-rom zelda3.sfc --extract-enemy-sheet 0

# Show help
./src/restool/zelda3_restool --help
```

**Supported Features:**
- `--extract-graphics` - Extract Link sprites to `linksprite.png`
- `--extract-enemy-sheet <N>` - Extract enemy tileset N to `enemy_N.png` (N=0-11)
- ROM validation against 12 known variants (USA, Germany, France, etc.)

## Architecture

### Module Structure

```
src/restool/
├── main.c              # CLI entry point, argument parsing, command dispatch
├── types.h             # Common structures (Rom, TileData, Color, etc.)
├── restool_util.h/c    # ROM loading, SNES addressing, compression
├── graphics.h/c        # Tile decoding, palette conversion, PNG output
└── CMakeLists.txt      # Build configuration
```

### Dependencies

**Vendored (third_party/):**
- `stb_image_write.h` - PNG encoding (public domain, single-header)
- `sha256.h/c` - SHA-256 hashing for asset integrity

**Shared from main codebase (src/):**
- `platform.h/c` - Cross-platform file I/O abstraction
- `logging.h/c` - Unified logging system

**System Libraries:**
- Standard C library (`stdc`)
- Math library (`libm`) - for floating point operations

**No External Dependencies:**
- No SDL2 (unlike main game)
- No Python runtime (pure C implementation)
- No image processing libraries (PNG handled by lodepng)
- No text processing libraries (compression in pure C)

### Build Integration

The tool is integrated into the main CMake build system as an optional target:

```cmake
option(BUILD_RESTOOL "Build asset extraction tool" ON)

if(BUILD_RESTOOL)
    add_subdirectory(src/restool)
endif()
```

**Building:**
```bash
mkdir build && cd build
cmake .. -DBUILD_RESTOOL=ON  # ON by default
cmake --build . --target zelda3_restool
```

The resulting binary is self-contained and can be distributed separately from the main game.

## Key Concepts

### SNES LoROM Addressing

The tool implements SNES LoROM bank-switched memory mapping:

**Address Format:** `$BB:AAAA` (24-bit, bank:offset notation)
- Bank (BB): 0x00-0xFF
- Offset (AAAA): 0x0000-0xFFFF

**LoROM Mapping:**
- Banks 0x00-0x7F: ROM data mapped to 0x8000-0xFFFF (32KB per bank)
- Banks 0x80-0xFF: Mirror of 0x00-0x7F (not used for ROM data)

**Conversion Formula:**
```c
uint32_t SnesAddrToRomOffset(uint32_t snes_addr) {
  uint8_t bank = (snes_addr >> 16) & 0xFF;
  uint16_t offset = snes_addr & 0xFFFF;

  // Only addresses 0x8000-0xFFFF map to ROM
  if (offset < 0x8000) return INVALID;

  return (bank * 0x8000) + (offset - 0x8000);
}
```

**Example:**
- SNES address `$10:8000` → ROM offset `0x80000` (512KB)
- SNES address `$00:8000` → ROM offset `0x00000` (start of ROM)

### ROM SHA-1 Validation

The tool validates ROMs against known SHA-1 hashes to ensure correct version:

**Supported Variants:**
- USA (English)
- Germany (German)
- France (French)
- Canada (French Canadian)
- Europe (English PAL)
- Spanish translation
- Polish translation
- Portuguese translation
- English Redux (2 versions)
- Dutch translation
- Swedish translation

**Implementation Note:**
SHA-1 calculation requires hashing a **copy** of the ROM data, as the implementation modifies the input buffer during byte-swapping. See `Rom_Load()` in `restool_util.c` for details.

### SNES Graphics Format

#### Planar Tile Encoding

SNES graphics use **planar** encoding instead of chunky (interleaved) pixels:

**2bpp (4 colors, 16 bytes per 8x8 tile):**
- Bitplane 0: 8 bytes (bit 0 of each pixel)
- Bitplane 1: 8 bytes (bit 1 of each pixel)

**3bpp (8 colors, 24 bytes per 8x8 tile):**
- Bitplanes 0-1: 16 bytes (like 2bpp)
- Bitplane 2: 8 bytes (bit 2 of each pixel)

**4bpp (16 colors, 32 bytes per 8x8 tile):**
- Bitplanes 0-1: 16 bytes
- Bitplanes 2-3: 16 bytes

**Bitplane Layout:**
Each row of a tile spans 2 bytes in bitplanes 0-1, with pixels stored MSB-first:
```
Row data:    [Plane0 byte] [Plane1 byte]
Bit 7 = leftmost pixel (x=0)
Bit 0 = rightmost pixel (x=7)
```

**Decoding Process:**
```c
void DecodeTile4bpp(const uint8_t *src, uint8_t *dst) {
  for (int y = 0; y < 8; y++) {
    uint8_t d0 = src[y * 2 + 0];   // Bitplane 0
    uint8_t d1 = src[y * 2 + 1];   // Bitplane 1
    uint8_t d2 = src[y * 2 + 16];  // Bitplane 2
    uint8_t d3 = src[y * 2 + 17];  // Bitplane 3

    for (int x = 0; x < 8; x++) {
      // Extract bit from each plane (MSB first)
      uint8_t bit0 = (d0 >> (7 - x)) & 1;
      uint8_t bit1 = (d1 >> (7 - x)) & 1;
      uint8_t bit2 = (d2 >> (7 - x)) & 1;
      uint8_t bit3 = (d3 >> (7 - x)) & 1;

      // Combine into palette index
      dst[y * 8 + x] = bit0 | (bit1 << 1) | (bit2 << 2) | (bit3 << 3);
    }
  }
}
```

#### SNES Color Format (BGR555)

SNES palettes use 15-bit color (5 bits per channel) in BGR order:

**Format:** `0BBBBBGGGGGRRRRR` (stored as little-endian 16-bit word)
- Bits 0-4: Red (5 bits)
- Bits 5-9: Green (5 bits)
- Bits 10-14: Blue (5 bits)
- Bit 15: Unused (always 0)

**Conversion to 8-bit RGB:**
```c
Color SnesColorToRGBA(uint16_t snes_color) {
  uint8_t r5 = (snes_color >> 0) & 0x1F;
  uint8_t g5 = (snes_color >> 5) & 0x1F;
  uint8_t b5 = (snes_color >> 10) & 0x1F;

  // Expand 5-bit to 8-bit by replicating upper bits
  // This ensures 0x1F (31) → 0xFF (255)
  Color c;
  c.r = (r5 << 3) | (r5 >> 2);
  c.g = (g5 << 3) | (g5 >> 2);
  c.b = (b5 << 3) | (b5 >> 2);
  c.a = 255;

  return c;
}
```

**Why bit replication?** Simply left-shifting loses precision. Replicating the upper bits ensures:
- `0x00` (0/31) → `0x00` (0/255)
- `0x1F` (31/31) → `0xFF` (255/255)
- Intermediate values scale proportionally

### SNES Compression

The tool implements SNES LZ77-style compression with 5 command types:

**Command Format:** `CCCLLLLL` (1 byte)
- `CCC`: Command type (3 bits, 0-7)
- `LLLLL`: Length (5 bits, 0-31)
- If length is 0, next byte contains extended length (up to 255)

**Command Types:**
- **Type 0 (0x00-0x1F):** Direct copy - Copy next N bytes literally
- **Type 1 (0x20-0x3F):** RLE memset - Repeat single byte N times
- **Type 2 (0x40-0x5F):** RLE memset16 - Repeat 16-bit word N times
- **Type 3 (0x60-0x7F):** Incremental fill - Fill with incrementing values
- **Type 4+ (0x80-0xFF):** LZ77 back-reference - Copy from earlier in output buffer

**Status:** Decompression complete, compression deferred (not needed for extraction).

## Code Conventions

### Naming

**Functions:**
- Module prefix: `Rom_Load()`, `Snes_Decompress()`, `SnesColorToRGBA()`
- Internal/static functions: `snake_case` without prefix

**Structures:**
- PascalCase: `Rom`, `TileData`, `DecompressedData`, `Color`
- Always typedef'd

**Variables:**
- `snake_case` for locals and parameters
- Global constants: `kConstantName` (e.g., `kROM_SHA1_USA`)

**Macros:**
- UPPER_SNAKE_CASE for constants
- PascalCase for function-like macros

### Memory Management

**Ownership Rules:**
- Functions returning pointers transfer ownership to caller
- Caller must free using corresponding `Free` function
- Example: `Rom_Load()` returns `Rom*`, caller must call `Rom_Free()`

**Resource Cleanup:**
```c
Rom *rom = Rom_Load("zelda3.sfc");
if (!rom) {
  // Error handling
  return 1;
}

// Use ROM...

Rom_Free(rom);  // Always clean up
```

### Error Handling

**Return Values:**
- Pointers: `NULL` on error
- Integers: Negative on error, 0 for success
- Booleans: `false` on error

**Logging:**
```c
if (!rom->data) {
  LogError("Failed to read ROM file: %s", path);
  return NULL;
}
```

Use `LogError()`, `LogWarn()`, `LogInfo()`, `LogDebug()` from `logging.h`.

## Adding New Features

### Adding a New Extraction Type

1. **Define structures** in `types.h`:
```c
typedef struct {
  uint8_t *data;
  size_t size;
  // ... other fields
} MyAssetType;
```

2. **Implement extraction** in new module or existing file:
```c
MyAssetType* ExtractMyAsset(Rom *rom, uint32_t snes_addr);
void FreeMyAsset(MyAssetType *asset);
```

3. **Add CLI option** in `main.c`:
```c
else if (strcmp(argv[i], "--extract-my-asset") == 0) {
  args->extract_my_asset = true;
}
```

4. **Implement extraction logic** in command handler.

### Adding ROM Variant Support

Add SHA-1 hash to `types.h`:
```c
#define ROM_SHA1_NEW "abcdef1234567890..."  // New ROM variant
```

Update validation logic in `main.c` to check new hash and display appropriate version string.

## Testing

**Current Test Coverage:**
- ROM loading with SMC header detection
- SHA-1 validation against known ROMs
- SNES address conversion (LoROM)
- Byte/Word/Addr reading from ROM
- SNES decompression (all 5 command types)
- Tile decoding (2bpp/3bpp/4bpp)
- Palette conversion
- PNG output
- Link sprite extraction (896 tiles, 4bpp, 128x448px)

**Manual Testing:**
```bash
./zelda3_restool --help
./zelda3_restool --version
./zelda3_restool --verbose --extract-from-rom zelda3.sfc
```

**Verification:**
Compare output PNGs with Python tool results for pixel-perfect accuracy.

## Completed Features

**All Phases Complete:**
- ✅ Phase 1-7: All asset extraction (165/165 assets)
- ✅ Phase 8: Asset compilation to zelda3_assets.dat (byte-perfect)
- ✅ Phase 9: Full CLI integration
- ✅ Phase 10: Pure C dialogue extraction (zero Python dependency)

**Byte-Perfect Verification:**
- 100% match with Python reference implementation
- 679,700 bytes total asset data
- 668KB zelda3_assets.dat file
- Zero differences via binary comparison

## Potential Future Enhancements

**Multi-Language Support:**
- Current: US language dialogue only
- Potential: Add DE/FR/EN/ES language support (~200 lines per language)
- Requires: Porting alphabet/dictionary/encoder data structures from Python

**Compression:**
- Current: Decompression only
- Potential: Add SNES compression for asset modification tools

See `tools/RESTOOL_MIGRATION.md` for migration history.

## References

- Python toolchain: `assets/` directory
- SNES dev wiki: https://snes.nesdev.org/wiki/
- LoROM mapping: https://snes.nesdev.org/wiki/Memory_map
- ALTTP disassembly: https://github.com/kan-dash/zelda3
