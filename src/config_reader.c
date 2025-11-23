#include "config_reader.h"
#include "config_writer.h"
#include "launcher_ui.h"
#include "config.h"
#include "features.h"
#include "logging.h"
#include "platform.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// Simple INI parser for launcher - reads values into Config struct
// Control mappings are stored in launcher_ui.c globals

static char* trim_whitespace(char *str) {
    // Trim leading
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;

    // Trim trailing
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    return str;
}

static int parse_bool(const char *value) {
    return (strcmp(value, "1") == 0 ||
            strcmp(value, "true") == 0 ||
            strcmp(value, "True") == 0);
}

static int parse_int(const char *value) {
    return atoi(value);
}

static char* parse_string(const char *value) {
    if (!value || !*value) return NULL;
    return strdup(value);
}

static int parse_aspect_ratio(const char *value) {
    // Map aspect ratio strings to enum values
    if (strstr(value, "16:9")) return 1;
    if (strstr(value, "16:10")) return 2;
    if (strstr(value, "4:3")) return 3;
    if (strstr(value, "original")) return 0;
    return 0;  // default to original
}

static int parse_output_method(const char *value) {
    if (strcmp(value, "SDL") == 0) return 0;
    if (strcmp(value, "OpenGL") == 0) return 1;
    if (strcmp(value, "OpenGL ES") == 0) return 2;
    if (strcmp(value, "Vulkan") == 0) return 3;
    return 0;  // default to SDL
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
            else if (strcmp(key, "ExtendedAspectRatio") == 0) config->extended_aspect_ratio = parse_aspect_ratio(value);
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
            else if (strcmp(key, "Fullscreen") == 0) config->fullscreen = parse_bool(value);
            else if (strcmp(key, "IgnoreAspectRatio") == 0) config->ignore_aspect_ratio = parse_bool(value);
            else if (strcmp(key, "OutputMethod") == 0) config->output_method = parse_output_method(value);
            else if (strcmp(key, "LinearFiltering") == 0) config->linear_filtering = parse_bool(value);
            else if (strcmp(key, "NewRenderer") == 0) config->new_renderer = parse_bool(value);
            else if (strcmp(key, "EnhancedMode7") == 0) config->enhanced_mode7 = parse_bool(value);
            else if (strcmp(key, "NoSpriteLimits") == 0) config->no_sprite_limits = parse_bool(value);
        }
        else if (strcmp(current_section, "Sound") == 0) {
            if (strcmp(key, "EnableAudio") == 0) config->enable_audio = parse_bool(value);
            else if (strcmp(key, "AudioFrequency") == 0) config->audio_freq = parse_int(value);
            else if (strcmp(key, "AudioChannels") == 0) config->audio_channels = parse_int(value);
            else if (strcmp(key, "AudioSamples") == 0) config->audio_samples = parse_int(value);
            else if (strcmp(key, "EnableMSU") == 0) config->enable_msu = parse_bool(value);
            else if (strcmp(key, "ResumeMSU") == 0) config->resume_msu = parse_bool(value);
            else if (strcmp(key, "MSUVolume") == 0) config->msuvolume = parse_int(value);
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
            else if (strcmp(key, "BreakPotsWithSword") == 0 && flag_value)
                config->features0 |= kFeatures0_BreakPotsWithSword;
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
        else if (strcmp(current_section, "Paths") == 0) {
            if (strcmp(key, "LinkGraphics") == 0) config->link_graphics = parse_string(value);
            else if (strcmp(key, "Shader") == 0) config->shader = parse_string(value);
            else if (strcmp(key, "MSUPath") == 0) config->msu_path = parse_string(value);
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
            else if (strcmp(key, "CheatLife") == 0) {
                if (g_kbd_cheat_life) free(g_kbd_cheat_life);
                g_kbd_cheat_life = strdup(value);
            }
            else if (strcmp(key, "CheatKeys") == 0) {
                if (g_kbd_cheat_keys) free(g_kbd_cheat_keys);
                g_kbd_cheat_keys = strdup(value);
            }
            else if (strcmp(key, "CheatWalkThroughWalls") == 0) {
                if (g_kbd_cheat_walkthrough) free(g_kbd_cheat_walkthrough);
                g_kbd_cheat_walkthrough = strdup(value);
            }
            else if (strcmp(key, "ClearKeyLog") == 0) {
                if (g_kbd_clear_keylog) free(g_kbd_clear_keylog);
                g_kbd_clear_keylog = strdup(value);
            }
            else if (strcmp(key, "StopReplay") == 0) {
                if (g_kbd_stop_replay) free(g_kbd_stop_replay);
                g_kbd_stop_replay = strdup(value);
            }
            else if (strcmp(key, "Fullscreen") == 0) {
                if (g_kbd_fullscreen) free(g_kbd_fullscreen);
                g_kbd_fullscreen = strdup(value);
            }
            else if (strcmp(key, "Reset") == 0) {
                if (g_kbd_reset) free(g_kbd_reset);
                g_kbd_reset = strdup(value);
            }
            else if (strcmp(key, "PauseDimmed") == 0) {
                if (g_kbd_pause_dimmed) free(g_kbd_pause_dimmed);
                g_kbd_pause_dimmed = strdup(value);
            }
            else if (strcmp(key, "Pause") == 0) {
                if (g_kbd_pause) free(g_kbd_pause);
                g_kbd_pause = strdup(value);
            }
            else if (strcmp(key, "Turbo") == 0) {
                if (g_kbd_turbo) free(g_kbd_turbo);
                g_kbd_turbo = strdup(value);
            }
            else if (strcmp(key, "ReplayTurbo") == 0) {
                if (g_kbd_replay_turbo) free(g_kbd_replay_turbo);
                g_kbd_replay_turbo = strdup(value);
            }
            else if (strcmp(key, "WindowBigger") == 0) {
                if (g_kbd_window_bigger) free(g_kbd_window_bigger);
                g_kbd_window_bigger = strdup(value);
            }
            else if (strcmp(key, "WindowSmaller") == 0) {
                if (g_kbd_window_smaller) free(g_kbd_window_smaller);
                g_kbd_window_smaller = strdup(value);
            }
            else if (strcmp(key, "VolumeUp") == 0) {
                if (g_kbd_volume_up) free(g_kbd_volume_up);
                g_kbd_volume_up = strdup(value);
            }
            else if (strcmp(key, "VolumeDown") == 0) {
                if (g_kbd_volume_down) free(g_kbd_volume_down);
                g_kbd_volume_down = strdup(value);
            }
        }
        else if (strcmp(current_section, "GamepadMap") == 0) {
            if (strcmp(key, "Controls") == 0) {
                LauncherUI_ParseGamepadControlString(value, g_gamepad_controls);
            }
            else if (strcmp(key, "Save") == 0) {
                if (g_gamepad_save) free(g_gamepad_save);
                g_gamepad_save = strdup(value);
            }
            else if (strcmp(key, "Load") == 0) {
                if (g_gamepad_load) free(g_gamepad_load);
                g_gamepad_load = strdup(value);
            }
        }

        line = next_line;
    }

    free(data);
    LogInfo("ConfigReader: Successfully read config from %s", path);
    return true;
}
