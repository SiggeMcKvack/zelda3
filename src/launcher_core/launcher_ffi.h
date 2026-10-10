// C-ABI FFI surface for the Tauri launcher backend.
//
// The on-disk zelda3.ini is the contract (replay compatibility requires exact
// byte-for-byte output), so config is marshalled as a JSON string that mirrors
// the decoded Config struct plus the keyboard/gamepad control-mapping state.
// All returned char* are heap-allocated; free them with LauncherFfi_FreeString.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Read the INI at ini_path and return its full configuration as a JSON string.
// Returns NULL on failure (e.g. file unreadable/unparseable).
char *LauncherFfi_LoadConfigJson(const char *ini_path);

// Apply config_json onto a Config + control-mapping state, validate it, and
// write the canonical INI to ini_path. Returns a JSON object string
// {"ok":bool,"error":string-or-null}. Returns NULL only on allocation failure.
char *LauncherFfi_SaveConfigJson(const char *ini_path, const char *config_json);

// Validate config_json without writing. Returns {"ok":bool,"error":...}.
char *LauncherFfi_ValidateConfigJson(const char *config_json);

// Write the default zelda3.ini (the build-time template) to ini_path, replacing
// any existing file. Returns {"ok":bool,"error":...}.
char *LauncherFfi_ResetDefaults(const char *ini_path);

// Identify a ROM file. Returns {"read":bool,"valid":bool,"lang_code":str,"lang_name":str}.
// rom_path may be a translation patch (.ips, .vcdiff, .xdelta or .ups); it is then applied to the US ROM at
// base_rom_path (may be NULL for plain ROMs).
char *LauncherFfi_IdentifyRom(const char *rom_path, const char *base_rom_path);

// Create zelda3_assets.dat from a US ROM (+ optional language ROMs).
// options_json: {"us_rom_path":str,"output_dir":str,"language_roms":[path,...]}
// Returns {"ok":bool,"error":str-or-null,"languages":[str,...]}.
char *LauncherFfi_CreateDat(const char *options_json);

// Report DAT presence + languages in dir. Returns {"exists":bool,"languages":[str,...]}.
char *LauncherFfi_DatStatus(const char *dir);

// Progress callback for long-running encodes. `fraction` is 0..1 overall.
typedef void (*LauncherProgressCb)(const char *message, float fraction, void *user);

// Encode every <prefix><N>.pcm in `folder` to <prefix><N>.opuz (MSU -> OPUZ).
// Calls `cb` (may be NULL) with progress. Returns
// {"ok":bool,"error":str-or-null,"encoded":int,"total":int}.
// If opus support was not compiled in, returns ok:false with an explanatory error.
char *LauncherFfi_EncodeMsuToOpus(const char *folder, const char *prefix,
                                  LauncherProgressCb cb, void *user);

// Free a string returned by any LauncherFfi_* function.
void LauncherFfi_FreeString(char *s);

#ifdef __cplusplus
}
#endif
