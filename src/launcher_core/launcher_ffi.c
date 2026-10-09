// C-ABI FFI surface implementation. See launcher_ffi.h.
//
// LoadConfigJson:  ConfigReader_Read -> serialize Config + keymap globals to JSON.
// SaveConfigJson:  parse JSON -> Config + keymap globals -> Validate -> Write.
//
// The JSON is a 1:1 dump of the *decoded* Config struct (the same struct the INI
// writer consumes), so the marshalling cannot drift from the INI option set: any
// field ConfigWriter emits comes straight from a Config field or a keymap global.
#include "launcher_ffi.h"
#include "config_reader.h"
#include "config_writer.h"
#include "keymap_state.h"
#include "control_strings.h"
#include "default_config_ini.h"
#include "../config.h"
#include "restool_lib.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// ----------------------------------------------------------------------------
// Keymap global tables (drive emit, parse, and free uniformly)
// ----------------------------------------------------------------------------

// Single-string controls (one binding each), shared key name for kbd + gamepad.
typedef struct { const char *key; char **kbd; char **pad; } ScalarDef;
static const ScalarDef kScalars[] = {
    {"cheat_life",        &g_kbd_cheat_life,        &g_gamepad_cheat_life},
    {"cheat_keys",        &g_kbd_cheat_keys,        &g_gamepad_cheat_keys},
    {"cheat_walkthrough", &g_kbd_cheat_walkthrough, &g_gamepad_cheat_walkthrough},
    {"clear_keylog",      &g_kbd_clear_keylog,      &g_gamepad_clear_keylog},
    {"stop_replay",       &g_kbd_stop_replay,       &g_gamepad_stop_replay},
    {"fullscreen",        &g_kbd_fullscreen,        &g_gamepad_fullscreen},
    {"reset",             &g_kbd_reset,             &g_gamepad_reset},
    {"pause_dimmed",      &g_kbd_pause_dimmed,      &g_gamepad_pause_dimmed},
    {"pause",             &g_kbd_pause,             &g_gamepad_pause},
    {"turbo",             &g_kbd_turbo,             &g_gamepad_turbo},
    {"replay_turbo",      &g_kbd_replay_turbo,      &g_gamepad_replay_turbo},
    {"window_bigger",     &g_kbd_window_bigger,     &g_gamepad_window_bigger},
    {"window_smaller",    &g_kbd_window_smaller,    &g_gamepad_window_smaller},
    {"volume_up",         &g_kbd_volume_up,         &g_gamepad_volume_up},
    {"volume_down",       &g_kbd_volume_down,       &g_gamepad_volume_down},
    {"display_perf",      &g_kbd_display_perf,      &g_gamepad_display_perf},
    {"toggle_renderer",   &g_kbd_toggle_renderer,   &g_gamepad_toggle_renderer},
};
static const int kNumScalars = (int)(sizeof(kScalars) / sizeof(kScalars[0]));

// Multi-slot controls (comma-separated arrays in the INI).
typedef struct { const char *key; char **kbd; char **pad; int n; } ArrayDef;
static const ArrayDef kArrays[] = {
    {"controls", g_kbd_controls, g_gamepad_controls, NUM_SNES_BUTTONS},
    {"load",     g_kbd_load,     g_gamepad_load,     NUM_SAVE_SLOTS},
    {"save",     g_kbd_save,     g_gamepad_save,     NUM_SAVE_SLOTS},
    {"replay",   g_kbd_replay,   g_gamepad_replay,   NUM_SAVE_SLOTS},
};
static const int kNumArrays = (int)(sizeof(kArrays) / sizeof(kArrays[0]));

// Free + NULL every keymap global so repeated FFI calls don't leak.
static void keymap_free_all(void) {
    for (int i = 0; i < kNumScalars; i++) {
        free(*kScalars[i].kbd); *kScalars[i].kbd = NULL;
        free(*kScalars[i].pad); *kScalars[i].pad = NULL;
    }
    for (int i = 0; i < kNumArrays; i++) {
        for (int j = 0; j < kArrays[i].n; j++) {
            free(kArrays[i].kbd[j]); kArrays[i].kbd[j] = NULL;
            free(kArrays[i].pad[j]); kArrays[i].pad[j] = NULL;
        }
    }
}

