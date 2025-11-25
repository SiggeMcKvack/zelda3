# C Restool Usage Guide

This guide documents the C-based asset extraction tool (`zelda3_restool`) for Zelda3. This tool extracts game assets from The Legend of Zelda: A Link to the Past ROM files and compiles them into `zelda3_assets.dat`, which is required to run the zelda3 engine.

## Table of Contents

- [Quick Start](#quick-start)
- [Command-Line Parameters](#command-line-parameters)
- [Supported ROM Versions](#supported-rom-versions)
- [Common Workflows](#common-workflows)
- [Output Files](#output-files)
- [Multi-Language Support](#multi-language-support)
- [Troubleshooting](#troubleshooting)
- [Technical Details](#technical-details)

## Quick Start

The most common use case - extracting assets from a US ROM:

```bash
# 1. Build the tool (if not already built)
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)

# 2. Place your ROM in the project root
cp /path/to/your/rom.sfc ../zelda3.sfc

# 3. Extract assets and compile
./src/restool/zelda3_restool --extract-from-rom ../zelda3.sfc --compile

# 4. Verify output
ls -lh ../zelda3_assets.dat  # Should be ~680KB
```

## Command-Line Parameters

### `--extract-from-rom <PATH>`

Extract all game assets from ROM and optionally compile them into `zelda3_assets.dat`.

**Type:** Required string (ROM path)

```bash
# Extract assets
./zelda3_restool --extract-from-rom zelda3.sfc

# Extract and compile
./zelda3_restool --extract-from-rom zelda3.sfc --compile
```

### `--compile`

Compile extracted assets into `zelda3_assets.dat`.

**Type:** Boolean flag
**Default:** False

```bash
# Extract and compile in one step
./zelda3_restool --extract-from-rom zelda3.sfc --compile

# Or compile after extraction
./zelda3_restool --compile
```

### `--output <PATH>`

Specify output directory for extracted assets.

**Type:** Optional string
**Default:** `assets/` directory

```bash
./zelda3_restool --extract-from-rom zelda3.sfc --output ./my_assets
```

### `--extract-dialogue`

Extract dialogue strings from ROM to a language-specific text file.

**Type:** Boolean flag
**Default:** False

Output files created in `assets/`:
- `dialogue.txt` (US)
- `dialogue_de.txt` (German)
- `dialogue_fr.txt` (French)
- `dialogue_fr_c.txt` (French Canadian)
- `dialogue_en.txt` (European English)
- `dialogue_es.txt` (Spanish)
- `dialogue_pl.txt` (Polish)
- `dialogue_pt.txt` (Portuguese)
- `dialogue_nl.txt` (Dutch)
- `dialogue_sv.txt` (Swedish)

```bash
# Extract German dialogue
./zelda3_restool --extract-from-rom german.sfc --extract-dialogue

# Extract Spanish dialogue
./zelda3_restool --extract-from-rom spanish.sfc --extract-dialogue
```

### `--extract-graphics`

Extract Link sprite graphics only.

**Type:** Boolean flag
**Default:** False

```bash
./zelda3_restool --extract-from-rom zelda3.sfc --extract-graphics
# Output: linksprite.png (128×448 pixels)
```

### `--extract-enemy-sheet <N>`

Extract a specific enemy sprite tileset.

**Type:** Integer (tileset index 0-70)

```bash
./zelda3_restool --extract-from-rom zelda3.sfc --extract-enemy-sheet 0
# Output: enemy_0.png (128×32 pixels)
```

### `--extract-overworld`

Extract overworld data only.

**Type:** Boolean flag
**Default:** False

```bash
./zelda3_restool --extract-from-rom zelda3.sfc --extract-overworld
```

### `--verbose`

Enable verbose output for debugging.

**Type:** Boolean flag
**Default:** False

```bash
./zelda3_restool --verbose --extract-from-rom zelda3.sfc
```

### `--help`

Display help message with all available options.

```bash
./zelda3_restool --help
```

### `--version`

Display version information.

```bash
./zelda3_restool --version
```

## Supported ROM Versions

The tool validates ROMs using SHA1 hashing. Only these versions are supported:

### Official Releases

| Language | Region | SHA1 Hash |
|----------|--------|-----------|
| English (US) | USA | `6d4f10a8b10e10dbe624cb23cf03b88bb8252973` |
| German | Germany | `2e62494967fb0afdf5da1635607f9641df7c6559` |
| French | France | `229364a1b92a05167cd38609b1aa98f7041987cc` |
| French | Canada | `c1c6c7f76fff936c534ff11f87a54162fc0aa100` |
| English | Europe | `7c073a222569b9b8e8ca5fcb5dfec3b5e31da895` |

### Fan Translations

| Language | Code | SHA1 Hash | Source |
|----------|------|-----------|--------|
| Spanish | `es` | `461fcbd700d1332009c0e85a7a136e2a8e4b111e` | [romhacking.net/2195](https://www.romhacking.net/translations/2195/) |
| Polish | `pl` | `3c4d605eefda1d76f101965138f238476655b11d` | [romhacking.net/5760](https://www.romhacking.net/translations/5760/) |
| Portuguese | `pt` | `d0d09ed41f9c373fe6afdccafbf0da8c88d3d90d` | [romhacking.net/6530](https://www.romhacking.net/translations/6530/) |
| Dutch | `nl` | `fa8adfdba2697c9a54d583a1284a22ac764c7637` | [romhacking.net/1124](https://www.romhacking.net/translations/1124/) |
| Swedish | `sv` | `43cd3438469b2c3fe879ea2f410b3ef3cb3f1ca4` | [romhacking.net/982](https://www.romhacking.net/translations/982/) |
| English Redux (v1) | `redux` | `b2a07a59e64c498bc1b2f28728f9bf4014c8d582` | [romhacking.net/6657](https://www.romhacking.net/translations/6657/) |
| English Redux (v2) | `redux` | `9325c22eb0a2a1f0017157c8b620bc3a605cede1` | [romhacking.net/hacks/2594](https://www.romhacking.net/hacks/2594/) |

### Verify Your ROM

```bash
# Linux/macOS
sha1sum zelda3.sfc

# Windows
certutil -hashfile zelda3.sfc SHA1
```

**SHA256 for US ROM:** `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`

## Common Workflows

### Workflow 1: Standard Asset Extraction (US ROM)

```bash
# Place ROM in project root
cp /path/to/zelda3.sfc .

# Extract and compile
./build/src/restool/zelda3_restool --extract-from-rom zelda3.sfc --compile

# Output: zelda3_assets.dat (~680KB)
```

### Workflow 2: Extract Dialogue from Translated ROM

```bash
# Extract German dialogue
./zelda3_restool --extract-from-rom german.sfc --extract-dialogue

# Creates: assets/dialogue_de.txt
```

### Workflow 3: Inspect Extracted Data

```bash
# Extract without compiling
./zelda3_restool --extract-from-rom zelda3.sfc

# Examine YAML files
cat assets/dungeon/dungeon-0.yaml
cat assets/overworld/overworld-0.yaml

# View graphics
open assets/linksprite.png  # macOS
xdg-open assets/linksprite.png  # Linux

# Check dialogue
head -n 20 assets/dialogue.txt
```

### Workflow 4: Extract Specific Assets

```bash
# Extract only Link sprites
./zelda3_restool --extract-from-rom zelda3.sfc --extract-graphics

# Extract specific enemy tileset
./zelda3_restool --extract-from-rom zelda3.sfc --extract-enemy-sheet 5

# Extract overworld data only
./zelda3_restool --extract-from-rom zelda3.sfc --extract-overworld
```

### Workflow 5: Verbose Debugging

```bash
./zelda3_restool --verbose --extract-from-rom zelda3.sfc --compile
```

## Output Files

### Final Output (Parent Directory)

```
zelda3/
└── zelda3_assets.dat          # ~680KB binary asset file
```

### Intermediate Files (Created by `--extract-from-rom`)

```
assets/
├── dungeon/
│   ├── dungeon-0.yaml         # Room 0 data
│   ├── dungeon-1.yaml         # Room 1 data
│   ├── ...                    # Rooms 2-318
│   ├── dungeon-319.yaml       # Room 319 data
│   ├── default_rooms.yaml     # 8 default room templates
│   └── overlay_rooms.yaml     # 19 overlay templates
│
├── overworld/
│   ├── overworld-0.yaml       # Area 0 (Light world 0,0)
│   ├── overworld-1.yaml       # Area 1
│   ├── ...                    # Areas 2-158
│   └── overworld-159.yaml     # Area 159
│
├── linksprite.png             # Link sprites (128×448 pixels)
├── font.png                   # Font glyphs
├── dialogue.txt               # US dialogue (397 strings)
├── dialogue_de.txt            # German dialogue (if extracted)
├── dialogue_fr.txt            # French dialogue (if extracted)
│   ... (other dialogue files)
│
├── map32_to_map16.txt         # Tile conversion table
├── music_info.yaml            # Music metadata
├── sfx.txt                    # Sound effects data
├── sound_intro.txt            # Intro music data
├── sound_indoor.txt           # Indoor music data
└── sound_ending.txt           # Ending music data
```

### zelda3_assets.dat Structure

Binary file containing **165 assets**:

- **Dialogue assets (3):** Packed dialogue strings, fonts, language mapping
- **Graphics (5):** 108 sprite sheets, backgrounds, Link sprites, map graphics
- **Map data (7):** Overworld and map conversion tables
- **Overworld tables (24):** Bird travel, entrances, exits, secrets, sprites
- **Dungeon data (20):** Room layouts, sprites, chests, doors
- **Entrance data (26):** Dungeon entrances, starting points
- **Dungeon maps (2):** Floor layouts, map tiles
- **Palettes (15):** All color palettes
- **Miscellaneous (25):** Credits, ending sequences, tile attributes
- **Music (3):** Intro, indoor, ending sound banks
- **Enemy data (1):** Damage tables
- **Tilemaps (6):** Background tilemaps

Total size: ~680KB (679,700 bytes)

## Multi-Language Support

### Supported Languages

All 11 languages produce byte-perfect output matching the Python tool:

| Language | Code | Alphabet | Dictionary |
|----------|------|----------|------------|
| US English | us | 95 chars | 87 entries |
| Europe English | en | 112 chars | 87 entries |
| German | de | 112 chars | 112 entries |
| French | fr | 112 chars | 99 entries |
| Canadian French | fr-c | 112 chars | 99 entries |
| Swedish | sv | 99 chars | 97 entries |
| Polish | pl | 99 chars | 97 entries |
| Portuguese | pt | 121 chars | 97 entries |
| Spanish | es | 99 chars | 97 entries |
| Dutch | nl | 94 chars | 97 entries |
| Redux | redux | (uses US) | (uses US) |

### Extract Dialogue for Each Language

```bash
# Extract from each language ROM
./zelda3_restool --extract-from-rom usa.sfc --extract-dialogue
./zelda3_restool --extract-from-rom german.sfc --extract-dialogue
./zelda3_restool --extract-from-rom french.sfc --extract-dialogue
./zelda3_restool --extract-from-rom spanish.sfc --extract-dialogue
./zelda3_restool --extract-from-rom polish.sfc --extract-dialogue
./zelda3_restool --extract-from-rom portuguese.sfc --extract-dialogue
./zelda3_restool --extract-from-rom dutch.sfc --extract-dialogue
./zelda3_restool --extract-from-rom swedish.sfc --extract-dialogue
```

### Language Selection at Runtime

Once dialogue files are extracted, the game will select the language based on your `zelda3.ini`:

```ini
[General]
Language=de  # Options: us, de, fr, fr-c, en, es, pl, pt, nl, sv, redux
```

## Troubleshooting

### Error: ROM Not Found

```
Error: Could not open ROM file
```

**Solution:** Verify the ROM path exists:

```bash
./zelda3_restool --extract-from-rom /correct/path/to/zelda3.sfc
```

### Error: ROM Not Supported

```
Error: Unknown ROM (SHA1: xxxxxxxx...)
```

**Solution:** Verify your ROM hash matches a supported version:

```bash
sha1sum zelda3.sfc  # Linux/macOS
certutil -hashfile zelda3.sfc SHA1  # Windows
```

Compare with hashes in [Supported ROM Versions](#supported-rom-versions).

### Error: SMC Header Issues

The tool automatically detects and handles SMC headers (512-byte headers prepended to some ROM files). If you encounter issues:

```bash
# Check ROM size
ls -l zelda3.sfc
# 1048576 bytes = no header
# 1049088 bytes = has SMC header (handled automatically)
```

### Swedish ROM Issues

The Swedish ROM has a non-standard size (0x10083b) with an embedded header. The tool handles this automatically.

### Portuguese ROM String Count

The Portuguese ROM is limited to 397 strings due to missing FINISH byte in ROM data. This is handled automatically.

### Permission Denied

```
Error: Permission denied: 'assets/dungeon/dungeon-0.yaml'
```

**Solution:** Check directory permissions:

```bash
chmod -R u+w assets/
./zelda3_restool --extract-from-rom zelda3.sfc
```

### Incomplete Extraction

If extraction appears incomplete:

```bash
# Check file counts
find assets/dungeon -name "*.yaml" | wc -l  # Should be 322
find assets/overworld -name "*.yaml" | wc -l  # Should be 160

# If wrong, delete and re-extract
rm -rf assets/dungeon assets/overworld
./zelda3_restool --extract-from-rom zelda3.sfc
```

## Technical Details

### Dialogue Format

Dialogue strings use special command syntax:

```
1: Oh! You're finally awake![Waitkey][Scroll]You were in really bad shape![Waitkey][Scroll]A young boy named [Name] brought you here.[Waitkey]
```

**Commands:**
- `[Waitkey]` - Wait for button press
- `[Scroll]` - Scroll to next line
- `[Name]` - Insert player name
- `[Window]` - Window control
- `[Choose]` - Choice dialog
- `[Item]` - Display item name
- `[Number]` - Insert number
- `[Position]` - Text positioning
- `[ScrollSpd]` - Scroll speed control

**Special characters:**
- `[LinkL]` - Link character (left)
- `[Ankh]` - Ankh symbol
- `[Up]`, `[Down]`, `[Left]`, `[Right]` - Direction arrows
- `[A]`, `[B]`, `[X]`, `[Y]` - Button symbols

### Text Compression

Dialogue uses dictionary-based compression:

1. **Dictionary:** Language-specific common substrings (87-121 entries)
2. **Encoding:**
   - Characters 0-0x66: Direct character lookup (US) or 0-0x6F (EU)
   - Characters 0x67-0x7F: Control commands (US) or 0x70-0x87 (EU)
   - Characters 0x88+: Dictionary tokens
3. **Command format:** US uses COMMAND_START=0x67, EU uses COMMAND_START=0x70

### SNES Graphics Formats

#### 2bpp (2 bits per pixel, 4 colors)
- Tile size: 8×8 pixels
- Data per tile: 16 bytes
- Usage: Fonts, simple graphics

#### 3bpp (3 bits per pixel, 8 colors)
- Tile size: 8×8 pixels
- Data per tile: 24 bytes
- Usage: Most enemy sprites

#### 4bpp (4 bits per pixel, 16 colors)
- Tile size: 8×8 pixels
- Data per tile: 32 bytes
- Usage: Link sprite, complex graphics

### Palette Conversion

**SNES format:** 15-bit BGR (5 bits per channel)
```
Color format: 0bbbbbgg gggrrrrr
```

**PNG format:** 24-bit RGB (8 bits per channel)

Conversion expands 5-bit values to 8-bit:
```c
rgb = (value & 0x1F) << 3 | (value & 0x1F) >> 2
```

### ROM Address Mapping

The tool uses LoROM (Low ROM) memory mapping:

**Address Format:** 24-bit address written as `$BB:AAAA`
- `BB` = Bank number (0x00-0xFF)
- `AAAA` = Offset within bank (0x0000-0xFFFF)

**Conversion Formula:**
```
ROM_offset = (bank × 0x8000) + (offset - 0x8000)
```

**Examples:**
```
SNES $00:8000 → ROM offset 0x000000 (start of ROM)
SNES $10:8000 → ROM offset 0x080000 (512KB into ROM)
```

### Build Requirements

- **Compiler:** GCC, Clang, or MSVC
- **Build system:** CMake 3.10+
- **Dependencies:** libyaml (for YAML parsing)
- **Platforms:** Windows, Linux, macOS, Android

### System Requirements

- **Disk space:** ~50MB for intermediate files + 680KB for final asset
- **ROM file:** Valid 1MB SNES ROM (may have 512-byte SMC header, automatically handled)
