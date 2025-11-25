# Python Restool Usage Guide

This guide documents the Python-based asset extraction tool (`assets/restool.py`) for Zelda3. This tool extracts game assets from The Legend of Zelda: A Link to the Past ROM files and compiles them into `zelda3_assets.dat`, which is required to run the zelda3 engine.

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
# 1. Install Python dependencies
pip install -r requirements.txt

# 2. Place your ROM in the project root
cp /path/to/your/rom.sfc zelda3.sfc

# 3. Extract assets
python3 assets/restool.py --extract-from-rom

# 4. Verify output
ls -lh zelda3_assets.dat  # Should be ~2MB
```

## Command-Line Parameters

### `-r, --rom [PATH]`

Specify the ROM file path.

**Type:** Optional string
**Default:** Searches for `zelda3.sfc` or `zelda3.smc` in parent directory

```bash
# Use default ROM location
python3 assets/restool.py --extract-from-rom

# Specify custom path
python3 assets/restool.py --extract-from-rom -r /path/to/rom.sfc
python3 assets/restool.py --extract-from-rom --rom ~/Games/zelda3.sfc
```

### `-e, --extract-from-rom`

Extract all game assets from ROM and compile them into `zelda3_assets.dat`.

**Type:** Boolean flag
**Default:** False

This creates intermediate files in `assets/` directory:
- 320 dungeon room YAML files
- 160 overworld area YAML files
- Graphics (PNG files)
- Dialogue text files
- Music data
- Other game data

Then compiles everything into `zelda3_assets.dat` (~2MB).

```bash
python3 assets/restool.py --extract-from-rom
```

### `--extract-dialogue`

Extract dialogue strings from a translated ROM to a language-specific text file.

**Type:** Boolean flag
**Default:** False
**Requirements:** Must specify ROM path with `-r`

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
python3 assets/restool.py --extract-dialogue -r german.sfc

# Extract Spanish dialogue
python3 assets/restool.py --extract-dialogue -r spanish.sfc
```

**Note:** This option exits after extraction without compiling assets.

### `--languages L1,L2,...`

Specify additional languages to include in compiled `zelda3_assets.dat`.

**Type:** Comma-separated string
**Default:** None (US only)
**Valid values:** `de`, `fr`, `fr-c`, `en`, `es`, `pl`, `pt`, `redux`, `nl`, `sv`
**Requirements:** Corresponding `dialogue_XX.txt` file must exist

US language is always included. This option adds additional languages to the asset file.

```bash
# Include German and French
python3 assets/restool.py --extract-from-rom --languages=de,fr

# Include multiple languages
python3 assets/restool.py --extract-from-rom --languages de,es,pl,pt
```

### `--no-build`

Extract assets but skip compilation to `zelda3_assets.dat`.

**Type:** Boolean flag
**Default:** False

Useful for inspecting intermediate files without creating the final asset file.

```bash
python3 assets/restool.py --extract-from-rom --no-build

# Examine extracted data
cat assets/dungeon/dungeon-0.yaml
cat assets/overworld/overworld-0.yaml
open assets/linksprite.png
```

### `--print-strings`

Print all dialogue strings to console.

**Type:** Boolean flag
**Default:** False

Outputs all 397 dialogue strings in format: `ID: String content`

```bash
# Print to console
python3 assets/restool.py --print-strings

# Save to file
python3 assets/restool.py --print-strings > dialogue_dump.txt

# Print from specific ROM
python3 assets/restool.py --print-strings -r german.sfc > german_dialogue.txt
```

**Note:** This option skips asset compilation.

### `--print-assets-header`

Generate C header file with asset definitions.

**Type:** Boolean flag
**Default:** False

Creates preprocessor defines for accessing assets from C code.

```bash
# Generate header
python3 assets/restool.py --print-assets-header > src/assets.h
```

Output format:
```c
#pragma once
#include "types.h"

enum {
  kNumberOfAssets = 165
};
extern const uint8 *g_asset_ptrs[kNumberOfAssets];
extern uint32 g_asset_sizes[kNumberOfAssets];

#define kSprGfx(idx) FindInAssetArray(0, idx)
#define kOverworldMapGfx ((uint8*)g_asset_ptrs[12])
#define kOverworldMapGfx_SIZE (g_asset_sizes[12])
// ... more defines
```

### `--sprites-from-png`