// Free the heap strings ConfigReader_Read allocated into a Config.
static void config_free_strings(Config *c) {
    free((void*)c->link_graphics); c->link_graphics = NULL;
    free((void*)c->shader);        c->shader = NULL;
    free((void*)c->msu_path);      c->msu_path = NULL;
    free((void*)c->language);      c->language = NULL;
    free(c->memory_buffer);        c->memory_buffer = NULL;
}

// ----------------------------------------------------------------------------
// Small JSON helpers
// ----------------------------------------------------------------------------

static void add_str(cJSON *obj, const char *key, const char *val) {
    cJSON_AddStringToObject(obj, key, val ? val : "");
}

static void add_str_array(cJSON *obj, const char *key, char **arr, int n) {
    cJSON *a = cJSON_AddArrayToObject(obj, key);
    for (int i = 0; i < n; i++)
        cJSON_AddItemToArray(a, cJSON_CreateString(arr[i] ? arr[i] : ""));
}

// Return strdup of a string item, or NULL when absent/empty (writer treats
// NULL and "" identically, so this is lossless).
static char *get_str(const cJSON *obj, const char *key) {
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsString(it) || !it->valuestring || !it->valuestring[0]) return NULL;
    return strdup(it->valuestring);
}

// Read an integer that must fit [lo, hi] (the range of the Config field it goes
// into). Out-of-range values record `key` in *bad instead of being truncated.
static int get_int(const cJSON *obj, const char *key, int lo, int hi, const char **bad) {
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsNumber(it)) return 0;
    if (it->valuedouble < lo || it->valuedouble > hi) {
        if (!*bad) *bad = key;
        return 0;
    }
    return it->valueint;
}

static bool get_bool(const cJSON *obj, const char *key, bool def) {
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsBool(it)) return cJSON_IsTrue(it);
    if (cJSON_IsNumber(it)) return it->valueint != 0;
    return def;
}

static uint32 get_u32(const cJSON *obj, const char *key, uint32 def) {
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(it) ? (uint32)it->valuedouble : def;
}

// Read a string array; frees existing slots first. Fills "" -> NULL.
static void get_str_array(const cJSON *obj, const char *key, char **arr, int n) {
    const cJSON *a = cJSON_GetObjectItemCaseSensitive(obj, key);
    for (int i = 0; i < n; i++) {
        free(arr[i]);
        arr[i] = NULL;
        const cJSON *it = (a && cJSON_IsArray(a)) ? cJSON_GetArrayItem(a, i) : NULL;
        if (cJSON_IsString(it) && it->valuestring)
            arr[i] = strdup(it->valuestring);
        else
            arr[i] = strdup("");  // preserve slot positions, like the INI parser
    }
}

// ----------------------------------------------------------------------------
// Config <-> JSON
// ----------------------------------------------------------------------------

