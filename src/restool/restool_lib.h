// restool_lib.h - Library API for restool functions
// Used by Android app and other embedding scenarios
#ifndef RESTOOL_LIB_H
#define RESTOOL_LIB_H

#include <stdbool.h>
#include <stddef.h>

// Error codes
#define RESTOOL_OK                0
#define RESTOOL_ERR_ROM_LOAD      1
#define RESTOOL_ERR_ROM_INVALID   2
#define RESTOOL_ERR_ROM_NOT_US    3
#define RESTOOL_ERR_EXTRACT       4
#define RESTOOL_ERR_WRITE         5
#define RESTOOL_ERR_DIALOGUE      6
#define RESTOOL_ERR_MEMORY        7

// ROM identification result
typedef struct {
    char lang_code[16];   // Language code (us, de, fr, etc.)
    char lang_name[64];   // Display name (USA, German, French, etc.)
    bool valid;           // True if SHA1 matches a known ROM
} RestoolRomInfo;

// Identify ROM file and get language info
// rom_path: Path to ROM file
// out_info: Output struct with language info
// Returns: true if ROM was read successfully (even if unknown)
bool Restool_IdentifyRom(const char *rom_path, RestoolRomInfo *out_info);

// Extract dialogue from a single ROM file to text file
// rom_path: Path to ROM file (any language)
// output_dir: Directory to write dialogue_{lang}.txt (or dialogue.txt for US)
// Returns: RESTOOL_OK on success, error code on failure
int Restool_ExtractDialogue(const char *rom_path, const char *output_dir);

// Options for asset compilation
typedef struct {
    const char *us_rom_path;     // Path to US ROM file (required)
    const char *output_path;     // Path to output zelda3_assets.dat
    const char *languages;       // Comma-separated language codes (e.g., "de,fr") or NULL for US only
    const char *dialogue_dir;    // Directory containing dialogue_{lang}.txt files (can be NULL)
    bool sprites_from_png;       // If true, load sprites from PNG files instead of ROM
} RestoolCompileOptions;

// Compile assets from US ROM with optional additional languages
// options: Compilation options struct
// Returns: RESTOOL_OK on success, error code on failure
int Restool_CompileAssetsEx(const RestoolCompileOptions *options);

// Legacy API - kept for compatibility
// us_rom_path: Path to US ROM file (required)
// output_path: Path to output zelda3_assets.dat
// languages: Comma-separated language codes (e.g., "de,fr") or NULL for US only
// dialogue_dir: Directory containing dialogue_{lang}.txt files (can be NULL to use default)
// Returns: RESTOOL_OK on success, error code on failure
int Restool_CompileAssets(const char *us_rom_path, const char *output_path,
                          const char *languages, const char *dialogue_dir);

// Set the directory where dialogue files are loaded from
// Used internally by Restool_CompileAssets when dialogue_dir is specified
void Restool_SetDialogueDir(const char *dir);

// Get the current dialogue directory override (NULL = use default assets/)
const char *Restool_GetDialogueDir(void);

// ===========================================================================
// DAT File Access (for launcher UI)
// ===========================================================================

// Check if zelda3_assets.dat exists in the given directory
bool Restool_DatFileExists(const char *dir);

// Get available languages from DAT file
// dir: directory containing zelda3_assets.dat
// languages: output array for language codes (e.g., "us", "de", "retrans-kal")
// max_languages: size of languages array
// Returns: number of languages found, 0 on error
int Restool_GetDatLanguages(const char *dir, char languages[][16], int max_languages);

#endif // RESTOOL_LIB_H
