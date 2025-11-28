# Zelda3 C Restool - Remaining Work

**Last Updated:** 2025-11-27

---

## Summary

### Implemented
- **165/165 assets** extracted (byte-perfect match with Python)
- **679,700 bytes** of asset data
- **11/11 languages** supported for dialogue extraction (byte-perfect match with Python)
- **Zero Python dependencies**
- **~6,500 lines of C code** (Python: 7,017 lines)
- **Platforms:** Windows, Linux, macOS, Android

---

## Multi-Language Dialogue Support

Supported languages (all byte-perfect match with Python):

| Language | Code | Alphabet | Dictionary | Status |
|----------|------|----------|------------|--------|
| US English | us | 95 chars | 87 entries | Verified |
| Europe English | en | 112 chars | 87 entries | Verified |
| German | de | 112 chars | 112 entries | Verified |
| French | fr | 112 chars | 99 entries | Verified |
| Canadian French | fr-c | 112 chars | 99 entries | Verified |
| Swedish | sv | 99 chars | 97 entries | Verified |
| Polish | pl | 99 chars | 97 entries | Verified |
| Portuguese | pt | 121 chars | 97 entries | Verified |
| Spanish | es | 99 chars | 97 entries | Verified |
| Dutch | nl | 94 chars | 97 entries | Verified |
| Redux | redux | (uses US) | (uses US) | Verified |

### Implementation Notes

Each language requires:
- Custom alphabet array (character mappings)
- Custom dictionary array (compression tokens)
- Language-specific configuration (ROM addresses, escape characters, command format)

Special handling:
- **Swedish ROM:** Header workaround for 0x10083b size
- **Portuguese ROM:** Limited to 397 strings (no FINISH byte in ROM)
- **EU languages:** Different command format (EU vs US encoder)

---

## Python Features Not Implemented in C

The following features exist in the Python `restool.py` but are not present in the C implementation:

### 1. `--sprites-from-png` Flag

**Python:** Loads sprite graphics from PNG files instead of ROM data
- Used for: ROM hacking, sprite modifications, custom graphics
- Implementation: ~50 lines (uses PIL library)

**C:** ✅ **Implemented** (November 2025)
- Uses lodepng to parse sprite sheet PNGs (assets/sprites/sprites_*.png)
- Decodes embedded metadata tags to identify tileset IDs and palette info
- Converts 24-bit RGB pixels back to indexed format using embedded palette
- Encodes to SNES 3bpp planar format (1536 bytes per tileset)
- ~400 lines in `sprite_loader.c`
- Byte-perfect match with Python output verified

### 2. `--print-strings` Debug Mode

**Python:** Decompresses and prints all dialogue strings
- Used for: Debugging text compression, verifying dialogue
- Implementation: Uses existing decompression code

**C:** Partially implemented via `--extract-dialogue`
- Extracts dialogue to text files
- Could add stdout printing mode

**Implementation requirement:**
- Add `--print-strings` flag to print to stdout instead of file
- **Estimated:** ~50 lines

### 3. `--print-assets-header` Debug Mode

**Python:** Prints asset metadata and header information
- Used for: Debugging asset compilation

**C:** Not implemented

**Implementation requirement:**
- Asset header parsing
- Metadata formatting
- **Estimated:** ~100 lines

### 4. Default Compilation Behavior

**Python:** Compiles `zelda3_assets.dat` by default (unless `--no-build`)
```bash
python3 assets/restool.py --extract-from-rom
# Automatically compiles assets
```

**C:** Requires explicit `--compile` flag
```bash
./zelda3_restool --extract-from-rom zelda3.sfc --compile
# Must explicitly request compilation
```

**Implementation requirement:**
- Change default behavior to auto-compile
- Add `--no-compile` flag to skip compilation
- **Estimated:** ~50 lines

### 5. Multiple Languages in Single Build

**Python:** `--languages de,fr,es` compiles multiple languages into single asset file
- Allows runtime language switching
- All language data packed into zelda3_assets.dat

**C:** Single language per extraction (outputs to separate files)

**Implementation requirement:**
- Language selection/packing logic
- Multi-language asset structure
- Runtime language switching support
- **Estimated:** ~400 lines

### 6. Individual Extraction Options (C has more than Python)

**Note:** This is inverse - C has features Python doesn't:
- `--extract-graphics` (Link sprites only)
- `--extract-enemy-sheet <N>` (specific enemy tileset)
- `--extract-overworld` (overworld data only)
- `--extract-dialogue` (dialogue strings only)
- `--test-*` flags (verification modes)

**Python:** Extracts everything at once, no granular control

---

### Summary of Missing Features

| Feature | Python | C | Effort (lines) |
|---------|--------|---|----------------|
| Multi-language dialogue | 11 languages | 11 languages | Done |
| `--sprites-from-png` | Yes | ✅ Yes | Done (~400) |
| `--print-strings` | Yes | Partial | ~50 |
| `--print-assets-header` | Yes | No | ~100 |
| Default auto-compile | Yes | No | ~50 |
| Multiple languages/build | Yes | No | ~400 |
| **TOTAL** | - | - | **~600 lines remaining** |

---

## Not Implemented

### SNES Compression (Encoding)

- Decompression: implemented
- Compression: not implemented
- Use cases: Asset modification, ROM hacking, custom asset injection
- Effort: ~500-1000 lines

### Asset Modification/Injection Tools

- Modify zelda3_assets.dat in-place
- Replace individual assets
- Re-compile modified assets back to ROM
- Effort: ~2000+ lines

---

## Notes

### Current Performance
- Asset compilation: ~2-3 seconds
- Python: ~5-10 seconds

### Compiler Warnings
- GNU extension warnings (harmless)

### Unused Functions
- CopyAssetFromPython, ExtractRomBasedAssets (kept for potential future use)