static cJSON *config_to_json(const Config *c) {
    cJSON *root = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "window_width", c->window_width);
    cJSON_AddNumberToObject(root, "window_height", c->window_height);
    cJSON_AddBoolToObject(root, "enhanced_mode7", c->enhanced_mode7);
    cJSON_AddBoolToObject(root, "new_renderer", c->new_renderer);
    cJSON_AddBoolToObject(root, "ignore_aspect_ratio", c->ignore_aspect_ratio);
    cJSON_AddNumberToObject(root, "fullscreen", c->fullscreen);
    cJSON_AddNumberToObject(root, "window_scale", c->window_scale);
    cJSON_AddBoolToObject(root, "enable_audio", c->enable_audio);
    cJSON_AddBoolToObject(root, "linear_filtering", c->linear_filtering);
    cJSON_AddNumberToObject(root, "output_method", c->output_method);
    cJSON_AddNumberToObject(root, "audio_freq", c->audio_freq);
    cJSON_AddNumberToObject(root, "audio_channels", c->audio_channels);
    cJSON_AddNumberToObject(root, "audio_samples", c->audio_samples);
    cJSON_AddBoolToObject(root, "autosave", c->autosave);
    cJSON_AddNumberToObject(root, "extended_aspect_ratio", c->extended_aspect_ratio);
    cJSON_AddNumberToObject(root, "custom_aspect_w", c->custom_aspect_w);
    cJSON_AddNumberToObject(root, "custom_aspect_h", c->custom_aspect_h);
    cJSON_AddBoolToObject(root, "extend_y", c->extend_y);
    cJSON_AddBoolToObject(root, "unchanged_sprites", c->unchanged_sprites);
    cJSON_AddBoolToObject(root, "no_visual_fixes", c->no_visual_fixes);
    cJSON_AddBoolToObject(root, "no_sprite_limits", c->no_sprite_limits);
    cJSON_AddBoolToObject(root, "display_perf_title", c->display_perf_title);
    cJSON_AddNumberToObject(root, "enable_msu", c->enable_msu);
    cJSON_AddBoolToObject(root, "resume_msu", c->resume_msu);
    cJSON_AddBoolToObject(root, "disable_frame_delay", c->disable_frame_delay);
    cJSON_AddNumberToObject(root, "msuvolume", c->msuvolume);
    cJSON_AddNumberToObject(root, "features0", c->features0);
    cJSON_AddNumberToObject(root, "break_pots_min_sword", c->break_pots_min_sword);

    add_str(root, "link_graphics", c->link_graphics);
    add_str(root, "shader", c->shader);
    add_str(root, "msu_path", c->msu_path);
    add_str(root, "language", c->language);

    // Keymap state (kbd + gamepad), driven by the shared tables.
    cJSON *kbd = cJSON_AddObjectToObject(root, "kbd");
    cJSON *pad = cJSON_AddObjectToObject(root, "gamepad");
    for (int i = 0; i < kNumArrays; i++) {
        add_str_array(kbd, kArrays[i].key, kArrays[i].kbd, kArrays[i].n);
        add_str_array(pad, kArrays[i].key, kArrays[i].pad, kArrays[i].n);
    }
    for (int i = 0; i < kNumScalars; i++) {
        add_str(kbd, kScalars[i].key, *kScalars[i].kbd);
        add_str(pad, kScalars[i].key, *kScalars[i].pad);
    }
    return root;
}

