#include "config_reader.h"
#include "config_reader_internal.h"
#include "config_writer.h"
#include "keymap_state.h"      // control-mapping globals (GTK-free)
#include "control_strings.h"   // LauncherUI_Parse*ControlString (GTK-free)
#include "../config.h"
#include "../game_features.h"
#include "../logging.h"
#include "../platform.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// Simple INI parser for launcher - reads values into Config struct
// Control mappings are stored in launcher_ui.c globals

// Make functions non-static when TEST_BUILD is defined for unit testing
#ifdef TEST_BUILD
#define STATIC_OR_TEST
#else
#define STATIC_OR_TEST static
#endif

STATIC_OR_TEST char* trim_whitespace(char *str) {
    // Trim leading
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;

    // Trim trailing
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    return str;
}

// ASCII case-insensitive equality (the game matches INI names with StringEqualsNoCase).
static bool str_ieq(const char *a, const char *b) {
    for (; *a && *b; a++, b++)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
    return *a == *b;
}

// Accepts the same truthy spellings as the game's ParseBool (1/true/yes/on, any case).
STATIC_OR_TEST int parse_bool(const char *value) {
    return str_ieq(value, "1") || str_ieq(value, "true") ||
           str_ieq(value, "yes") || str_ieq(value, "on");
}

// EnableMSU = false|true|deluxe|opuz|deluxe-opuz -> kMsuEnabled_* bits (mirrors config.c).
STATIC_OR_TEST int parse_msu_mode(const char *value) {
    if (str_ieq(value, "opuz")) return kMsuEnabled_Msu | kMsuEnabled_Opuz;
    if (str_ieq(value, "deluxe")) return kMsuEnabled_Msu | kMsuEnabled_MsuDeluxe;
    if (str_ieq(value, "deluxe-opuz")) return kMsuEnabled_Msu | kMsuEnabled_MsuDeluxe | kMsuEnabled_Opuz;
    return parse_bool(value) ? kMsuEnabled_Msu : 0;
}

STATIC_OR_TEST int parse_int(const char *value) {
    return atoi(value);
}

STATIC_OR_TEST char* parse_string(const char *value) {
    if (!value || !*value) return NULL;
    return strdup(value);
}

// Helper to update a string variable (free old, set new)
STATIC_OR_TEST void update_string(char **dest, const char *value) {
    if (*dest) free(*dest);
    *dest = (value && *value) ? strdup(value) : NULL;
}

STATIC_OR_TEST int parse_aspect_ratio(const char *value, int *custom_w, int *custom_h) {
    // Map aspect ratio strings to enum values
    // Check for known presets first
    if (strstr(value, "4:3")) return 0;
    if (strstr(value, "16:9") && !strstr(value, "16:10")) return 1;
    if (strstr(value, "16:10")) return 2;
    if (strstr(value, "18:9")) return 3;
    if (strstr(value, "original")) return 0;

    // Try to parse as custom W:H ratio
    int w, h;
    // Find a W:H pattern in the value string
    const char *p = value;
    while (*p) {
        if (sscanf(p, "%d:%d", &w, &h) == 2 && h > 0) {
            *custom_w = w;
            *custom_h = h;
            return 4;  // Custom
        }
        p++;
    }

    return 0;  // default to 4:3
}

STATIC_OR_TEST int parse_output_method(const char *value) {
    if (str_ieq(value, "SDL")) return kOutputMethod_SDL;
    if (str_ieq(value, "SDL-Software")) return kOutputMethod_SDLSoftware;
    if (str_ieq(value, "OpenGL")) return kOutputMethod_OpenGL;
    if (str_ieq(value, "OpenGL ES")) return kOutputMethod_OpenGL_ES;
    if (str_ieq(value, "Vulkan")) return kOutputMethod_Vulkan;
    return kOutputMethod_SDL;  // default to SDL
}

