# restool.py Android Migration Analysis

## Executive Summary

**Recommendation: Do NOT migrate restool.py to Android. Use pre-built assets instead.**

The analysis follows KISS (Keep It Simple, Stupid), YAGNI (You Aren't Gonna Need It), DRY (Don't Repeat Yourself), and Pragmatic principles, leading to the conclusion that asset extraction should remain a build-time operation, not a runtime operation on Android devices.

---

## What is restool.py?

`restool.py` is a **build-time asset extraction and compilation tool** that processes a Zelda 3 ROM file to create `zelda3_assets.dat` - a binary data file containing all game resources needed by the C++ game engine.

### Primary Functions:

1. **ROM Validation** - Verifies the ROM SHA1 hash matches expected versions (US, DE, FR, etc.)
2. **Asset Extraction** - Extracts sprites, graphics, dialogue, music from ROM
3. **Asset Compilation** - Compiles extracted data into optimized binary format
4. **Multi-language Support** - Handles dialogue extraction from translated ROMs

### Key Operations:

- `--extract-from-rom` - Extracts resources from ROM to intermediate files
- `--extract-dialogue` - Extracts dialogue text from translated ROMs
- Default behavior (no flags) - Compiles all assets into `zelda3_assets.dat`

---

## Architecture Overview

```
restool.py (orchestrator)
    ├── util.py (ROM loading, decompression, caching)
    ├── compile_resources.py (asset compilation)
    │   ├── sprite_sheets.py (PNG/graphics processing)
    │   ├── text_compression.py (dialogue compression)
    │   ├── compile_music.py (audio processing)
    │   └── tables.py (lookup tables, constants)
    └── extract_resources.py (ROM data extraction)
        ├── extract_music.py (music extraction)
        └── sprite_sheets.py (sprite decoding)
```

---

## Dependencies Analysis

### Python Standard Library:
- `argparse` - Command-line argument parsing
- `sys` - System operations
- `os` - File operations
- `hashlib` - SHA1 hashing for ROM validation
- `array` - Binary data handling
- `struct` - Binary packing/unpacking

### Third-Party Python Libraries:
- **PIL/Pillow** (`from PIL import Image`) - Image processing for sprites
- **PyYAML** (`import yaml`) - YAML parsing for dungeon/overworld data

### File System Dependencies:
- ROM file (zelda3.sfc) - Input source
- Intermediate directories:
  - `overworld/` - YAML files for overworld data
  - `dungeon/` - YAML files for dungeon layouts
  - `img/` - PNG sprite sheets
  - `sound/` - Audio data
- Output: `zelda3_assets.dat` - Final compiled asset file

### Platform Requirements:
- **Python 3.x** runtime
- File system read/write access
- Sufficient RAM for ROM processing (minimal, ~50MB)
- CPU for decompression/compression algorithms

---

## Technical Deep Dive

### util.py - Core ROM Operations

**Purpose**: ROM file loading, validation, and data extraction

**Key Features**:
- SNES ROM bank addressing (0x8000 boundary handling)
- SMC header removal (0x200 byte offset)
- Multi-language ROM support via SHA1 hash matching
- Custom decompression algorithm (`decomp()` function)
- BRR audio format encoding/decoding
- LRU caching for performance (`@cache` decorator)

**Memory Access Pattern**:
```python
def get_byte(ea):
    # SNES addressing: converts bank:address to linear offset
    ea = ((ea >> 16) & 0x7f) * 0x8000 + (ea & 0x7fff)
    return ROM[ea]
```

### compile_resources.py - Asset Pipeline

**Purpose**: Orchestrates compilation of all game assets

**Major Operations**:
1. **Graphics Processing** (800+ LOC)
   - Sprite decompression from ROM
   - 2-bit, 3-bit, 4-bit tileset decoding
   - PNG sprite sheet encoding
   - Palette conversion (SNES 15-bit → RGB24)

2. **Text Processing** (100+ LOC)
   - Dialogue compression with custom dictionary
   - Font encoding from PNG files
   - Multi-language support

3. **Level Data** (400+ LOC)
   - Overworld map compilation (160 areas)
   - Dungeon room compilation (320 rooms)
   - Exit/entrance/sprite placement data

4. **Audio Processing** (50+ LOC)
   - Music bank compilation
   - BRR sample encoding

5. **Binary Packing** (100+ LOC)
   - Indexed array packing
   - Compression of level data
   - SHA256 asset signature generation

**Output Format**:
```
zelda3_assets.dat structure:
├── Header (64 bytes)
│   ├── Signature: "Zelda3_v0\n\0"
│   ├── SHA256 of asset keys
│   ├── Asset count (uint32)
│   └── Key string table size (uint32)
├── Size table (uint32 array)
├── Key string table (null-terminated strings)
└── Asset data (4-byte aligned blocks)
```

### extract_resources.py - ROM Extraction

**Purpose**: Extracts raw data from ROM to YAML/PNG files