// Populate a Config + the keymap globals from JSON. Caller owns/zeroes Config.
// Returns the first key whose value doesn't fit its Config field, else NULL.
static const char *json_to_config(const cJSON *root, Config *c) {
    const char *bad = NULL;
    c->window_width = get_int(root, "window_width", 0, 65535, &bad);
    c->window_height = get_int(root, "window_height", 0, 65535, &bad);
    c->enhanced_mode7 = get_bool(root, "enhanced_mode7", false);
    c->new_renderer = get_bool(root, "new_renderer", false);
    c->ignore_aspect_ratio = get_bool(root, "ignore_aspect_ratio", false);
    c->fullscreen = (uint8)get_int(root, "fullscreen", 0, 255, &bad);
    c->window_scale = (uint8)get_int(root, "window_scale", 0, 255, &bad);
    c->enable_audio = get_bool(root, "enable_audio", false);
    c->linear_filtering = get_bool(root, "linear_filtering", false);
    c->output_method = (uint8)get_int(root, "output_method", 0, 255, &bad);
    c->audio_freq = (uint16)get_int(root, "audio_freq", 0, 65535, &bad);
    c->audio_channels = (uint8)get_int(root, "audio_channels", 0, 255, &bad);
    c->audio_samples = (uint16)get_int(root, "audio_samples", 0, 65535, &bad);
    c->autosave = get_bool(root, "autosave", false);
    c->extended_aspect_ratio = (uint8)get_int(root, "extended_aspect_ratio", 0, 255, &bad);
    c->custom_aspect_w = (uint16)get_int(root, "custom_aspect_w", 0, 65535, &bad);
    c->custom_aspect_h = (uint16)get_int(root, "custom_aspect_h", 0, 65535, &bad);
    c->extend_y = get_bool(root, "extend_y", false);
    c->unchanged_sprites = get_bool(root, "unchanged_sprites", false);
    c->no_visual_fixes = get_bool(root, "no_visual_fixes", false);
    c->no_sprite_limits = get_bool(root, "no_sprite_limits", false);
    c->display_perf_title = get_bool(root, "display_perf_title", false);
    c->enable_msu = (uint8)get_int(root, "enable_msu", 0, 255, &bad);
    c->resume_msu = get_bool(root, "resume_msu", false);
    c->disable_frame_delay = get_bool(root, "disable_frame_delay", false);
    c->msuvolume = (uint8)get_int(root, "msuvolume", 0, 255, &bad);
    c->features0 = get_u32(root, "features0", 0);
    c->break_pots_min_sword = (uint8)get_int(root, "break_pots_min_sword", 0, 255, &bad);

    c->link_graphics = get_str(root, "link_graphics");
    c->shader = get_str(root, "shader");
    c->msu_path = get_str(root, "msu_path");
    c->language = get_str(root, "language");

    const cJSON *kbd = cJSON_GetObjectItemCaseSensitive(root, "kbd");
    const cJSON *pad = cJSON_GetObjectItemCaseSensitive(root, "gamepad");
    keymap_free_all();
    for (int i = 0; i < kNumArrays; i++) {
        if (kbd) get_str_array(kbd, kArrays[i].key, kArrays[i].kbd, kArrays[i].n);
        if (pad) get_str_array(pad, kArrays[i].key, kArrays[i].pad, kArrays[i].n);
    }
    for (int i = 0; i < kNumScalars; i++) {
        if (kbd) *kScalars[i].kbd = get_str(kbd, kScalars[i].key);
        if (pad) *kScalars[i].pad = get_str(pad, kScalars[i].key);
    }
    return bad;
}

// json_to_config + ConfigWriter_Validate; fills err on failure.
static bool parse_and_validate(const cJSON *root, Config *cfg, char *err, size_t err_size) {
    ConfigWriter_InitDefaults(cfg);
    const char *bad = json_to_config(root, cfg);
    if (bad) {
        snprintf(err, err_size, "Value out of range: %s", bad);
        return false;
    }
    return ConfigWriter_Validate(cfg, err, err_size);
}

// Build the {"ok":bool,"error":...} result string for save/validate.
static char *make_result(bool ok, const char *error) {
    cJSON *r = cJSON_CreateObject();
    cJSON_AddBoolToObject(r, "ok", ok);
    if (error) cJSON_AddStringToObject(r, "error", error);
    else cJSON_AddNullToObject(r, "error");
    char *out = cJSON_PrintUnformatted(r);
    cJSON_Delete(r);
    return out;
}

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

char *LauncherFfi_LoadConfigJson(const char *ini_path) {
    if (!ini_path) return NULL;

    Config cfg;
    keymap_free_all();                 // start from a clean slate
    if (!ConfigReader_Read(ini_path, &cfg))  // also calls ConfigWriter_InitDefaults
        return NULL;

    cJSON *root = config_to_json(&cfg);
    char *out = cJSON_PrintUnformatted(root);

    cJSON_Delete(root);
    config_free_strings(&cfg);
    return out;
}

char *LauncherFfi_ValidateConfigJson(const char *config_json) {
    if (!config_json) return make_result(false, "null config json");

    cJSON *root = cJSON_Parse(config_json);
    if (!root) return make_result(false, "invalid json");

    Config cfg;
    char err[256] = "";
    bool ok = parse_and_validate(root, &cfg, err, sizeof(err));

    cJSON_Delete(root);
    config_free_strings(&cfg);
    return make_result(ok, ok ? NULL : err);
}