Load sprites from PNG files instead of extracting from ROM during compilation.

**Type:** Boolean flag
**Default:** False

Enables custom sprite workflow - edit PNG files and rebuild assets with modifications.

```bash
# Extract sprites initially
python3 assets/restool.py --extract-from-rom

# Edit assets/linksprite.png or other PNG files

# Rebuild with modified sprites
python3 assets/restool.py --sprites-from-png
```

## Supported ROM Versions

The tool validates ROMs using SHA1 hashing. Only these versions are supported:

### Official Releases

| Language | Region | SHA1 Hash |
|----------|--------|-----------|
| English (US) | USA | `6D4F10A8B10E10DBE624CB23CF03B88BB8252973` |
| German | Germany | `2E62494967FB0AFDF5DA1635607F9641DF7C6559` |
| French | France | `229364A1B92A05167CD38609B1AA98F7041987CC` |
| French | Canada | `C1C6C7F76FFF936C534FF11F87A54162FC0AA100` |
| English | Europe | `7C073A222569B9B8E8CA5FCB5DFEC3B5E31DA895` |

### Fan Translations

| Language | Code | SHA1 Hash | Source |
|----------|------|-----------|--------|
| Spanish | `es` | `461FCBD700D1332009C0E85A7A136E2A8E4B111E` | [romhacking.net/2195](https://www.romhacking.net/translations/2195/) |
| Polish | `pl` | `3C4D605EEFDA1D76F101965138F238476655B11D` | [romhacking.net/5760](https://www.romhacking.net/translations/5760/) |
| Portuguese | `pt` | `D0D09ED41F9C373FE6AFDCCAFBF0DA8C88D3D90D` | [romhacking.net/6530](https://www.romhacking.net/translations/6530/) |
| Dutch | `nl` | `FA8ADFDBA2697C9A54D583A1284A22AC764C7637` | [romhacking.net/1124](https://www.romhacking.net/translations/1124/) |
| Swedish | `sv` | `43CD3438469B2C3FE879EA2F410B3EF3CB3F1CA4` | [romhacking.net/982](https://www.romhacking.net/translations/982/) |
| English Redux (v1) | `redux` | `B2A07A59E64C498BC1B2F28728F9BF4014C8D582` | [romhacking.net/6657](https://www.romhacking.net/translations/6657/) |
| English Redux (v2) | `redux` | `9325C22EB0A2A1F0017157C8B620BC3A605CEDE1` | [romhacking.net/hacks/2594](https://www.romhacking.net/hacks/2594/) |

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
python3 assets/restool.py --extract-from-rom

# Output: zelda3_assets.dat (~2MB)
```

### Workflow 2: Custom ROM Path

```bash
# ROM in different location
python3 assets/restool.py --extract-from-rom -r ~/Games/ROMs/zelda3.sfc
```

### Workflow 3: Multi-Language Build

See [Multi-Language Support](#multi-language-support) section below.

### Workflow 4: Inspect Extracted Data

```bash
# Extract without compiling
python3 assets/restool.py --extract-from-rom --no-build

# Examine YAML files
cat assets/dungeon/dungeon-0.yaml
cat assets/overworld/overworld-0.yaml

# View graphics
open assets/linksprite.png  # macOS
xdg-open assets/linksprite.png  # Linux

# Check dialogue
head -n 20 assets/dialogue.txt
```

### Workflow 5: Print Dialogue

```bash
# Print to console
python3 assets/restool.py --print-strings

# Save to file
python3 assets/restool.py --print-strings > dialogue_dump.txt

# From specific ROM
python3 assets/restool.py --print-strings -r german.sfc > german_dialogue.txt
```

### Workflow 6: Custom Sprite Development

```bash
# Step 1: Extract original sprites
python3 assets/restool.py --extract-from-rom

# Step 2: Edit sprites
# Modify assets/linksprite.png or other PNG files

# Step 3: Rebuild with modified sprites
python3 assets/restool.py --sprites-from-png
```

### Workflow 7: Generate C Header

```bash
# Generate assets.h
python3 assets/restool.py --print-assets-header > src/assets.h
```

## Output Files

### Final Output (Parent Directory)

```
zelda3/
└── zelda3_assets.dat          # ~2MB binary asset file
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
├── img/                        # Placeholder directory
├── sound/                      # Music extraction directory
│
├── linksprite.png             # Link sprites (128×448 pixels)
├── font.png                   # Font glyphs
├── hud_icons.png              # HUD icons
├── dialogue.txt               # US dialogue (397 strings)
├── dialogue_de.txt            # German dialogue (if extracted)
├── dialogue_fr.txt            # French dialogue (if extracted)
├── dialogue_es.txt            # Spanish dialogue (if extracted)
│   ... (other dialogue files)
│
├── map32_to_map16.txt         # Tile conversion table (8872 lines)
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

Total size: ~2MB (2,097,152 bytes typical)

## Multi-Language Support

### Prerequisites

1. Obtain ROMs for each language you want to support
2. Verify each ROM's SHA1 hash matches supported versions
3. Have US ROM for base asset extraction

### Step-by-Step Guide

#### Step 1: Extract Base Assets (US ROM Required)

```bash
# US ROM must be used for extracting base assets
python3 assets/restool.py --extract-from-rom -r usa.sfc
```

This creates `zelda3_assets.dat` with US language only.

#### Step 2: Extract Dialogue from Additional Languages

```bash
# Extract German dialogue
python3 assets/restool.py --extract-dialogue -r german.sfc
# Creates: assets/dialogue_de.txt

# Extract French dialogue
python3 assets/restool.py --extract-dialogue -r french.sfc
# Creates: assets/dialogue_fr.txt

# Extract Spanish dialogue
python3 assets/restool.py --extract-dialogue -r spanish.sfc
# Creates: assets/dialogue_es.txt

# Extract Polish dialogue
python3 assets/restool.py --extract-dialogue -r polish.sfc
# Creates: assets/dialogue_pl.txt
```

#### Step 3: Rebuild with All Languages

```bash
# Rebuild assets including all extracted languages
python3 assets/restool.py --extract-from-rom --languages=de,fr,es,pl
```

This creates `zelda3_assets.dat` with 5 languages: US + de + fr + es + pl

### Language Selection at Runtime

Once `zelda3_assets.dat` contains multiple languages, the game will select the language based on your system locale or you can specify it in `zelda3.ini`:

```ini
[Main]
Language=de  # Options: us, de, fr, fr-c, en, es, pl, pt, nl, sv, redux
```

### Supported Language Codes

| Code | Language |
|------|----------|
| `us` | English (USA) - Always included |
| `de` | German |
| `fr` | French (France) |
| `fr-c` | French (Canada) |
| `en` | English (Europe) |
| `es` | Spanish |
| `pl` | Polish |
| `pt` | Portuguese |
| `nl` | Dutch |
| `sv` | Swedish |
| `redux` | English Redux |

## Troubleshooting

### Error: ROM Not Found

```
Exception: Could not find any ROMs (zelda3.sfc, zelda3.smc) at the default location
```

**Solution:** Place ROM in project root or specify path with `-r`:

```bash
python3 assets/restool.py --extract-from-rom -r /path/to/rom.sfc
```

### Error: ROM Not Supported

```
Exception:

ROM with hash XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX not supported.

Expected 6D4F10A8B10E10DBE624CB23CF03B88BB8252973.
Please verify your ROM is "Legend of Zelda, The - A Link to the Past (USA)"
```

**Solution:** Verify your ROM hash matches a supported version:

```bash
sha1sum zelda3.sfc  # Linux/macOS
certutil -hashfile zelda3.sfc SHA1  # Windows
```

Compare with hashes in [Supported ROM Versions](#supported-rom-versions).

### Error: Invalid Language Code

```
Exception: Language XX is not valid
```

**Solution:** Use valid language codes: `de`, `fr`, `fr-c`, `en`, `es`, `pl`, `pt`, `redux`, `nl`, `sv`

```bash
# Correct
python3 assets/restool.py --extract-from-rom --languages=de,fr

# Wrong
python3 assets/restool.py --extract-from-rom --languages=german,french
```

### Error: Missing Dialogue File

```
Exception: assets/dialogue_de.txt not found. You need to extract it with --extract-dialogue using the ROM of that language.
```

**Solution:** Extract dialogue first:

```bash
python3 assets/restool.py --extract-dialogue -r german.sfc
python3 assets/restool.py --extract-from-rom --languages=de
```

### Error: Permission Denied

```
PermissionError: [Errno 13] Permission denied: 'assets/dungeon/dungeon-0.yaml'
```

**Solution:** Check directory permissions:

```bash
chmod -R u+w assets/
python3 assets/restool.py --extract-from-rom
```

### Error: ModuleNotFoundError

```
ModuleNotFoundError: No module named 'PIL'
```

**Solution:** Install Python dependencies:

```bash
pip install -r requirements.txt
# Or manually:
pip install Pillow PyYAML
```

### Incomplete Extraction

If extraction appears incomplete (missing files or wrong file count):

```bash
# Check file counts
find assets/dungeon -name "*.yaml" | wc -l  # Should be 322 (320 rooms + 2 templates)
find assets/overworld -name "*.yaml" | wc -l  # Should be 160

# If wrong, delete and re-extract
rm -rf assets/dungeon assets/overworld
python3 assets/restool.py --extract-from-rom
```

### zelda3_assets.dat Wrong Size

Expected size: ~2MB (around 2,097,152 bytes)

```bash
ls -lh zelda3_assets.dat
```

If size is significantly different:

```bash
# Re-run extraction
rm zelda3_assets.dat
python3 assets/restool.py --extract-from-rom
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

1. **Dictionary:** 97 common substrings (e.g., "and ", "the", " is", "ing ")
2. **Encoding:**
   - Characters 0-0x66: Direct character lookup
   - Characters 0x67-0x7F: Control commands
   - Characters 0x88+: Dictionary tokens
3. **Compression ratio:** Typically 40-60% size reduction

### SNES Graphics Formats

#### 2bpp (2 bits per pixel, 4 colors)
- Tile size: 8×8 pixels
- Data per tile: 16 bytes
- Usage: Fonts, simple graphics

#### 3bpp (3 bits per pixel, 8 colors)
- Tile size: 8×8 pixels
- Data per tile: 24 bytes
- Usage: Most sprites

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

### Compression Formats

The tool uses 5 compression types for asset data:

#### Type 0: Literal Data
```
Format: 000xxxxx [data...]
Length: xxxxx + 1 bytes
Action: Copy bytes literally
```

#### Type 1: Memset
```
Format: 001xxxxx [value]
Length: xxxxx + 1
Action: Write same byte (length) times
```

#### Type 2: Memset16
```
Format: 010xxxxx [byte1] [byte2]
Length: xxxxx + 1
Action: Write alternating bytes
```

#### Type 3: Increment
```
Format: 011xxxxx [start]
Length: xxxxx + 1
Action: Write incrementing sequence
```

#### Type 4: Back-reference
```
Format: 1xxxxxxx [offset_lo] [offset_hi]
Length: xxxxxxx + 1
Action: Copy from previous data at offset
```

#### Terminator
```
Format: 0xFF
Action: End of compressed data
```

### YAML Structure Examples

#### Dungeon Room
```yaml
Header:
  floor1: 1
  floor2: 15
  layout: 0
  bg2: Off
  collision: Nothing
  palette: 0
  blockset: 0
  effect: Nothing
  hole0_dest: [0, 0]
Sprites:
- [10, 5, 'upper', 'Keese-0']
- [15, 8, 'lower', 'Stalfos-0']
Secrets:
- [12, 7, 'Enemy_Drop_Pull']
Chests: [0x01]
Layer1: [...]
Layer2: []
Layer3: []
```

#### Overworld Area
```yaml
Header:
  name: Hyrule Castle
  size: big
  gfx: 0
  palette: 0
  music:
    beginning: Light_World
Travel: []
Entrances:
- index: 0
  x: 15
  y: 16
  entrance_id: 0
Exits: []
Items: []
Sprites.Beginning:
  info:
    gfx: 0
    palette: 0
  sprites:
  - [10, 5, 'Soldier-0']
```

### Dependencies

**Python:** 3.6+

**Required packages:**
- **Pillow:** Image processing
  - PNG read/write
  - SNES tile format encoding/decoding
  - Palette conversion
- **PyYAML:** YAML parsing
  - Dungeon/overworld YAML read/write
  - Music metadata

**Install:**
```bash
pip install -r requirements.txt
```

### System Requirements

- **Disk space:** ~50MB for intermediate files + 2MB for final asset
- **Memory:** ~500MB during compilation
- **ROM file:** Valid 1MB SNES ROM (may have 512-byte SMC header, automatically removed)