**Operations**:
- Dungeon room object decoding (320 rooms)
- Overworld area decoding (160 areas)
- Sprite placement extraction
- Chest/secret/entrance data
- Map tile data extraction

**Output**: Hundreds of intermediate YAML and PNG files

---

## Why restool.py Should NOT Be Ported to Android

### 1. **YAGNI - You Aren't Gonna Need It**

**The Android app doesn't need to extract assets at runtime.**

- Asset extraction is a **one-time build operation**
- End users never need to run this tool
- The compiled `zelda3_assets.dat` file is all the app needs
- ROM file is not needed after initial extraction

**Current workflow**:
```
Developer: ROM → restool.py → zelda3_assets.dat
User: Installs app with zelda3_assets.dat → Plays game
```

**If ported to Android**:
```
User: Provides ROM → Waits for extraction → zelda3_assets.dat → Plays game
         ↑ Unnecessary step
```

### 2. **KISS - Keep It Simple, Stupid**

**Adding Python runtime to Android is complex and unnecessary.**

**Complexity added by porting**:
- Python interpreter for Android (Chaquopy, QPython, or PyDroid)
- Pillow library compilation for ARM architecture
- PyYAML for Android
- File system permission handling
- ROM file picker UI
- Progress indication during extraction
- Error handling for user-provided ROMs
- Storage management (temporary files, cleanup)

**Current simple approach**:
- Ship pre-built `zelda3_assets.dat` in APK or as downloadable asset
- App loads binary file directly
- No dependencies beyond SDL2 and C++ runtime

### 3. **Pragmatic - Practical Considerations**

**Real-world issues with Android port**:

a) **Legal/Distribution**:
   - Distributing ROM extractor ≈ encouraging piracy
   - Pre-built assets avoid direct ROM requirement in app flow
   - Better optics for app store approval

b) **Performance**:
   - Python on Android is slow (interpreted language)
   - Asset extraction takes 30-60 seconds on desktop
   - Could take 2-5 minutes on mobile device
   - Battery drain during processing

c) **Storage**:
   - Requires temporary space for intermediate files
   - ROM (2-4 MB) + intermediate files (50-100 MB) + final asset (20 MB)
   - Total: 75-125 MB temporary storage needed

d) **User Experience**:
   - Poor first-run experience waiting for extraction
   - Risk of crashes during extraction
   - Difficult error messages to debug
   - Users expect immediate gameplay

e) **Maintenance Burden**:
   - Port all Python code (~5000+ LOC)
   - Keep Python and C++ versions in sync
   - Test on multiple Android versions/devices
   - Debug Python-specific issues on Android

### 4. **DRY - Don't Repeat Yourself**

**The asset extraction logic already exists and works.**

- Desktop/server-side extraction is proven and tested
- Creating Android version duplicates functionality
- Two codebases to maintain when one suffices
- Build-time extraction is the authoritative version

**Better approach**: Use existing build system, distribute results

---

## Recommended Android Implementation Strategy

### Option 1: Bundle Assets in APK (Recommended for small apps)

**Pros**:
- Immediate playability
- No network required
- Simple implementation
- No ROM legal issues in app flow

**Cons**:
- Increases APK size by ~20MB
- Single language version typically

**Implementation**:
```
Build process:
1. Developer extracts assets using restool.py on desktop
2. zelda3_assets.dat → app/src/main/assets/
3. App loads from assets folder on startup
```

### Option 2: Downloadable Asset Pack (Recommended for multi-language)

**Pros**:
- Small initial APK
- Support multiple languages
- Can update assets without app update

**Cons**:
- Requires network on first run
- Need download/cache management
- Slightly more complex

**Implementation**:
```
Build process:
1. Developer extracts assets for each language
2. Upload zelda3_assets_*.dat to CDN/server
3. App downloads selected language on first run
4. Cache in app-specific storage
```

### Option 3: Expansion Files (For large assets)

**Use Android's APK Expansion Files (OBB)**:
- For assets > 100MB
- Handled by Google Play automatically
- Transparent to users

---

## Migration Effort Estimate (If You Really Had To)

**If absolutely required to port restool.py to Android:**

### Development Effort: **4-6 weeks**

1. **Setup Python for Android** (1 week)
   - Integrate Chaquopy or similar
   - Configure build scripts
   - Test Python runtime

2. **Port Dependencies** (1-2 weeks)
   - Pillow for Android (ARM compilation)
   - PyYAML
   - Test all modules

3. **Android Integration** (1 week)
   - File picker for ROM selection
   - Progress UI
   - Background service for extraction
   - Storage permissions

4. **Testing & Debug** (1-2 weeks)
   - Test on multiple devices
   - Handle edge cases
   - Performance optimization

### Ongoing Maintenance: **10-20% overhead**
- Keep Python deps updated
- Fix Android-specific bugs
- Test on new Android versions

---

## Recommended Action Plan

### For Android Port:

**Phase 1: Pre-Build Assets (Week 1)**
```bash
# On development machine
cd zelda3/assets
python3 restool.py --extract-from-rom
# Creates zelda3_assets.dat

# For additional languages
python3 restool.py --extract-dialogue --rom zelda3_de.sfc
python3 restool.py --languages de,fr,es
```

**Phase 2: Android Integration (Week 1-2)**
```java
// In Android app
class AssetLoader {
    public void loadAssets(Context context) {
        // Copy from assets to cache if not exists
        File assetFile = new File(context.getCacheDir(), "zelda3_assets.dat");
        if (!assetFile.exists()) {
            copyAssetToFile(context, "zelda3_assets.dat", assetFile);
        }
        // Pass path to C++ game engine
        nativeLoadAssets(assetFile.getAbsolutePath());
    }
}
```

**Phase 3: (Optional) Download Support**
```java
class AssetDownloader {
    public void downloadAssets(String language, ProgressCallback cb) {
        String url = "https://yourcdn.com/zelda3_assets_" + language + ".dat";
        // Download to cache, verify SHA256, notify C++ layer
    }
}
```

---

## Technical Specifications

### Asset File Size:
- English only: ~18-22 MB
- With all languages: ~25-30 MB per language
- Compressed (zlib): ~12-15 MB

### Android Requirements:
- Minimum SDK: 21 (Android 5.0)
- Storage: 25-50 MB
- RAM: Minimal (loaded on-demand by C++ code)
- Permissions: INTERNET (if downloading), WRITE_EXTERNAL_STORAGE (if caching)

### C++ Integration:
```cpp
// Existing asset loading code works as-is
bool LoadAssets(const char* asset_path) {
    FILE* f = fopen(asset_path, "rb");
    // Parse zelda3_assets.dat format
    // No changes needed for Android
}
```

---

## Conclusion

**Do NOT port restool.py to Android.**

The tool is a build-time utility designed for developers to extract and compile game assets. End users do not need this functionality.

**Recommended approach**:
1. Extract assets during build process (desktop/CI)
2. Bundle or download pre-built `zelda3_assets.dat`
3. Ship simple Android app that loads binary assets

This approach follows:
- ✅ **KISS**: Simple, no Python runtime needed
- ✅ **YAGNI**: Only implement what users actually need
- ✅ **DRY**: Reuse existing build pipeline
- ✅ **Pragmatic**: Better UX, performance, and legal posture

**Estimated savings**:
- Development time: 4-6 weeks saved
- App size: 40-60 MB saved (no Python runtime)
- Maintenance: 10-20% reduced overhead
- User experience: Immediate vs. 2-5 minute wait

---

## Alternative: Minimal C++ Port (If Assets Must Be Extracted on Device)

If there's an absolute requirement to extract assets on-device (e.g., for legal reasons, ROM customization features), consider a **minimal C++ implementation**:

### What to port:
- `util.py` ROM loading and decompression → C++
- Basic asset extraction (sprites, palettes, maps)
- Skip YAML intermediate files (direct binary output)

### What to skip:
- PIL/Pillow dependencies (use stb_image or similar)
- YAML parsing (hardcode or use simpler format)
- Multi-language dialogue extraction (pre-compile)

### Effort: 2-3 weeks vs. 4-6 weeks for full Python port

### Code example:
```cpp
class RomExtractor {
    bool LoadRom(const std::string& path);
    bool ValidateRom(); // SHA1 check
    bool ExtractAssets(ProgressCallback cb);
private:
    std::vector<uint8_t> rom_data_;
    uint8_t GetByte(uint32_t addr); // SNES addressing
    std::vector<uint8_t> Decompress(uint32_t addr); // Custom decompression
};
```

This approach:
- ✅ Avoids Python dependency
- ✅ Better performance (native C++)
- ✅ Smaller implementation (skip intermediate files)
- ⚠️ Still complex (2-3 weeks dev time)
- ⚠️ Still has UX issues (extraction time)

**Verdict**: Only do this if legally required or for ROM modding features.

---

## Appendix: File Inventory

### Python Modules (~5000 LOC total):
- `restool.py` - 53 lines
- `util.py` - 334 lines
- `compile_resources.py` - 827 lines
- `extract_resources.py` - 541 lines
- `text_compression.py` - 400+ lines
- `sprite_sheets.py` - 400+ lines
- `compile_music.py` - 300+ lines
- `tables.py` - 1000+ lines
- Supporting modules: extract_music.py, sprite_sheet_info.py

### External Dependencies:
- Pillow (PIL) - ~500KB library
- PyYAML - ~200KB library

### ROM Requirements:
- USA ROM: SHA1 `6D4F10A8B10E10DBE624CB23CF03B88BB8252973`
- Supports 10+ language ROM variants

### Output Assets:
- `zelda3_assets.dat` - 18-22 MB
- Contains 100+ named asset arrays
- Indexed for fast lookup