char *LauncherFfi_SaveConfigJson(const char *ini_path, const char *config_json) {
    if (!ini_path) return make_result(false, "null ini path");
    if (!config_json) return make_result(false, "null config json");

    cJSON *root = cJSON_Parse(config_json);
    if (!root) return make_result(false, "invalid json");

    Config cfg;
    char err[256] = "";
    bool ok = parse_and_validate(root, &cfg, err, sizeof(err));
    if (ok && !ConfigWriter_Write(ini_path, &cfg)) {
        ok = false;
        snprintf(err, sizeof(err), "failed to write %s", ini_path);
    }

    cJSON_Delete(root);
    config_free_strings(&cfg);
    return make_result(ok, ok ? NULL : err);
}

// ----------------------------------------------------------------------------
// restool asset workflow (ROM identify, DAT compile, DAT status)
// ----------------------------------------------------------------------------

static const char *restool_err_string(int rc) {
    switch (rc) {
        case RESTOOL_OK:              return NULL;
        case RESTOOL_ERR_ROM_LOAD:    return "could not load ROM file";
        case RESTOOL_ERR_ROM_INVALID: return "ROM is not a recognized A Link to the Past ROM";
        case RESTOOL_ERR_ROM_NOT_US:  return "base ROM must be the US version";
        case RESTOOL_ERR_EXTRACT:     return "asset extraction failed";
        case RESTOOL_ERR_WRITE:       return "could not write output file";
        case RESTOOL_ERR_DIALOGUE:    return "dialogue extraction failed";
        case RESTOOL_ERR_MEMORY:      return "out of memory";
        default:                      return "unknown error";
    }
}

char *LauncherFfi_IdentifyRom(const char *rom_path) {
    cJSON *r = cJSON_CreateObject();
    RestoolRomInfo info;
    memset(&info, 0, sizeof(info));
    bool read = rom_path && Restool_IdentifyRom(rom_path, &info);
    cJSON_AddBoolToObject(r, "read", read);
    cJSON_AddBoolToObject(r, "valid", read && info.valid);
    cJSON_AddStringToObject(r, "lang_code", read ? info.lang_code : "");
    cJSON_AddStringToObject(r, "lang_name", read ? info.lang_name : "");
    char *out = cJSON_PrintUnformatted(r);
    cJSON_Delete(r);
    return out;
}