bool ConfigReader_Read(const char *path, Config *config) {
    size_t length;
    char *data = (char*)Platform_ReadWholeFile(path, &length);
    if (!data) {
        LogError("ConfigReader: Failed to read %s", path);
        return false;
    }

    // Start with defaults
    ConfigWriter_InitDefaults(config);

    char *line = data;
    char *next_line;
    char current_section[64] = "";

    // Parse line by line
    while (line && *line) {
        // Find next line
        next_line = strchr(line, '\n');
        if (next_line) {
            *next_line = '\0';
            next_line++;
        }

        // Trim whitespace
        line = trim_whitespace(line);

        // Skip empty lines and comments
        if (*line == '\0' || *line == '#' || *line == ';') {
            line = next_line;
            continue;
        }

        // Section header
        if (*line == '[') {
            char *end = strchr(line, ']');
            if (end) {
                *end = '\0';
                strncpy(current_section, line + 1, sizeof(current_section) - 1);
                current_section[sizeof(current_section) - 1] = '\0';
            }
            line = next_line;
            continue;
        }

        // Key = Value
        char *equals = strchr(line, '=');
        if (!equals) {
            line = next_line;
            continue;
        }

        *equals = '\0';
        char *key = trim_whitespace(line);
        char *value = trim_whitespace(equals + 1);

        // Parse based on section
        if (str_ieq(current_section, "General")) {
            if (str_ieq(key, "Autosave")) config->autosave = parse_bool(value);
            else if (str_ieq(key, "DisplayPerfInTitle")) config->display_perf_title = parse_bool(value);
            else if (str_ieq(key, "DisableFrameDelay")) config->disable_frame_delay = parse_bool(value);
            else if (str_ieq(key, "ExtendedAspectRatio")) {
                int custom_w = 0, custom_h = 0;
                config->extended_aspect_ratio = parse_aspect_ratio(value, &custom_w, &custom_h);
                config->custom_aspect_w = custom_w;
                config->custom_aspect_h = custom_h;
                config->extend_y = strstr(value, "extend_y") != NULL;
                config->unchanged_sprites = strstr(value, "unchanged_sprites") != NULL;
                config->no_visual_fixes = strstr(value, "no_visual_fixes") != NULL;
                config->no_camera_clamp = strstr(value, "no_camera_clamp") != NULL;
            }
            else if (str_ieq(key, "Language")) config->language = parse_string(value);
        }
        else if (str_ieq(current_section, "Graphics")) {
            if (str_ieq(key, "WindowSize")) {
                if (strstr(value, "Auto")) {
                    config->window_width = 0;
                    config->window_height = 0;
                } else if (strstr(value, "Fullscreen")) {
                    config->fullscreen = 1;
                } else {
                    // Try to parse "WIDTHxHEIGHT"
                    int w, h;
                    if (sscanf(value, "%dx%d", &w, &h) == 2) {
                        config->window_width = w;
                        config->window_height = h;
                    } else {
                        // Try scale (e.g., "2x")
                        int scale;
                        if (sscanf(value, "%dx", &scale) == 1) {
                            config->window_scale = scale;
                            config->window_width = 256 * scale;
                            config->window_height = 224 * scale;
                        }
                    }
                }
            }
            else if (str_ieq(key, "WindowScale")) config->window_scale = parse_int(value);
            else if (str_ieq(key, "Fullscreen")) config->fullscreen = parse_int(value);
            else if (str_ieq(key, "IgnoreAspectRatio")) config->ignore_aspect_ratio = parse_bool(value);
            else if (str_ieq(key, "OutputMethod")) config->output_method = parse_output_method(value);
            else if (str_ieq(key, "LinearFiltering")) config->linear_filtering = parse_bool(value);
            else if (str_ieq(key, "NewRenderer")) config->new_renderer = parse_bool(value);
            else if (str_ieq(key, "EnhancedMode7")) config->enhanced_mode7 = parse_bool(value);
            else if (str_ieq(key, "NoSpriteLimits")) config->no_sprite_limits = parse_bool(value);
            else if (str_ieq(key, "DimFlashes")) {
                if (parse_bool(value)) config->features0 |= kFeatures0_DimFlashes;
            }
            else if (str_ieq(key, "LinkGraphics")) config->link_graphics = parse_string(value);
            else if (str_ieq(key, "Shader")) config->shader = parse_string(value);
        }
        else if (str_ieq(current_section, "Sound")) {
            if (str_ieq(key, "EnableAudio")) config->enable_audio = parse_bool(value);
            else if (str_ieq(key, "AudioFreq")) config->audio_freq = parse_int(value);
            else if (str_ieq(key, "AudioChannels")) config->audio_channels = parse_int(value);
            else if (str_ieq(key, "AudioSamples")) config->audio_samples = parse_int(value);
            else if (str_ieq(key, "EnableMSU")) config->enable_msu = parse_msu_mode(value);
            else if (str_ieq(key, "ResumeMSU")) config->resume_msu = parse_bool(value);
            else if (str_ieq(key, "MSUVolume")) config->msuvolume = parse_int(value);
            else if (str_ieq(key, "MSUPath")) config->msu_path = parse_string(value);
        }
        else if (str_ieq(current_section, "Features")) {
            // Parse individual feature flags (matches config_writer output)
            int flag_value = parse_bool(value);
            if (str_ieq(key, "ItemSwitchLR") && flag_value)
                config->features0 |= kFeatures0_SwitchLR;
            else if (str_ieq(key, "ItemSwitchLRLimit") && flag_value)
                config->features0 |= kFeatures0_SwitchLRLimit;
            else if (str_ieq(key, "TurnWhileDashing") && flag_value)
                config->features0 |= kFeatures0_TurnWhileDashing;
            else if (str_ieq(key, "MirrorToDarkworld") && flag_value)
                config->features0 |= kFeatures0_MirrorToDarkworld;
            else if (str_ieq(key, "CollectItemsWithSword") && flag_value)
                config->features0 |= kFeatures0_CollectItemsWithSword;
            else if (str_ieq(key, "BreakPotsWithSword")) {
                int v = atoi(value);
                config->break_pots_min_sword = (v < 0) ? 0 : (v > 4) ? 4 : v;
            }
            else if (str_ieq(key, "MoreActiveBombs") && flag_value)
                config->features0 |= kFeatures0_MoreActiveBombs;
            else if (str_ieq(key, "CarryMoreRupees") && flag_value)
                config->features0 |= kFeatures0_CarryMoreRupees;
            else if (str_ieq(key, "CancelBirdTravel") && flag_value)
                config->features0 |= kFeatures0_CancelBirdTravel;
            else if (str_ieq(key, "DisableLowHealthBeep") && flag_value)
                config->features0 |= kFeatures0_DisableLowHealthBeep;
            else if (str_ieq(key, "SkipIntroOnKeypress") && flag_value)
                config->features0 |= kFeatures0_SkipIntroOnKeypress;
            else if (str_ieq(key, "ShowMaxItemsInYellow") && flag_value)
                config->features0 |= kFeatures0_ShowMaxItemsInYellow;
            else if (str_ieq(key, "MiscBugFixes") && flag_value)
                config->features0 |= kFeatures0_MiscBugFixes;
            else if (str_ieq(key, "GameChangingBugFixes") && flag_value)
                config->features0 |= kFeatures0_GameChangingBugFixes;
            else if (str_ieq(key, "Pokemode") && flag_value)
                config->features0 |= kFeatures0_Pokemode;
            else if (str_ieq(key, "PrincessZeldaHelps") && flag_value)
                config->features0 |= kFeatures0_PrincessZeldaHelps;
        }
        else if (str_ieq(current_section, "KeyMap")) {
            if (str_ieq(key, "Controls")) {
                LauncherUI_ParseControlString(value, g_kbd_controls);
            }
            else if (str_ieq(key, "Load")) {
                LauncherUI_ParseControlString(value, g_kbd_load);
            }
            else if (str_ieq(key, "Save")) {
                LauncherUI_ParseControlString(value, g_kbd_save);
            }
            else if (str_ieq(key, "Replay")) {
                LauncherUI_ParseControlString(value, g_kbd_replay);
            }
            else if (str_ieq(key, "CheatLife")) update_string(&g_kbd_cheat_life, value);
            else if (str_ieq(key, "CheatKeys")) update_string(&g_kbd_cheat_keys, value);
            else if (str_ieq(key, "CheatEquipment") || str_ieq(key, "CheatWalkThroughWalls"))
                update_string(&g_kbd_cheat_walkthrough, value);
            else if (str_ieq(key, "ClearKeyLog")) update_string(&g_kbd_clear_keylog, value);
            else if (str_ieq(key, "StopReplay")) update_string(&g_kbd_stop_replay, value);
            else if (str_ieq(key, "Fullscreen")) update_string(&g_kbd_fullscreen, value);
            else if (str_ieq(key, "Reset")) update_string(&g_kbd_reset, value);
            else if (str_ieq(key, "PauseDimmed")) update_string(&g_kbd_pause_dimmed, value);
            else if (str_ieq(key, "Pause")) update_string(&g_kbd_pause, value);
            else if (str_ieq(key, "Turbo")) update_string(&g_kbd_turbo, value);
            else if (str_ieq(key, "ReplayTurbo")) update_string(&g_kbd_replay_turbo, value);
            else if (str_ieq(key, "WindowBigger")) update_string(&g_kbd_window_bigger, value);
            else if (str_ieq(key, "WindowSmaller")) update_string(&g_kbd_window_smaller, value);
            else if (str_ieq(key, "VolumeUp")) update_string(&g_kbd_volume_up, value);
            else if (str_ieq(key, "VolumeDown")) update_string(&g_kbd_volume_down, value);
            else if (str_ieq(key, "DisplayPerf")) update_string(&g_kbd_display_perf, value);
            else if (str_ieq(key, "ToggleRenderer")) update_string(&g_kbd_toggle_renderer, value);
        }
        else if (str_ieq(current_section, "GamepadMap")) {
            if (str_ieq(key, "Controls")) {
                LauncherUI_ParseGamepadControlString(value, g_gamepad_controls);
            }
            else if (str_ieq(key, "Load")) {
                LauncherUI_ParseGamepadControlString(value, g_gamepad_load);
            }
            else if (str_ieq(key, "Save")) {
                LauncherUI_ParseGamepadControlString(value, g_gamepad_save);
            }
            else if (str_ieq(key, "Replay")) {
                LauncherUI_ParseGamepadControlString(value, g_gamepad_replay);
            }
            else if (str_ieq(key, "CheatLife")) update_string(&g_gamepad_cheat_life, value);
            else if (str_ieq(key, "CheatKeys")) update_string(&g_gamepad_cheat_keys, value);
            else if (str_ieq(key, "CheatEquipment") || str_ieq(key, "CheatWalkThroughWalls"))
                update_string(&g_gamepad_cheat_walkthrough, value);
            else if (str_ieq(key, "ClearKeyLog")) update_string(&g_gamepad_clear_keylog, value);
            else if (str_ieq(key, "StopReplay")) update_string(&g_gamepad_stop_replay, value);
            else if (str_ieq(key, "Fullscreen")) update_string(&g_gamepad_fullscreen, value);
            else if (str_ieq(key, "Reset")) update_string(&g_gamepad_reset, value);
            else if (str_ieq(key, "PauseDimmed")) update_string(&g_gamepad_pause_dimmed, value);
            else if (str_ieq(key, "Pause")) update_string(&g_gamepad_pause, value);
            else if (str_ieq(key, "Turbo")) update_string(&g_gamepad_turbo, value);
            else if (str_ieq(key, "ReplayTurbo")) update_string(&g_gamepad_replay_turbo, value);
            else if (str_ieq(key, "WindowBigger")) update_string(&g_gamepad_window_bigger, value);
            else if (str_ieq(key, "WindowSmaller")) update_string(&g_gamepad_window_smaller, value);
            else if (str_ieq(key, "VolumeUp")) update_string(&g_gamepad_volume_up, value);
            else if (str_ieq(key, "VolumeDown")) update_string(&g_gamepad_volume_down, value);
            else if (str_ieq(key, "DisplayPerf")) update_string(&g_gamepad_display_perf, value);
            else if (str_ieq(key, "ToggleRenderer")) update_string(&g_gamepad_toggle_renderer, value);
        }

        line = next_line;
    }

    free(data);
    LogInfo("ConfigReader: Successfully read config from %s", path);
    return true;
}
