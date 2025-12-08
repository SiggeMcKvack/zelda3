#include "config_reader.h"
#include "config_reader_internal.h"
#include "config_writer.h"
#include "launcher_ui.h"
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

STATIC_OR_TEST int parse_bool(const char *value) {
    return (strcmp(value, "1") == 0 ||
            strcmp(value, "true") == 0 ||
            strcmp(value, "True") == 0);
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
    if (strcmp(value, "SDL") == 0) return kOutputMethod_SDL;
    if (strcmp(value, "SDL-Software") == 0) return kOutputMethod_SDLSoftware;
    if (strcmp(value, "OpenGL") == 0) return kOutputMethod_OpenGL;
    if (strcmp(value, "OpenGL ES") == 0) return kOutputMethod_OpenGL_ES;
    if (strcmp(value, "Vulkan") == 0) return kOutputMethod_Vulkan;
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
        if (strcmp(current_section, "General") == 0) {
            if (strcmp(key, "Autosave") == 0) config->autosave = parse_bool(value);
            else if (strcmp(key, "DisplayPerfInTitle") == 0) config->display_perf_title = parse_bool(value);
            else if (strcmp(key, "DisableFrameDelay") == 0) config->disable_frame_delay = parse_bool(value);
            else if (strcmp(key, "ExtendedAspectRatio") == 0) {
                int custom_w = 0, custom_h = 0;
                config->extended_aspect_ratio = parse_aspect_ratio(value, &custom_w, &custom_h);
                config->custom_aspect_w = custom_w;
                config->custom_aspect_h = custom_h;
                config->extend_y = strstr(value, "extend_y") != NULL;
            }
            else if (strcmp(key, "Language") == 0) config->language = parse_string(value);
        }
        else if (strcmp(current_section, "Graphics") == 0) {
            if (strcmp(key, "WindowSize") == 0) {
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
            else if (strcmp(key, "WindowScale") == 0) config->window_scale = parse_int(value);
            else if (strcmp(key, "Fullscreen") == 0) config->fullscreen = parse_int(value);
            else if (strcmp(key, "IgnoreAspectRatio") == 0) config->ignore_aspect_ratio = parse_bool(value);
            else if (strcmp(key, "OutputMethod") == 0) config->output_method = parse_output_method(value);
            else if (strcmp(key, "LinearFiltering") == 0) config->linear_filtering = parse_bool(value);
            else if (strcmp(key, "NewRenderer") == 0) config->new_renderer = parse_bool(value);
            else if (strcmp(key, "EnhancedMode7") == 0) config->enhanced_mode7 = parse_bool(value);
            else if (strcmp(key, "NoSpriteLimits") == 0) config->no_sprite_limits = parse_bool(value);
            else if (strcmp(key, "DimFlashes") == 0) {
                if (parse_bool(value)) config->features0 |= kFeatures0_DimFlashes;
            }
            else if (strcmp(key, "LinkGraphics") == 0) config->link_graphics = parse_string(value);
            else if (strcmp(key, "Shader") == 0) config->shader = parse_string(value);
        }
        else if (strcmp(current_section, "Sound") == 0) {
            if (strcmp(key, "EnableAudio") == 0) config->enable_audio = parse_bool(value);
            else if (strcmp(key, "AudioFreq") == 0) config->audio_freq = parse_int(value);
            else if (strcmp(key, "AudioChannels") == 0) config->audio_channels = parse_int(value);
            else if (strcmp(key, "AudioSamples") == 0) config->audio_samples = parse_int(value);
            else if (strcmp(key, "EnableMSU") == 0) config->enable_msu = parse_bool(value);
            else if (strcmp(key, "ResumeMSU") == 0) config->resume_msu = parse_bool(value);
            else if (strcmp(key, "MSUVolume") == 0) config->msuvolume = parse_int(value);
            else if (strcmp(key, "MSUPath") == 0) config->msu_path = parse_string(value);
        }
        else if (strcmp(current_section, "Features") == 0) {
            // Parse individual feature flags (matches config_writer output)
            int flag_value = parse_bool(value);
            if (strcmp(key, "ItemSwitchLR") == 0 && flag_value)
                config->features0 |= kFeatures0_SwitchLR;
            else if (strcmp(key, "ItemSwitchLRLimit") == 0 && flag_value)
                config->features0 |= kFeatures0_SwitchLRLimit;
            else if (strcmp(key, "TurnWhileDashing") == 0 && flag_value)
                config->features0 |= kFeatures0_TurnWhileDashing;
            else if (strcmp(key, "MirrorToDarkworld") == 0 && flag_value)
                config->features0 |= kFeatures0_MirrorToDarkworld;
            else if (strcmp(key, "CollectItemsWithSword") == 0 && flag_value)
                config->features0 |= kFeatures0_CollectItemsWithSword;
            else if (strcmp(key, "BreakPotsWithSword") == 0) {
                int v = atoi(value);
                config->break_pots_min_sword = (v < 0) ? 0 : (v > 4) ? 4 : v;
            }
            else if (strcmp(key, "MoreActiveBombs") == 0 && flag_value)
                config->features0 |= kFeatures0_MoreActiveBombs;
            else if (strcmp(key, "CarryMoreRupees") == 0 && flag_value)
                config->features0 |= kFeatures0_CarryMoreRupees;
            else if (strcmp(key, "CancelBirdTravel") == 0 && flag_value)
                config->features0 |= kFeatures0_CancelBirdTravel;
            else if (strcmp(key, "DisableLowHealthBeep") == 0 && flag_value)
                config->features0 |= kFeatures0_DisableLowHealthBeep;
            else if (strcmp(key, "SkipIntroOnKeypress") == 0 && flag_value)
                config->features0 |= kFeatures0_SkipIntroOnKeypress;
            else if (strcmp(key, "ShowMaxItemsInYellow") == 0 && flag_value)
                config->features0 |= kFeatures0_ShowMaxItemsInYellow;
            else if (strcmp(key, "MiscBugFixes") == 0 && flag_value)
                config->features0 |= kFeatures0_MiscBugFixes;
            else if (strcmp(key, "GameChangingBugFixes") == 0 && flag_value)
                config->features0 |= kFeatures0_GameChangingBugFixes;
            else if (strcmp(key, "Pokemode") == 0 && flag_value)
                config->features0 |= kFeatures0_Pokemode;
            else if (strcmp(key, "PrincessZeldaHelps") == 0 && flag_value)
                config->features0 |= kFeatures0_PrincessZeldaHelps;
        }
        else if (strcmp(current_section, "KeyMap") == 0) {
            if (strcmp(key, "Controls") == 0) {
                LauncherUI_ParseControlString(value, g_kbd_controls);
            }
            else if (strcmp(key, "Load") == 0) {
                LauncherUI_ParseControlString(value, g_kbd_load);
            }
            else if (strcmp(key, "Save") == 0) {
                LauncherUI_ParseControlString(value, g_kbd_save);
            }
            else if (strcmp(key, "Replay") == 0) {
                LauncherUI_ParseControlString(value, g_kbd_replay);
            }
            else if (strcmp(key, "CheatLife") == 0) update_string(&g_kbd_cheat_life, value);
            else if (strcmp(key, "CheatKeys") == 0) update_string(&g_kbd_cheat_keys, value);
            else if (strcmp(key, "CheatWalkThroughWalls") == 0) update_string(&g_kbd_cheat_walkthrough, value);
            else if (strcmp(key, "ClearKeyLog") == 0) update_string(&g_kbd_clear_keylog, value);
            else if (strcmp(key, "StopReplay") == 0) update_string(&g_kbd_stop_replay, value);
            else if (strcmp(key, "Fullscreen") == 0) update_string(&g_kbd_fullscreen, value);
            else if (strcmp(key, "Reset") == 0) update_string(&g_kbd_reset, value);
            else if (strcmp(key, "PauseDimmed") == 0) update_string(&g_kbd_pause_dimmed, value);
            else if (strcmp(key, "Pause") == 0) update_string(&g_kbd_pause, value);
            else if (strcmp(key, "Turbo") == 0) update_string(&g_kbd_turbo, value);
            else if (strcmp(key, "ReplayTurbo") == 0) update_string(&g_kbd_replay_turbo, value);
            else if (strcmp(key, "WindowBigger") == 0) update_string(&g_kbd_window_bigger, value);
            else if (strcmp(key, "WindowSmaller") == 0) update_string(&g_kbd_window_smaller, value);
            else if (strcmp(key, "VolumeUp") == 0) update_string(&g_kbd_volume_up, value);
            else if (strcmp(key, "VolumeDown") == 0) update_string(&g_kbd_volume_down, value);
            else if (strcmp(key, "DisplayPerf") == 0) update_string(&g_kbd_display_perf, value);
            else if (strcmp(key, "ToggleRenderer") == 0) update_string(&g_kbd_toggle_renderer, value);
        }
        else if (strcmp(current_section, "GamepadMap") == 0) {
            if (strcmp(key, "Controls") == 0) {
                LauncherUI_ParseGamepadControlString(value, g_gamepad_controls);
            }
            else if (strcmp(key, "Load") == 0) {
                LauncherUI_ParseGamepadControlString(value, g_gamepad_load);
            }
            else if (strcmp(key, "Save") == 0) {
                LauncherUI_ParseGamepadControlString(value, g_gamepad_save);
            }
            else if (strcmp(key, "Replay") == 0) {
                LauncherUI_ParseGamepadControlString(value, g_gamepad_replay);
            }
            else if (strcmp(key, "CheatLife") == 0) update_string(&g_gamepad_cheat_life, value);
            else if (strcmp(key, "CheatKeys") == 0) update_string(&g_gamepad_cheat_keys, value);
            else if (strcmp(key, "CheatEquipment") == 0 || strcmp(key, "CheatWalkThroughWalls") == 0)
                update_string(&g_gamepad_cheat_walkthrough, value);
            else if (strcmp(key, "ClearKeyLog") == 0) update_string(&g_gamepad_clear_keylog, value);
            else if (strcmp(key, "StopReplay") == 0) update_string(&g_gamepad_stop_replay, value);
            else if (strcmp(key, "Fullscreen") == 0) update_string(&g_gamepad_fullscreen, value);
            else if (strcmp(key, "Reset") == 0) update_string(&g_gamepad_reset, value);
            else if (strcmp(key, "PauseDimmed") == 0) update_string(&g_gamepad_pause_dimmed, value);
            else if (strcmp(key, "Pause") == 0) update_string(&g_gamepad_pause, value);
            else if (strcmp(key, "Turbo") == 0) update_string(&g_gamepad_turbo, value);
            else if (strcmp(key, "ReplayTurbo") == 0) update_string(&g_gamepad_replay_turbo, value);
            else if (strcmp(key, "WindowBigger") == 0) update_string(&g_gamepad_window_bigger, value);
            else if (strcmp(key, "WindowSmaller") == 0) update_string(&g_gamepad_window_smaller, value);
            else if (strcmp(key, "VolumeUp") == 0) update_string(&g_gamepad_volume_up, value);
            else if (strcmp(key, "VolumeDown") == 0) update_string(&g_gamepad_volume_down, value);
            else if (strcmp(key, "DisplayPerf") == 0) update_string(&g_gamepad_display_perf, value);
            else if (strcmp(key, "ToggleRenderer") == 0) update_string(&g_gamepad_toggle_renderer, value);
        }

        line = next_line;
    }

    free(data);
    LogInfo("ConfigReader: Successfully read config from %s", path);
    return true;
}