// options_json: { "us_rom_path": str, "output_dir": str, "language_roms": [path,...] }
// Mirrors the GTK launcher workflow: extract dialogue from each non-US language
// ROM into output_dir, compile assets (US + languages) to output_dir/zelda3_assets.dat,
// then remove the temporary dialogue_{lang}.txt files.
char *LauncherFfi_CreateDat(const char *options_json) {
    if (!options_json) return make_result(false, "null options");
    cJSON *root = cJSON_Parse(options_json);
    if (!root) return make_result(false, "invalid json");

    char *us_rom = get_str(root, "us_rom_path");
    char *out_dir = get_str(root, "output_dir");
    if (!us_rom || !out_dir) {
        cJSON_Delete(root);
        free(us_rom); free(out_dir);
        return make_result(false, "us_rom_path and output_dir are required");
    }

    // Validate the base ROM is a usable US ROM up front for a clear error.
    RestoolRomInfo base;
    memset(&base, 0, sizeof(base));
    if (!Restool_IdentifyRom(us_rom, &base) || !base.valid) {
        cJSON_Delete(root);
        free(us_rom); free(out_dir);
        return make_result(false, "base ROM is not a recognized ROM");
    }

    // Extract dialogue for each valid, non-US language ROM; build the language list.
    char languages[256] = "";
    char extracted[16][16];   // lang codes we created dialogue files for (to clean up)
    int extracted_count = 0;
    const char *err = NULL;

    const cJSON *roms = cJSON_GetObjectItemCaseSensitive(root, "language_roms");
    if (cJSON_IsArray(roms)) {
        const cJSON *item;
        cJSON_ArrayForEach(item, roms) {
            if (!cJSON_IsString(item) || !item->valuestring || !item->valuestring[0]) continue;

            RestoolRomInfo li;
            memset(&li, 0, sizeof(li));
            if (!Restool_IdentifyRom(item->valuestring, &li) || !li.valid) continue;
            if (strcmp(li.lang_code, "us") == 0) continue;

            int rc = Restool_ExtractDialogue(item->valuestring, out_dir);
            if (rc != RESTOOL_OK) { err = restool_err_string(rc); break; }

            if (extracted_count < 16) {
                strncpy(extracted[extracted_count], li.lang_code, 15);
                extracted[extracted_count][15] = '\0';
                extracted_count++;
            }
            if (languages[0]) strncat(languages, ",", sizeof(languages) - strlen(languages) - 1);
            strncat(languages, li.lang_code, sizeof(languages) - strlen(languages) - 1);
        }
    }

    int compile_rc = RESTOOL_OK;
    if (!err) {
        char output_path[600];
        snprintf(output_path, sizeof(output_path), "%s/zelda3_assets.dat", out_dir);
        RestoolCompileOptions opt = {
            .us_rom_path = us_rom,
            .output_path = output_path,
            .languages = languages[0] ? languages : NULL,
            .dialogue_dir = out_dir,
            .sprites_from_png = false,
            .custom_sprites = false,
        };
        compile_rc = Restool_CompileAssetsEx(&opt);
        if (compile_rc != RESTOOL_OK) err = restool_err_string(compile_rc);
    }

    // Clean up the temporary dialogue files we created.
    for (int i = 0; i < extracted_count; i++) {
        char p[600];
        snprintf(p, sizeof(p), "%s/dialogue_%s.txt", out_dir, extracted[i]);
        remove(p);
    }

    bool ok = (err == NULL);
    cJSON *r = cJSON_CreateObject();
    cJSON_AddBoolToObject(r, "ok", ok);
    if (err) cJSON_AddStringToObject(r, "error", err);
    else cJSON_AddNullToObject(r, "error");
    // Report the languages now present in the DAT (us + extracted).
    cJSON *langs = cJSON_AddArrayToObject(r, "languages");
    if (ok) {
        char dat_langs[32][16];
        int n = Restool_GetDatLanguages(out_dir, dat_langs, 32);
        for (int i = 0; i < n; i++) cJSON_AddItemToArray(langs, cJSON_CreateString(dat_langs[i]));
    }
    char *result = cJSON_PrintUnformatted(r);

    cJSON_Delete(r);
    cJSON_Delete(root);
    free(us_rom);
    free(out_dir);
    return result;
}

// Report whether a DAT exists in dir and which languages it contains.
char *LauncherFfi_DatStatus(const char *dir) {
    cJSON *r = cJSON_CreateObject();
    bool exists = dir && Restool_DatFileExists(dir);
    cJSON_AddBoolToObject(r, "exists", exists);
    cJSON *langs = cJSON_AddArrayToObject(r, "languages");
    if (exists) {
        char dat_langs[32][16];
        int n = Restool_GetDatLanguages(dir, dat_langs, 32);
        for (int i = 0; i < n; i++) cJSON_AddItemToArray(langs, cJSON_CreateString(dat_langs[i]));
    }
    char *out = cJSON_PrintUnformatted(r);
    cJSON_Delete(r);
    return out;
}

// ----------------------------------------------------------------------------
// opus encoding (MSU .pcm -> .opuz) with progress, optional (HAVE_OPUS_ENCODER)
// ----------------------------------------------------------------------------
#ifdef HAVE_OPUS_ENCODER
#include "opus_encoder_lib.h"
#include <dirent.h>
#include <ctype.h>

// Returns the track number if `name` is "<prefix><digits>.pcm", else -1.
static int pcm_track_number(const char *name, const char *prefix) {
    size_t plen = strlen(prefix);
    if (strncmp(name, prefix, plen) != 0) return -1;
    const char *p = name + plen;
    if (!isdigit((unsigned char)*p)) return -1;
    char *end;
    long n = strtol(p, &end, 10);
    if (strcmp(end, ".pcm") != 0) return -1;
    return (int)n;
}

// Per-file opus callback context: forwards to the launcher progress callback,
// converting per-file progress into an overall fraction.
typedef struct {
    LauncherProgressCb cb;
    void *user;
    int index;
    int total;
} ProgressForward;

static bool opus_progress_forward(float progress, void *data) {
    ProgressForward *pf = (ProgressForward *)data;
    if (pf->cb) {
        float overall = ((float)pf->index + progress) / (float)pf->total;
        char msg[64];
        snprintf(msg, sizeof(msg), "Encoding track %d of %d", pf->index + 1, pf->total);
        pf->cb(msg, overall, pf->user);
    }
    return true;  // never cancel (cancellation not exposed across the FFI yet)
}

char *LauncherFfi_EncodeMsuToOpus(const char *folder, const char *prefix,
                                  LauncherProgressCb cb, void *user) {
    if (!folder || !prefix) return make_result(false, "folder and prefix are required");

    DIR *d = opendir(folder);
    if (!d) return make_result(false, "cannot open MSU folder");

    int tracks[512];
    int count = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && count < 512) {
        int t = pcm_track_number(e->d_name, prefix);
        if (t >= 0) tracks[count++] = t;
    }
    closedir(d);
    if (count == 0) return make_result(false, "no matching .pcm files found");

    int encoded = 0;
    const char *err = NULL;
    for (int i = 0; i < count; i++) {
        char in[1024], out_path[1024];
        snprintf(in, sizeof(in), "%s/%s%d.pcm", folder, prefix, tracks[i]);
        snprintf(out_path, sizeof(out_path), "%s/%s%d.opuz", folder, prefix, tracks[i]);

        ProgressForward pf = { .cb = cb, .user = user, .index = i, .total = count };
        OpusEncoderOptionsEx opts = {
            .bitrate = 128000,
            .has_repeat = OpusEncoder_TrackHasRepeat(tracks[i]),
            .callback = cb ? opus_progress_forward : NULL,
            .callback_data = cb ? &pf : NULL,
        };
        int rc = OpusEncoder_EncodeFileEx(in, out_path, &opts);
        if (rc == OPUS_ENC_OK) {
            encoded++;
        } else {
            err = OpusEncoder_GetErrorString(rc);
            break;
        }
    }

    cJSON *r = cJSON_CreateObject();
    cJSON_AddBoolToObject(r, "ok", err == NULL);
    if (err) cJSON_AddStringToObject(r, "error", err);
    else cJSON_AddNullToObject(r, "error");
    cJSON_AddNumberToObject(r, "encoded", encoded);
    cJSON_AddNumberToObject(r, "total", count);
    char *result = cJSON_PrintUnformatted(r);
    cJSON_Delete(r);
    return result;
}
#else
char *LauncherFfi_EncodeMsuToOpus(const char *folder, const char *prefix,
                                  LauncherProgressCb cb, void *user) {
    (void)folder; (void)prefix; (void)cb; (void)user;
    return make_result(false, "opus support was not compiled into this build");
}
#endif  // HAVE_OPUS_ENCODER

char *LauncherFfi_ResetDefaults(const char *ini_path) {
    if (!ini_path) return make_result(false, "null ini path");

    FILE *f = fopen(ini_path, "wb");
    if (!f) return make_result(false, "cannot open ini for writing");

    bool ok = fputs(kDefaultConfigIni, f) != EOF;
    if (fclose(f) != 0) ok = false;

    return make_result(ok, ok ? NULL : "failed to write default config");
}

void LauncherFfi_FreeString(char *s) {
    free(s);  // cJSON_Print* and strdup both use the default allocator
}
