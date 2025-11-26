#include "launcher_ui.h"
#include "dat_reader.h"
#include "../config.h"
#include "../features.h"
#include "launcher_gamepad.h"
#include "../logging.h"
#include <gtk/gtk.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#ifdef __linux__
#include <unistd.h>
#endif

// UI Constants (NUM_SNES_BUTTONS and NUM_SAVE_SLOTS defined in launcher_ui.h)
#define NUM_STATE_TYPES         3   // Load, Save, Replay
#define NUM_CHEAT_KEYS          3
#define NUM_SYSTEM_KEYS         14

#define BUTTON_WIDTH            150
#define BUTTON_HEIGHT           35
#define CLEAR_BUTTON_WIDTH      80

#define LAUNCHER_DEFAULT_WIDTH  700
#define LAUNCHER_DEFAULT_HEIGHT 550

// Control mappings (12 SNES controls)
char *g_kbd_controls[12] = {NULL};
char *g_gamepad_controls[12] = {NULL};

// Keyboard save states (10 slots each)
char *g_kbd_load[10] = {NULL};
char *g_kbd_save[10] = {NULL};
char *g_kbd_replay[10] = {NULL};

// Keyboard cheats
char *g_kbd_cheat_life = NULL;
char *g_kbd_cheat_keys = NULL;
char *g_kbd_cheat_walkthrough = NULL;

// Keyboard system controls
char *g_kbd_clear_keylog = NULL;
char *g_kbd_stop_replay = NULL;
char *g_kbd_fullscreen = NULL;
char *g_kbd_reset = NULL;
char *g_kbd_pause_dimmed = NULL;
char *g_kbd_pause = NULL;
char *g_kbd_turbo = NULL;
char *g_kbd_replay_turbo = NULL;
char *g_kbd_window_bigger = NULL;
char *g_kbd_window_smaller = NULL;
char *g_kbd_volume_up = NULL;
char *g_kbd_volume_down = NULL;
char *g_kbd_display_perf = NULL;
char *g_kbd_toggle_renderer = NULL;

// Gamepad save states (10 slots each)
char *g_gamepad_load[10] = {NULL};
char *g_gamepad_save[10] = {NULL};
char *g_gamepad_replay[10] = {NULL};

// Gamepad cheats
char *g_gamepad_cheat_life = NULL;
char *g_gamepad_cheat_keys = NULL;
char *g_gamepad_cheat_walkthrough = NULL;

// Gamepad system controls
char *g_gamepad_clear_keylog = NULL;
char *g_gamepad_stop_replay = NULL;
char *g_gamepad_fullscreen = NULL;
char *g_gamepad_reset = NULL;
char *g_gamepad_pause_dimmed = NULL;
char *g_gamepad_pause = NULL;
char *g_gamepad_turbo = NULL;
char *g_gamepad_replay_turbo = NULL;
char *g_gamepad_window_bigger = NULL;
char *g_gamepad_window_smaller = NULL;
char *g_gamepad_volume_up = NULL;
char *g_gamepad_volume_down = NULL;
char *g_gamepad_display_perf = NULL;
char *g_gamepad_toggle_renderer = NULL;

// Control names for UI
static const char *kControlNames[12] = {
    "Up", "Down", "Left", "Right", "Select", "Start", "A", "B", "X", "Y", "L", "R"
};

// Get directory containing the launcher executable
void LauncherUI_GetExecutableDir(char *buf, size_t buf_size) {
#ifdef __APPLE__
    uint32_t size = buf_size;
    if (_NSGetExecutablePath(buf, &size) == 0) {
        char *last_slash = strrchr(buf, '/');
        if (last_slash) *last_slash = '\0';
    } else {
        snprintf(buf, buf_size, ".");
    }
#elif defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", buf, buf_size - 1);
    if (len != -1) {
        buf[len] = '\0';
        char *last_slash = strrchr(buf, '/');
        if (last_slash) *last_slash = '\0';
    } else {
        snprintf(buf, buf_size, ".");
    }
#else
    snprintf(buf, buf_size, ".");
#endif
}

// Widget references for updating config
static struct {
    // General tab
    GtkWidget *dat_status_label;
    GtkWidget *language_combo;
    GtkWidget *rom_path_entry;
    GtkWidget *make_dat_status;

    // Graphics tab
    GtkWidget *output_method;
    GtkWidget *window_size_mode;
    GtkWidget *window_width;
    GtkWidget *window_height;
    GtkWidget *window_width_label;
    GtkWidget *window_height_label;
    GtkWidget *window_scale;
    GtkWidget *window_scale_label;
    GtkWidget *fullscreen;
    GtkWidget *aspect_ratio;
    GtkWidget *ignore_aspect_ratio;
    GtkWidget *extend_y;
    GtkWidget *linear_filtering;
    GtkWidget *new_renderer;
    GtkWidget *enhanced_mode7;
    GtkWidget *no_sprite_limits;

    // Sound tab
    GtkWidget *enable_audio;
    GtkWidget *audio_freq;
    GtkWidget *audio_channels;
    GtkWidget *audio_samples;
    GtkWidget *enable_msu;
    GtkWidget *resume_msu;
    GtkWidget *msu_volume_spin;

    // Features tab
    GtkWidget *feat_switch_lr;
    GtkWidget *feat_switch_lr_limit;
    GtkWidget *feat_turn_dash;
    GtkWidget *feat_mirror_dw;
    GtkWidget *feat_sword_collect;
    GtkWidget *feat_sword_pots;
    GtkWidget *feat_more_bombs;
    GtkWidget *feat_more_rupees;
    GtkWidget *feat_cancel_bird;
    GtkWidget *feat_no_beep;
    GtkWidget *feat_skip_intro;
    GtkWidget *feat_yellow_items;
    GtkWidget *feat_misc_bugs;
    GtkWidget *feat_game_bugs;
    GtkWidget *feat_pokemode;
    GtkWidget *feat_zelda_helps;

    // Keyboard subtab grids (for accessing entry widgets)
    GtkWidget *kbd_states_grid;
    GtkWidget *kbd_cheats_grid;
    GtkWidget *kbd_system_grid;

    // Gamepad subtab grids (for accessing button widgets)
    GtkWidget *gamepad_states_grid;
    GtkWidget *gamepad_cheats_grid;
    GtkWidget *gamepad_system_grid;

    // Path selection widgets
    GtkWidget *msu_path_entry;
    GtkWidget *shader_path_entry;
} g_widgets;

// Helper: Create labeled combo box
static GtkWidget* create_combo_box_with_label(GtkWidget *grid, int row,
                                                const char *label_text,
                                                const char **options, int num_options) {
    GtkWidget *label = gtk_label_new(label_text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);

    GtkWidget *combo = gtk_combo_box_text_new();
    for (int i = 0; i < num_options; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), options[i]);
    }
    gtk_grid_attach(GTK_GRID(grid), combo, 1, row, 1, 1);

    return combo;
}

// Helper: Create checkbox with label
static GtkWidget* create_checkbox(GtkWidget *grid, int row, const char *label_text) {
    GtkWidget *check = gtk_check_button_new_with_label(label_text);
    gtk_grid_attach(GTK_GRID(grid), check, 0, row, 2, 1);
    return check;
}

// Helper: Create horizontal radio buttons with label
static GtkWidget* create_radio_buttons(GtkWidget *grid, int row, const char *label_text,
                                        const char **options, int num_options) {
    GtkWidget *label = gtk_label_new(label_text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GSList *group = NULL;
    GtkWidget *first_radio = NULL;

    for (int i = 0; i < num_options; i++) {
        GtkWidget *radio = gtk_radio_button_new_with_label(group, options[i]);
        group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));
        gtk_box_pack_start(GTK_BOX(box), radio, FALSE, FALSE, 0);
        if (i == 0) first_radio = radio;
    }

    gtk_grid_attach(GTK_GRID(grid), box, 1, row, 1, 1);
    return first_radio;  // Return first button for getting/setting active index
}

// Helper: Create horizontal scale with label
static GtkWidget* create_hscale_with_label(GtkWidget *grid, int row, const char *label_text,
                                             double min, double max, double step) {
    GtkWidget *label = gtk_label_new(label_text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);

    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, min, max, step);
    gtk_scale_set_digits(GTK_SCALE(scale), 0);  // No decimal places
    gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_RIGHT);
    gtk_widget_set_hexpand(scale, TRUE);
    gtk_grid_attach(GTK_GRID(grid), scale, 1, row, 1, 1);

    return scale;
}

// Parse comma-separated control string into array (keyboard controls)
void LauncherUI_ParseControlString(const char *str, char **controls) {
    if (!str || !*str) {
        // Set defaults if no string
        const char *defaults[NUM_SNES_BUTTONS] = {
            "Up", "Down", "Left", "Right", "Right Shift", "Return",
            "X", "Z", "S", "A", "C", "V"
        };
        for (int i = 0; i < NUM_SNES_BUTTONS; i++) {
            controls[i] = strdup(defaults[i]);
            if (!controls[i]) controls[i] = strdup("");
        }
        return;
    }

    // Parse comma-separated values (preserves empty values between commas)
    const char *p = str;
    int i = 0;

    while (*p && i < NUM_SNES_BUTTONS) {
        // Find next comma or end of string
        const char *comma = strchr(p, ',');
        const char *end = comma ? comma : p + strlen(p);

        // Extract substring
        size_t len = end - p;
        char *token = malloc(len + 1);
        if (!token) break;  // Allocation failed
        strncpy(token, p, len);
        token[len] = '\0';

        // Trim whitespace
        char *trimmed = token;
        while (isspace((unsigned char)*trimmed)) trimmed++;
        if (*trimmed) {
            char *trim_end = trimmed + strlen(trimmed) - 1;
            while (trim_end > trimmed && isspace((unsigned char)*trim_end)) trim_end--;
            trim_end[1] = '\0';
        }

        // Store value (even if empty)
        controls[i++] = strdup(trimmed);
        free(token);

        // Move past comma
        if (comma) {
            p = comma + 1;
        } else {
            break;
        }
    }

    // Fill remaining with empty strings
    while (i < NUM_SNES_BUTTONS) {
        controls[i++] = strdup("");
    }
}

// Parse comma-separated control string into array (gamepad controls)
void LauncherUI_ParseGamepadControlString(const char *str, char **controls) {
    if (!str || !*str) {
        // Set gamepad defaults if no string
        // Mapping: Up, Down, Left, Right, Select, Start, A, B, X, Y, L, R
        // Xbox-style controller layout (positional mapping for SNES buttons)
        const char *defaults[NUM_SNES_BUTTONS] = {
            "DpadUp", "DpadDown", "DpadLeft", "DpadRight", "Back", "Start",
            "B", "A", "Y", "X", "L1", "R1"
        };
        for (int i = 0; i < NUM_SNES_BUTTONS; i++) {
            controls[i] = strdup(defaults[i]);
            if (!controls[i]) controls[i] = strdup("");
        }
        return;
    }

    // Parse comma-separated values
    char *copy = strdup(str);
    if (!copy) {
        // Allocation failed, use defaults
        for (int i = 0; i < NUM_SNES_BUTTONS; i++) {
            controls[i] = strdup("");
        }
        return;
    }
    char *token = strtok(copy, ",");
    int i = 0;
    while (token && i < NUM_SNES_BUTTONS) {
        // Trim whitespace
        while (isspace((unsigned char)*token)) token++;
        char *end = token + strlen(token) - 1;
        while (end > token && isspace((unsigned char)*end)) end--;
        end[1] = '\0';

        controls[i++] = strdup(token);
        token = strtok(NULL, ",");
    }

    // Fill remaining with empty strings
    while (i < NUM_SNES_BUTTONS) {
        controls[i++] = strdup("");
    }

    free(copy);
}

// Format control array into comma-separated string
char* LauncherUI_FormatControlString(char **controls, int count) {
    size_t total_len = 0;
    for (int i = 0; i < count; i++) {
        if (controls[i]) {
            total_len += strlen(controls[i]);
        }
        if (i > 0) total_len += 2;  // +2 for ", "
    }

    char *result = malloc(total_len + 1);
    if (!result) return NULL;
    result[0] = '\0';

    for (int i = 0; i < count; i++) {
        if (i > 0) strcat(result, ", ");
        // Write value (or empty string if cleared)
        if (controls[i] && *controls[i]) {
            strcat(result, controls[i]);
        }
        // else: leave empty between commas
    }

    return result;
}

// Signal handler for window size mode dropdown
static void on_window_size_mode_changed(GtkComboBox *combo, gpointer user_data) {
    (void)user_data;
    int mode = gtk_combo_box_get_active(combo);
    bool is_custom = (mode == 1);  // 0=Auto, 1=Custom

    if (is_custom) {
        // Custom mode: show width/height, hide scale
        gtk_widget_show(g_widgets.window_width_label);
        gtk_widget_show(g_widgets.window_width);
        gtk_widget_show(g_widgets.window_height_label);
        gtk_widget_show(g_widgets.window_height);
        gtk_widget_hide(g_widgets.window_scale_label);
        gtk_widget_hide(g_widgets.window_scale);
    } else {
        // Auto mode: show scale, hide width/height
        gtk_widget_show(g_widgets.window_scale_label);
        gtk_widget_show(g_widgets.window_scale);
        gtk_widget_hide(g_widgets.window_width_label);
        gtk_widget_hide(g_widgets.window_width);
        gtk_widget_hide(g_widgets.window_height_label);
        gtk_widget_hide(g_widgets.window_height);
    }
}

// Signal handler for Shader path browse button
static void on_shader_path_browse_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;

    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Select Shader File",
        NULL,
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Open", GTK_RESPONSE_ACCEPT,
        NULL);

    // Add file filter for shader files
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Shader files (*.glsl, *.glslp)");
    gtk_file_filter_add_pattern(filter, "*.glsl");
    gtk_file_filter_add_pattern(filter, "*.glslp");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    // Add "All files" filter
    GtkFileFilter *filter_all = gtk_file_filter_new();
    gtk_file_filter_set_name(filter_all, "All files");
    gtk_file_filter_add_pattern(filter_all, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_all);

    // Set current file if one is already set
    const char *current_path = gtk_entry_get_text(GTK_ENTRY(g_widgets.shader_path_entry));
    if (current_path && *current_path) {
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), current_path);
    }

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_text(GTK_ENTRY(g_widgets.shader_path_entry), filename);
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

// Signal handler for MSU path browse button
static void on_msu_path_browse_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;

    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Select MSU Folder",
        NULL,
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Select", GTK_RESPONSE_ACCEPT,
        NULL);

    // Set current folder if one is already set
    const char *current_path = gtk_entry_get_text(GTK_ENTRY(g_widgets.msu_path_entry));
    if (current_path && *current_path) {
        // Strip the "/alttp_msu-" suffix if present to get the folder
        char *folder_path = strdup(current_path);
        char *suffix = strstr(folder_path, "/alttp_msu-");
        if (suffix) *suffix = '\0';
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), folder_path);
        free(folder_path);
    }

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

        // Append "/alttp_msu-" to the folder path
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/alttp_msu-", folder);

        gtk_entry_set_text(GTK_ENTRY(g_widgets.msu_path_entry), full_path);
        g_free(folder);
    }

    gtk_widget_destroy(dialog);
}

// Forward declarations
static void refresh_language_dropdown(const char *current_lang);
static void update_dat_status(void);

// Helper to get display name for a language code
static const char* get_language_display_name(const char *code) {
    static const struct { const char *code; const char *name; } kLangMap[] = {
        {"us", "English (US)"},
        {"de", "German"},
        {"fr", "French"},
        {"fr_c", "French (Canada)"},
        {"en", "English (EU)"},
        {"es", "Spanish"},
        {"pl", "Polish"},
        {"pt", "Portuguese"},
        {"nl", "Dutch"},
        {"sv", "Swedish"},
    };
    for (size_t i = 0; i < sizeof(kLangMap) / sizeof(kLangMap[0]); i++) {
        if (strcmp(code, kLangMap[i].code) == 0)
            return kLangMap[i].name;
    }
    return code;  // Return code if unknown
}

// Update DAT status label and language dropdown
static void update_dat_status(void) {
    char exe_dir[512];
    LauncherUI_GetExecutableDir(exe_dir, sizeof(exe_dir));

    if (DatReader_Exists(exe_dir)) {
        gtk_label_set_markup(GTK_LABEL(g_widgets.dat_status_label),
            "<span foreground='green'>zelda3_assets.dat found</span>");
    } else {
        gtk_label_set_markup(GTK_LABEL(g_widgets.dat_status_label),
            "<span foreground='red'>zelda3_assets.dat not found - create from ROM below</span>");
    }
}

// Signal handler for ROM path browse button
static void on_rom_browse_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;

    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Select ROM File",
        NULL,
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Open", GTK_RESPONSE_ACCEPT,
        NULL);

    // Add file filter for ROM files
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "SNES ROM files (*.sfc, *.smc)");
    gtk_file_filter_add_pattern(filter, "*.sfc");
    gtk_file_filter_add_pattern(filter, "*.smc");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    // Add "All files" filter
    GtkFileFilter *filter_all = gtk_file_filter_new();
    gtk_file_filter_set_name(filter_all, "All files");
    gtk_file_filter_add_pattern(filter_all, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_all);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_text(GTK_ENTRY(g_widgets.rom_path_entry), filename);
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

// Signal handler for Create DAT button
static void on_make_dat_clicked(GtkButton *button, gpointer user_data) {
    (void)user_data;

    const char *rom_path = gtk_entry_get_text(GTK_ENTRY(g_widgets.rom_path_entry));
    if (!rom_path || !*rom_path) {
        gtk_label_set_text(GTK_LABEL(g_widgets.make_dat_status), "Error: No ROM file selected");
        return;
    }

    gtk_label_set_text(GTK_LABEL(g_widgets.make_dat_status), "Extracting assets...");
    gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);

    // Force redraw before blocking operation
    while (gtk_events_pending())
        gtk_main_iteration();

    // Get directory where launcher (and restool) live
    char exe_dir[512];
    LauncherUI_GetExecutableDir(exe_dir, sizeof(exe_dir));

    // Build command to run restool with output to exe directory
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "\"%s/zelda3_restool\" --extract-from-rom \"%s\" --output \"%s\" --compile 2>&1",
        exe_dir, rom_path, exe_dir);

    // Use popen to capture restool output for better error messages
    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        gtk_label_set_text(GTK_LABEL(g_widgets.make_dat_status),
            "Error: Failed to run restool");
        gtk_widget_set_sensitive(GTK_WIDGET(button), TRUE);
        return;
    }

    char output[4096] = {0};
    size_t total = 0;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe) && total < sizeof(output) - 1) {
        size_t len = strlen(buf);
        memcpy(output + total, buf, len);
        total += len;
    }
    int result = pclose(pipe);

    if (result == 0) {
        gtk_label_set_text(GTK_LABEL(g_widgets.make_dat_status),
            "Success! Created zelda3_assets.dat");
        // Refresh DAT status and language dropdown
        update_dat_status();
        refresh_language_dropdown(NULL);  // Refresh with new languages from DAT
    } else {
        // Parse output for specific error patterns
        char error_msg[256] = "Error: Extraction failed.";

        // Check for known ROM detection (e.g., "Detected ROM: de - ...")
        char *detected = strstr(output, "Detected ROM:");
        if (detected) {
            // Extract the language/region code
            char *dash = strchr(detected + 13, '-');
            if (dash) {
                // Skip to the ROM name after " - "
                char *name_start = dash + 2;
                char *name_end = strchr(name_start, '"');
                if (!name_end) name_end = strchr(name_start, '\n');
                if (!name_end) name_end = name_start + strlen(name_start);

                // Extract region code (2-3 chars before the dash)
                char region[8] = {0};
                char *region_start = detected + 13;
                while (*region_start == ' ') region_start++;
                int i = 0;
                while (region_start < dash && *region_start != ' ' && i < 7) {
                    region[i++] = *region_start++;
                }
                region[i] = '\0';

                // Create user-friendly message
                if (strcmp(region, "de") == 0) {
                    snprintf(error_msg, sizeof(error_msg),
                        "Error: Detected German ROM. Only USA ROM is supported.");
                } else if (strcmp(region, "fr") == 0 || strcmp(region, "fr_c") == 0) {
                    snprintf(error_msg, sizeof(error_msg),
                        "Error: Detected French ROM. Only USA ROM is supported.");
                } else if (strcmp(region, "en") == 0) {
                    snprintf(error_msg, sizeof(error_msg),
                        "Error: Detected European ROM. Only USA ROM is supported.");
                } else if (strcmp(region, "es") == 0) {
                    snprintf(error_msg, sizeof(error_msg),
                        "Error: Detected Spanish ROM. Only USA ROM is supported.");
                } else if (region[0]) {
                    snprintf(error_msg, sizeof(error_msg),
                        "Error: Detected %s ROM. Only USA ROM is supported.", region);
                }
            }
        } else if (strstr(output, "not supported")) {
            snprintf(error_msg, sizeof(error_msg),
                "Error: Unrecognized ROM (SHA1 mismatch). Only USA ROM is supported.");
        } else if (strstr(output, "Failed to read") || strstr(output, "Cannot open")) {
            snprintf(error_msg, sizeof(error_msg),
                "Error: Failed to read ROM file.");
        }

        gtk_label_set_text(GTK_LABEL(g_widgets.make_dat_status), error_msg);
    }

    gtk_widget_set_sensitive(GTK_WIDGET(button), TRUE);
}

// Populate language dropdown with languages available in DAT file
static void refresh_language_dropdown(const char *current_lang) {
    // Get available languages from DAT file
    char exe_dir[512];
    LauncherUI_GetExecutableDir(exe_dir, sizeof(exe_dir));

    char available_langs[16][8];
    int num_langs = DatReader_GetLanguages(exe_dir, available_langs, 16);

    // Create list store: display name, code
    GtkListStore *store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);

    int active_idx = 0;

    if (num_langs > 0) {
        // Populate with languages found in DAT
        for (int i = 0; i < num_langs; i++) {
            GtkTreeIter iter;
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter,
                0, get_language_display_name(available_langs[i]),  // Display name
                1, available_langs[i],  // Code
                -1);

            // Check if this is the currently selected language
            if (current_lang && strcmp(available_langs[i], current_lang) == 0) {
                active_idx = i;
            }
        }
    } else {
        // No DAT file or no languages - show placeholder
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
            0, "(No asset file)",
            1, "",
            -1);
    }

    gtk_combo_box_set_model(GTK_COMBO_BOX(g_widgets.language_combo), GTK_TREE_MODEL(store));
    g_object_unref(store);

    // Set up cell renderer
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_clear(GTK_CELL_LAYOUT(g_widgets.language_combo));
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(g_widgets.language_combo), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(g_widgets.language_combo), renderer,
        "text", 0,
        NULL);

    gtk_combo_box_set_active(GTK_COMBO_BOX(g_widgets.language_combo), active_idx);
}

// Create General tab
static GtkWidget* create_general_tab(const Config *config) {
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);

    int row = 0;

    // === Asset File Section ===
    GtkWidget *asset_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(asset_label), "<b>Asset File</b>");
    gtk_widget_set_halign(asset_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), asset_label, 0, row++, 2, 1);

    // DAT file status label
    GtkWidget *status_label = gtk_label_new("Status:");
    gtk_widget_set_halign(status_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), status_label, 0, row, 1, 1);

    g_widgets.dat_status_label = gtk_label_new("");
    gtk_widget_set_halign(g_widgets.dat_status_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), g_widgets.dat_status_label, 1, row++, 1, 1);

    // Update the status immediately
    update_dat_status();

    // === Language Section ===
    GtkWidget *lang_section_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lang_section_label), "<b>Language</b>");
    gtk_widget_set_halign(lang_section_label, GTK_ALIGN_START);
    gtk_widget_set_margin_top(lang_section_label, 10);
    gtk_grid_attach(GTK_GRID(grid), lang_section_label, 0, row++, 2, 1);

    GtkWidget *lang_label = gtk_label_new("Language:");
    gtk_widget_set_halign(lang_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), lang_label, 0, row, 1, 1);

    g_widgets.language_combo = gtk_combo_box_new();
    gtk_grid_attach(GTK_GRID(grid), g_widgets.language_combo, 1, row++, 1, 1);

    // Populate the language dropdown (reads from DAT file)
    refresh_language_dropdown(config->language);

    // === Create Asset File Section ===
    GtkWidget *create_section_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(create_section_label), "<b>Create Asset File from ROM</b>");
    gtk_widget_set_halign(create_section_label, GTK_ALIGN_START);
    gtk_widget_set_margin_top(create_section_label, 10);
    gtk_grid_attach(GTK_GRID(grid), create_section_label, 0, row++, 2, 1);

    // ROM file path + browse button
    GtkWidget *rom_label = gtk_label_new("ROM File:");
    gtk_widget_set_halign(rom_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), rom_label, 0, row, 1, 1);

    GtkWidget *rom_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    g_widgets.rom_path_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(rom_hbox), g_widgets.rom_path_entry, TRUE, TRUE, 0);

    GtkWidget *rom_browse_btn = gtk_button_new_with_label("Browse...");
    g_signal_connect(rom_browse_btn, "clicked", G_CALLBACK(on_rom_browse_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(rom_hbox), rom_browse_btn, FALSE, FALSE, 0);

    gtk_grid_attach(GTK_GRID(grid), rom_hbox, 1, row++, 1, 1);

    // Create DAT button
    GtkWidget *make_dat_btn = gtk_button_new_with_label("Create Asset File");
    g_signal_connect(make_dat_btn, "clicked", G_CALLBACK(on_make_dat_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid), make_dat_btn, 1, row++, 1, 1);

    // Status label
    g_widgets.make_dat_status = gtk_label_new("Ready");
    gtk_widget_set_halign(g_widgets.make_dat_status, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), g_widgets.make_dat_status, 1, row++, 1, 1);

    return grid;
}

// Create Graphics tab
static GtkWidget* create_graphics_tab(const Config *config) {
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);

    int row = 0;

    // Output method
    const char *output_methods[] = {"SDL", "SDL-Software", "OpenGL", "OpenGL ES", "Vulkan"};
    g_widgets.output_method = create_combo_box_with_label(grid, row++,
        "Output Method:", output_methods, 5);
    gtk_combo_box_set_active(GTK_COMBO_BOX(g_widgets.output_method), config->output_method);

    // Window Size Mode dropdown
    const char *window_size_modes[] = {"Auto", "Custom"};
    g_widgets.window_size_mode = create_combo_box_with_label(grid, row++,
        "Window Size:", window_size_modes, 2);

    // Determine initial mode: Auto if both width and height are 0
    bool is_auto = (config->window_width == 0 && config->window_height == 0);
    gtk_combo_box_set_active(GTK_COMBO_BOX(g_widgets.window_size_mode), is_auto ? 0 : 1);

    // Window Width (only visible in Custom mode)
    g_widgets.window_width_label = gtk_label_new("Window Width:");
    gtk_widget_set_halign(g_widgets.window_width_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), g_widgets.window_width_label, 0, row, 1, 1);

    g_widgets.window_width = gtk_spin_button_new_with_range(0, 3840, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_widgets.window_width), config->window_width);
    gtk_grid_attach(GTK_GRID(grid), g_widgets.window_width, 1, row, 1, 1);
    row++;

    // Window Height (only visible in Custom mode)
    g_widgets.window_height_label = gtk_label_new("Window Height:");
    gtk_widget_set_halign(g_widgets.window_height_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), g_widgets.window_height_label, 0, row, 1, 1);

    g_widgets.window_height = gtk_spin_button_new_with_range(0, 2160, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_widgets.window_height), config->window_height);
    gtk_grid_attach(GTK_GRID(grid), g_widgets.window_height, 1, row, 1, 1);
    row++;

    // Window Scale (only visible in Auto mode)
    g_widgets.window_scale_label = gtk_label_new("Window Scale:");
    gtk_widget_set_halign(g_widgets.window_scale_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), g_widgets.window_scale_label, 0, row, 1, 1);

    g_widgets.window_scale = gtk_spin_button_new_with_range(1, 10, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_widgets.window_scale), config->window_scale);
    gtk_grid_attach(GTK_GRID(grid), g_widgets.window_scale, 1, row, 1, 1);
    row++;

    // Mark widgets as manually controlled (won't be shown by gtk_widget_show_all)
    gtk_widget_set_no_show_all(g_widgets.window_width_label, TRUE);
    gtk_widget_set_no_show_all(g_widgets.window_width, TRUE);
    gtk_widget_set_no_show_all(g_widgets.window_height_label, TRUE);
    gtk_widget_set_no_show_all(g_widgets.window_height, TRUE);
    gtk_widget_set_no_show_all(g_widgets.window_scale_label, TRUE);
    gtk_widget_set_no_show_all(g_widgets.window_scale, TRUE);

    // Set initial visibility based on mode
    if (is_auto) {
        // Auto mode: show scale, hide width/height
        gtk_widget_show(g_widgets.window_scale_label);
        gtk_widget_show(g_widgets.window_scale);
        gtk_widget_hide(g_widgets.window_width_label);
        gtk_widget_hide(g_widgets.window_width);
        gtk_widget_hide(g_widgets.window_height_label);
        gtk_widget_hide(g_widgets.window_height);
    } else {
        // Custom mode: show width/height, hide scale
        gtk_widget_show(g_widgets.window_width_label);
        gtk_widget_show(g_widgets.window_width);
        gtk_widget_show(g_widgets.window_height_label);
        gtk_widget_show(g_widgets.window_height);
        gtk_widget_hide(g_widgets.window_scale_label);
        gtk_widget_hide(g_widgets.window_scale);
    }

    // Connect signal handler for window size mode changes
    g_signal_connect(g_widgets.window_size_mode, "changed",
                     G_CALLBACK(on_window_size_mode_changed), NULL);

    // Fullscreen
    const char *fullscreen_options[] = {"Windowed", "Borderless Fullscreen", "Fullscreen"};
    g_widgets.fullscreen = create_radio_buttons(grid, row++, "Display Mode:", fullscreen_options, 3);

    // Set active radio button based on config value
    GSList *fs_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(g_widgets.fullscreen));
    int fs_index = config->fullscreen < 3 ? config->fullscreen : 0;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_slist_nth_data(fs_group, 2 - fs_index)), TRUE);

    // Aspect ratio
    const char *aspect_ratios[] = {"4:3", "16:9", "16:10", "18:9"};
    g_widgets.aspect_ratio = create_combo_box_with_label(grid, row++,
        "Aspect Ratio:", aspect_ratios, 4);
    gtk_combo_box_set_active(GTK_COMBO_BOX(g_widgets.aspect_ratio), config->extended_aspect_ratio);

    // Checkboxes
    g_widgets.ignore_aspect_ratio = create_checkbox(grid, row++, "Stretch to fill window (ignore aspect ratio)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.ignore_aspect_ratio), config->ignore_aspect_ratio);

    g_widgets.extend_y = create_checkbox(grid, row++, "Extend render height (224 lines -> 240 lines)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.extend_y), config->extend_y);

    g_widgets.linear_filtering = create_checkbox(grid, row++, "Use linear filtering for smoother pixels");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.linear_filtering), config->linear_filtering);

    g_widgets.new_renderer = create_checkbox(grid, row++, "Use optimized SNES PPU renderer");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.new_renderer), config->new_renderer);

    g_widgets.enhanced_mode7 = create_checkbox(grid, row++, "Display the world map with higher resolution (Enhanced Mode 7)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.enhanced_mode7), config->enhanced_mode7);

    g_widgets.no_sprite_limits = create_checkbox(grid, row++, "Disable SNES sprite limit (8 sprites per scanline)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.no_sprite_limits), config->no_sprite_limits);

    // Shader file path
    GtkWidget *shader_label = gtk_label_new("Shader File:");
    gtk_widget_set_halign(shader_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), shader_label, 0, row, 1, 1);

    GtkWidget *shader_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    g_widgets.shader_path_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(g_widgets.shader_path_entry),
                       config->shader ? config->shader : "");
    gtk_box_pack_start(GTK_BOX(shader_hbox), g_widgets.shader_path_entry, TRUE, TRUE, 0);

    GtkWidget *shader_browse_btn = gtk_button_new_with_label("Browse...");
    g_signal_connect(shader_browse_btn, "clicked",
                     G_CALLBACK(on_shader_path_browse_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(shader_hbox), shader_browse_btn, FALSE, FALSE, 0);

    gtk_grid_attach(GTK_GRID(grid), shader_hbox, 1, row, 1, 1);
    row++;

    return grid;
}

// Create Sound tab
static GtkWidget* create_sound_tab(const Config *config) {
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);

    int row = 0;

    g_widgets.enable_audio = create_checkbox(grid, row++, "Enable Audio");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.enable_audio), config->enable_audio);

    const char *channel_options[] = {"Mono", "Stereo"};
    g_widgets.audio_channels = create_radio_buttons(grid, row++, "Audio Channels:", channel_options, 2);

    // Set active based on config (1=Mono, 2=Stereo)
    GSList *ch_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(g_widgets.audio_channels));
    int ch_index = config->audio_channels == 2 ? 1 : 0;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_slist_nth_data(ch_group, 1 - ch_index)), TRUE);

    // Audio frequency
    const char *freqs[] = {"11025", "22050", "32000", "44100 (Use with PCM MSU)", "48000 (Use with OPUZ MSU)"};
    g_widgets.audio_freq = create_combo_box_with_label(grid, row++,
        "Audio Frequency:", freqs, 5);
    // Map freq to index
    int freq_idx = 3; // default 44100
    switch (config->audio_freq) {
        case 11025: freq_idx = 0; break;
        case 22050: freq_idx = 1; break;
        case 32000: freq_idx = 2; break;
        case 44100: freq_idx = 3; break;
        case 48000: freq_idx = 4; break;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(g_widgets.audio_freq), freq_idx);

    const char *sample_opts[] = {"256", "512", "1024", "2048", "4096"};
    int sample_values[] = {256, 512, 1024, 2048, 4096};
    g_widgets.audio_samples = create_combo_box_with_label(grid, row++,
        "Audio Samples:", sample_opts, 5);
    // Find matching index for current value
    int sample_idx = 2; // default to 1024
    for (int i = 0; i < 5; i++) {
        if (config->audio_samples == sample_values[i]) {
            sample_idx = i;
            break;
        }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(g_widgets.audio_samples), sample_idx);

    // MSU
    const char *msu_opts[] = {"Disabled", "PCM", "PCM Deluxe", "Opuz", "Opuz Deluxe"};
    g_widgets.enable_msu = create_combo_box_with_label(grid, row++,
        "MSU:", msu_opts, 5);
    // Map config value to dropdown index
    int msu_idx = 0; // Default disabled
    if (config->enable_msu & kMsuEnabled_Msu) {
        if ((config->enable_msu & (kMsuEnabled_MsuDeluxe | kMsuEnabled_Opuz)) == (kMsuEnabled_MsuDeluxe | kMsuEnabled_Opuz)) {
            msu_idx = 4; // Opuz Deluxe
        } else if (config->enable_msu & kMsuEnabled_MsuDeluxe) {
            msu_idx = 2; // PCM Deluxe
        } else if (config->enable_msu & kMsuEnabled_Opuz) {
            msu_idx = 3; // Opuz
        } else {
            msu_idx = 1; // PCM
        }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(g_widgets.enable_msu), msu_idx);

    g_widgets.msu_volume_spin = create_hscale_with_label(grid, row++,
        "MSU Volume:", 0, 100, 1);
    gtk_range_set_value(GTK_RANGE(g_widgets.msu_volume_spin), config->msuvolume);

    g_widgets.resume_msu = create_checkbox(grid, row++, "Resume MSU position when re-entering overworld area");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.resume_msu), config->resume_msu);

    // MSU folder path
    GtkWidget *msu_path_label = gtk_label_new("MSU Folder:");
    gtk_widget_set_halign(msu_path_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), msu_path_label, 0, row, 1, 1);

    GtkWidget *msu_path_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    g_widgets.msu_path_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(g_widgets.msu_path_entry),
                       config->msu_path ? config->msu_path : "");
    gtk_box_pack_start(GTK_BOX(msu_path_hbox), g_widgets.msu_path_entry, TRUE, TRUE, 0);

    GtkWidget *msu_path_browse_btn = gtk_button_new_with_label("Browse...");
    g_signal_connect(msu_path_browse_btn, "clicked",
                     G_CALLBACK(on_msu_path_browse_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(msu_path_hbox), msu_path_browse_btn, FALSE, FALSE, 0);

    gtk_grid_attach(GTK_GRID(grid), msu_path_hbox, 1, row, 1, 1);
    row++;

    return grid;
}

// Create Features tab
static GtkWidget* create_features_tab(const Config *config) {
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);

    int row = 0;
    uint32 features = config->features0;

    g_widgets.feat_switch_lr = create_checkbox(grid, row++, "Item switching with L/R shoulder buttons");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_switch_lr),
        features & kFeatures0_SwitchLR);

    g_widgets.feat_switch_lr_limit = create_checkbox(grid, row++, "Limit L/R item switching to first 4 items only");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_switch_lr_limit),
        features & kFeatures0_SwitchLRLimit);

    g_widgets.feat_turn_dash = create_checkbox(grid, row++, "Allow Link to turn while dashing");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_turn_dash),
        features & kFeatures0_TurnWhileDashing);

    g_widgets.feat_mirror_dw = create_checkbox(grid, row++, "Allow magic mirror to warp to the Dark World");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_mirror_dw),
        features & kFeatures0_MirrorToDarkworld);

    g_widgets.feat_sword_collect = create_checkbox(grid, row++, "Collect items (hearts, rupees) with sword");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_sword_collect),
        features & kFeatures0_CollectItemsWithSword);

    g_widgets.feat_sword_pots = create_checkbox(grid, row++, "Break pots with level 2-4 sword");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_sword_pots),
        features & kFeatures0_BreakPotsWithSword);

    g_widgets.feat_more_bombs = create_checkbox(grid, row++, "Allow more active bombs (4 instead of 2)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_more_bombs),
        features & kFeatures0_MoreActiveBombs);

    g_widgets.feat_more_rupees = create_checkbox(grid, row++, "Increase rupee capacity to 9999");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_more_rupees),
        features & kFeatures0_CarryMoreRupees);

    g_widgets.feat_cancel_bird = create_checkbox(grid, row++, "Cancel bird travel by pressing X");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_cancel_bird),
        features & kFeatures0_CancelBirdTravel);

    g_widgets.feat_no_beep = create_checkbox(grid, row++, "Disable low health beep sound");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_no_beep),
        features & kFeatures0_DisableLowHealthBeep);

    g_widgets.feat_skip_intro = create_checkbox(grid, row++, "Skip intro on any keypress");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_skip_intro),
        features & kFeatures0_SkipIntroOnKeypress);

    g_widgets.feat_yellow_items = create_checkbox(grid, row++, "Highlight maxed items in yellow");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_yellow_items),
        features & kFeatures0_ShowMaxItemsInYellow);

    g_widgets.feat_misc_bugs = create_checkbox(grid, row++, "Fix misc bugs from original game");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_misc_bugs),
        features & kFeatures0_MiscBugFixes);

    g_widgets.feat_game_bugs = create_checkbox(grid, row++, "Fix bugs that change gameplay");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_game_bugs),
        features & kFeatures0_GameChangingBugFixes);

    g_widgets.feat_pokemode = create_checkbox(grid, row++, "Experimental: Pokemode");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_pokemode),
        features & kFeatures0_Pokemode);

    g_widgets.feat_zelda_helps = create_checkbox(grid, row++, "Experimental: Princess Zelda helps in battle");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_zelda_helps),
        features & kFeatures0_PrincessZeldaHelps);

    return grid;
}

// Keyboard capture: global state for dialog
static int g_captured_control_index = -1;
static GtkWidget *g_capture_dialog = NULL;
static bool g_key_captured = false;

// Key press event handler for capture dialog
static gboolean on_key_press_capture(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    (void)widget;
    GtkWidget *button = (GtkWidget*)user_data;

    // Get GDK key name and convert to SDL scancode name
    const char *gdk_name = gdk_keyval_name(event->keyval);

    // Skip if this is a modifier key itself (don't capture Ctrl, Shift, Alt alone)
    if (gdk_name && (strstr(gdk_name, "Control") || strstr(gdk_name, "Shift") ||
                     strstr(gdk_name, "Alt") || strstr(gdk_name, "Meta") ||
                     strstr(gdk_name, "Super"))) {
        return TRUE;  // Ignore modifier keys
    }

    // GDK to SDL key name mapping table
    // GDK returns verbose names (e.g. "minus"), SDL expects symbols (e.g. "-")
    static const struct {
        const char *gdk;
        const char *sdl;
    } key_mapping[] = {
        // Punctuation and symbols
        {"minus", "-"},
        {"equal", "="},
        {"plus", "+"},
        {"comma", ","},
        {"period", "."},
        {"slash", "/"},
        {"backslash", "\\"},
        {"semicolon", ";"},
        {"apostrophe", "'"},
        {"grave", "`"},
        {"bracketleft", "["},
        {"bracketright", "]"},
        {"space", "Space"},
        // Keep localized keys as-is (e.g., German "section" key)
        // GDK "section" → SDL "section"
        {NULL, NULL}  // Sentinel
    };

    // Map GDK key name to SDL scancode name
    // SDL uses uppercase single letters, GDK uses lowercase
    char key_name[64] = {0};
    if (gdk_name) {
        // Check mapping table first
        const char *mapped_name = NULL;
        for (int i = 0; key_mapping[i].gdk != NULL; i++) {
            if (strcmp(gdk_name, key_mapping[i].gdk) == 0) {
                mapped_name = key_mapping[i].sdl;
                break;
            }
        }

        if (mapped_name) {
            // Use mapped name
            strncpy(key_name, mapped_name, sizeof(key_name) - 1);
        } else if (strlen(gdk_name) == 1 && gdk_name[0] >= 'a' && gdk_name[0] <= 'z') {
            // Capitalize single letters
            key_name[0] = gdk_name[0] - 'a' + 'A';
            key_name[1] = '\0';
        } else {
            // For other keys, use GDK name as-is (F1, Return, etc. match SDL names)
            strncpy(key_name, gdk_name, sizeof(key_name) - 1);
        }
    }

    if (key_name[0]) {
        // Build full key string with modifiers
        char full_key[128] = {0};

        // Add modifiers (Ctrl, Shift, Alt)
        if (event->state & GDK_CONTROL_MASK) {
            strcat(full_key, "Ctrl+");
        }
        if (event->state & GDK_SHIFT_MASK) {
            strcat(full_key, "Shift+");
        }
        if (event->state & GDK_MOD1_MASK) {  // Alt key
            strcat(full_key, "Alt+");
        }

        // Append the base key
        strcat(full_key, key_name);

        // Update control mapping if this is for indexed controls
        if (g_captured_control_index >= 0) {
            if (g_kbd_controls[g_captured_control_index]) {
                free(g_kbd_controls[g_captured_control_index]);
            }
            g_kbd_controls[g_captured_control_index] = strdup(full_key);
        }

        // Update individual variable if attached (for Cheats/System buttons)
        char **var_ptr = (char**)g_object_get_data(G_OBJECT(button), "variable_ptr");
        if (var_ptr) {
            if (*var_ptr) {
                free(*var_ptr);
            }
            *var_ptr = strdup(full_key);
        }

        // Update button label
        gtk_button_set_label(GTK_BUTTON(button), full_key);

        g_key_captured = true;
        gtk_dialog_response(GTK_DIALOG(g_capture_dialog), GTK_RESPONSE_OK);
    }

    return TRUE;
}

// Generic keyboard button click handler
static void on_key_button_clicked(GtkWidget *button, gpointer user_data) {
    const char *prompt = (const char*)user_data;

    // Create capture dialog
    GtkWidget *parent = gtk_widget_get_toplevel(button);
    g_capture_dialog = gtk_dialog_new_with_buttons(
        "Capture Key",
        GTK_WINDOW(parent),
        GTK_DIALOG_MODAL,
        "Cancel", GTK_RESPONSE_CANCEL,
        NULL
    );

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(g_capture_dialog));
    GtkWidget *label = gtk_label_new(NULL);
    char markup[256];
    snprintf(markup, sizeof(markup), "<big><b>Press a key for: %s</b></big>\n\n(supports Ctrl+, Shift+, Alt+ modifiers)\n(or Cancel to abort)", prompt);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_container_add(GTK_CONTAINER(content), label);

    // Connect key press handler
    g_signal_connect(g_capture_dialog, "key-press-event", G_CALLBACK(on_key_press_capture), button);

    gtk_widget_show_all(g_capture_dialog);
    gtk_dialog_run(GTK_DIALOG(g_capture_dialog));
    gtk_widget_destroy(g_capture_dialog);

    g_capture_dialog = NULL;
}

// Structure to pass data to clear button handler
typedef struct {
    GtkWidget *button;  // The binding button to update
    char **variable;    // Pointer to the variable to clear
} ClearButtonData;

// Clear button click handler
static void on_clear_button_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    ClearButtonData *data = (ClearButtonData*)user_data;

    // Clear the variable (set to empty string)
    if (data->variable && *data->variable) {
        free(*data->variable);
    }
    *data->variable = strdup("");

    // Update button label
    gtk_button_set_label(GTK_BUTTON(data->button), "(not set)");
}

// Keyboard button click handler for controls
static void on_keyboard_button_clicked(GtkWidget *button, gpointer user_data) {
    int index = GPOINTER_TO_INT(user_data);
    g_captured_control_index = index;
    g_key_captured = false;

    on_key_button_clicked(button, (gpointer)kControlNames[index]);
}

// Create KeyMap tab with interactive buttons
static GtkWidget* create_keymap_tab(const Config *config) {
    (void)config;

    // Create notebook for subtabs
    GtkWidget *notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);

    // --- Controls Subtab ---
    GtkWidget *controls_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(controls_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    GtkWidget *controls_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(controls_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(controls_grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(controls_grid), 10);

    GtkWidget *controls_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(controls_title), "<b>SNES Controller</b>");
    gtk_widget_set_halign(controls_title, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(controls_grid), controls_title, 0, 0, 3, 1);

    for (int i = 0; i < NUM_SNES_BUTTONS; i++) {
        // Create label for control name
        char label_text[64];
        snprintf(label_text, sizeof(label_text), "%s:", kControlNames[i]);
        GtkWidget *label = gtk_label_new(label_text);
        gtk_widget_set_halign(label, GTK_ALIGN_END);
        gtk_grid_attach(GTK_GRID(controls_grid), label, 0, i + 1, 1, 1);

        // Create button showing only the key
        const char *key = g_kbd_controls[i] ? g_kbd_controls[i] : "(not set)";
        GtkWidget *button = gtk_button_new_with_label(key);
        gtk_widget_set_size_request(button, BUTTON_WIDTH, BUTTON_HEIGHT);
        g_signal_connect(button, "clicked", G_CALLBACK(on_keyboard_button_clicked), GINT_TO_POINTER(i));
        gtk_grid_attach(GTK_GRID(controls_grid), button, 1, i + 1, 1, 1);

        // Add Clear button
        GtkWidget *clear_button = gtk_button_new_with_label("Clear");
        gtk_widget_set_size_request(clear_button, CLEAR_BUTTON_WIDTH, BUTTON_HEIGHT);
        ClearButtonData *clear_data = malloc(sizeof(ClearButtonData));
        if (clear_data) {
            clear_data->button = button;
            clear_data->variable = &g_kbd_controls[i];
            g_signal_connect(clear_button, "clicked", G_CALLBACK(on_clear_button_clicked), clear_data);
            g_signal_connect_swapped(clear_button, "destroy", G_CALLBACK(free), clear_data);
        }
        gtk_grid_attach(GTK_GRID(controls_grid), clear_button, 2, i + 1, 1, 1);
    }

    gtk_container_add(GTK_CONTAINER(controls_scroll), controls_grid);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), controls_scroll, gtk_label_new("Controls"));

    // --- Save States Subtab ---
    GtkWidget *states_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(states_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    g_widgets.kbd_states_grid = gtk_grid_new();
    GtkWidget *states_grid = g_widgets.kbd_states_grid;
    gtk_grid_set_row_spacing(GTK_GRID(states_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(states_grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(states_grid), 10);

    GtkWidget *states_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(states_title), "<b>Save States</b>");
    gtk_widget_set_halign(states_title, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(states_grid), states_title, 0, 0, 2, 1);

    int row = 1;
    const char *state_labels[] = {"Load", "Save", "Replay"};
    char **state_arrays[] = {g_kbd_load, g_kbd_save, g_kbd_replay};

    for (int type = 0; type < 3; type++) {
        GtkWidget *type_label = gtk_label_new(NULL);
        char markup[64];
        snprintf(markup, sizeof(markup), "<b>%s:</b>", state_labels[type]);
        gtk_label_set_markup(GTK_LABEL(type_label), markup);
        gtk_widget_set_halign(type_label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(states_grid), type_label, 0, row++, 3, 1);

        for (int i = 0; i < NUM_SAVE_SLOTS; i++) {
            char slot_label[64];
            snprintf(slot_label, sizeof(slot_label), "%s Slot %d:", state_labels[type], i + 1);
            GtkWidget *label = gtk_label_new(slot_label);
            gtk_widget_set_halign(label, GTK_ALIGN_END);
            gtk_grid_attach(GTK_GRID(states_grid), label, 0, row, 1, 1);

            const char *value;
            if (!state_arrays[type][i] || !*state_arrays[type][i]) {
                value = "(not set)";
            } else {
                value = state_arrays[type][i];
            }
            GtkWidget *button = gtk_button_new_with_label(value);
            gtk_widget_set_size_request(button, BUTTON_WIDTH, BUTTON_HEIGHT);
            g_signal_connect(button, "clicked", G_CALLBACK(on_key_button_clicked), slot_label);
            gtk_grid_attach(GTK_GRID(states_grid), button, 1, row, 1, 1);
            g_object_set_data(G_OBJECT(states_grid), g_strdup_printf("state_%d_%d", type, i), button);

            // Attach variable pointer to button for key capture
            g_object_set_data(G_OBJECT(button), "variable_ptr", &state_arrays[type][i]);

            // Add Clear button
            GtkWidget *clear_button = gtk_button_new_with_label("Clear");
            gtk_widget_set_size_request(clear_button, CLEAR_BUTTON_WIDTH, BUTTON_HEIGHT);
            ClearButtonData *clear_data = malloc(sizeof(ClearButtonData));
            if (clear_data) {
                clear_data->button = button;
                clear_data->variable = &state_arrays[type][i];
                g_signal_connect(clear_button, "clicked", G_CALLBACK(on_clear_button_clicked), clear_data);
                g_signal_connect_swapped(clear_button, "destroy", G_CALLBACK(free), clear_data);
            }
            gtk_grid_attach(GTK_GRID(states_grid), clear_button, 2, row, 1, 1);

            row++;
        }
    }

    gtk_container_add(GTK_CONTAINER(states_scroll), states_grid);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), states_scroll, gtk_label_new("Save States"));

    // --- Cheats Subtab ---
    GtkWidget *cheats_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cheats_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    g_widgets.kbd_cheats_grid = gtk_grid_new();
    GtkWidget *cheats_grid = g_widgets.kbd_cheats_grid;
    gtk_grid_set_row_spacing(GTK_GRID(cheats_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(cheats_grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(cheats_grid), 10);

    GtkWidget *cheats_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(cheats_title), "<b>Cheats</b>");
    gtk_widget_set_halign(cheats_title, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(cheats_grid), cheats_title, 0, 0, 3, 1);

    row = 1;
    struct { const char *label; char **ptr; const char *def; } cheats[] = {
        {"Refill Health & Magic:", &g_kbd_cheat_life, "W"},
        {"Set key count to 1:", &g_kbd_cheat_keys, "O"},
        {"Toggle Walk Through Walls:", &g_kbd_cheat_walkthrough, "Ctrl+E"}
    };

    for (int i = 0; i < 3; i++) {
        GtkWidget *label = gtk_label_new(cheats[i].label);
        gtk_widget_set_halign(label, GTK_ALIGN_END);
        gtk_grid_attach(GTK_GRID(cheats_grid), label, 0, row, 1, 1);

        const char *value;
        if (!*cheats[i].ptr || !**cheats[i].ptr) {
            value = "(not set)";
        } else {
            value = *cheats[i].ptr;
        }
        GtkWidget *button = gtk_button_new_with_label(value);
        gtk_widget_set_size_request(button, BUTTON_WIDTH, BUTTON_HEIGHT);
        g_signal_connect(button, "clicked", G_CALLBACK(on_key_button_clicked), (gpointer)cheats[i].label);
        gtk_grid_attach(GTK_GRID(cheats_grid), button, 1, row, 1, 1);
        g_object_set_data(G_OBJECT(cheats_grid), g_strdup_printf("cheat_%d", i), button);

        // Attach variable pointer to button for key capture
        g_object_set_data(G_OBJECT(button), "variable_ptr", cheats[i].ptr);

        // Add Clear button
        GtkWidget *clear_button = gtk_button_new_with_label("Clear");
        gtk_widget_set_size_request(clear_button, CLEAR_BUTTON_WIDTH, BUTTON_HEIGHT);
        ClearButtonData *clear_data = malloc(sizeof(ClearButtonData));
        if (clear_data) {
            clear_data->button = button;
            clear_data->variable = cheats[i].ptr;
            g_signal_connect(clear_button, "clicked", G_CALLBACK(on_clear_button_clicked), clear_data);
            g_signal_connect_swapped(clear_button, "destroy", G_CALLBACK(free), clear_data);
        }
        gtk_grid_attach(GTK_GRID(cheats_grid), clear_button, 2, row, 1, 1);

        row++;
    }

    gtk_container_add(GTK_CONTAINER(cheats_scroll), cheats_grid);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), cheats_scroll, gtk_label_new("Cheats"));

    // --- System Subtab ---
    GtkWidget *system_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(system_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    g_widgets.kbd_system_grid = gtk_grid_new();
    GtkWidget *system_grid = g_widgets.kbd_system_grid;
    gtk_grid_set_row_spacing(GTK_GRID(system_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(system_grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(system_grid), 10);

    GtkWidget *system_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(system_title), "<b>System Controls</b>");
    gtk_widget_set_halign(system_title, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(system_grid), system_title, 0, 0, 3, 1);

    row = 1;
    struct { const char *label; char **ptr; const char *def; } system_keys[] = {
        {"Toggle Fullscreen:", &g_kbd_fullscreen, "Alt+Return"},
        {"Reset:", &g_kbd_reset, "Ctrl+R"},
        {"Pause (Dimmed):", &g_kbd_pause_dimmed, "P"},
        {"Pause:", &g_kbd_pause, "Shift+P"},
        {"Turbo:", &g_kbd_turbo, "Tab"},
        {"Replay Turbo:", &g_kbd_replay_turbo, "T"},
        {"Window Bigger:", &g_kbd_window_bigger, "Ctrl+Up"},
        {"Window Smaller:", &g_kbd_window_smaller, "Ctrl+Down"},
        {"Volume Up:", &g_kbd_volume_up, "Shift+="},
        {"Volume Down:", &g_kbd_volume_down, "Shift+-"},
        {"Display FPS:", &g_kbd_display_perf, "F"},
        {"Toggle Renderer:", &g_kbd_toggle_renderer, "R"},
        {"Stop Replay:", &g_kbd_stop_replay, ""},
        {"Clear input recording log (debug):", &g_kbd_clear_keylog, ""}
    };

    for (int i = 0; i < NUM_SYSTEM_KEYS; i++) {
        GtkWidget *label = gtk_label_new(system_keys[i].label);
        gtk_widget_set_halign(label, GTK_ALIGN_END);
        gtk_grid_attach(GTK_GRID(system_grid), label, 0, row, 1, 1);

        const char *value;
        if (!*system_keys[i].ptr || !**system_keys[i].ptr) {
            value = "(not set)";
        } else {
            value = *system_keys[i].ptr;
        }
        GtkWidget *button = gtk_button_new_with_label(value);
        gtk_widget_set_size_request(button, BUTTON_WIDTH, BUTTON_HEIGHT);
        g_signal_connect(button, "clicked", G_CALLBACK(on_key_button_clicked), (gpointer)system_keys[i].label);
        gtk_grid_attach(GTK_GRID(system_grid), button, 1, row, 1, 1);
        g_object_set_data(G_OBJECT(system_grid), g_strdup_printf("system_%d", i), button);

        // Attach variable pointer to button for key capture
        g_object_set_data(G_OBJECT(button), "variable_ptr", system_keys[i].ptr);

        // Add Clear button
        GtkWidget *clear_button = gtk_button_new_with_label("Clear");
        gtk_widget_set_size_request(clear_button, CLEAR_BUTTON_WIDTH, BUTTON_HEIGHT);
        ClearButtonData *clear_data = malloc(sizeof(ClearButtonData));
        if (clear_data) {
            clear_data->button = button;
            clear_data->variable = system_keys[i].ptr;
            g_signal_connect(clear_button, "clicked", G_CALLBACK(on_clear_button_clicked), clear_data);
            g_signal_connect_swapped(clear_button, "destroy", G_CALLBACK(free), clear_data);
        }
        gtk_grid_attach(GTK_GRID(system_grid), clear_button, 2, row, 1, 1);

        row++;
    }

    gtk_container_add(GTK_CONTAINER(system_scroll), system_grid);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), system_scroll, gtk_label_new("System"));

    return notebook;
}

// Gamepad button click handler
static void on_gamepad_button_clicked(GtkWidget *button, gpointer user_data) {
    // Check if this is for indexed controls or individual variable
    char **var_ptr = (char**)g_object_get_data(G_OBJECT(button), "variable_ptr");
    const char *prompt = (const char*)g_object_get_data(G_OBJECT(button), "prompt");
    int index = -1;

    if (!var_ptr) {
        // Indexed control (original behavior)
        index = GPOINTER_TO_INT(user_data);
        prompt = kControlNames[index];
    }

    // Get first gamepad
    GamepadInfo gamepads[1];
    int num_pads = LauncherGamepad_ListControllers(gamepads, 1);
    if (num_pads == 0) {
        GtkWidget *parent = gtk_widget_get_toplevel(button);
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(parent),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "No gamepad detected!\n\nPlease connect a gamepad and try again."
        );
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    // Create capture dialog
    GtkWidget *parent = gtk_widget_get_toplevel(button);
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Capture Button",
        GTK_WINDOW(parent),
        GTK_DIALOG_MODAL,
        "Cancel", GTK_RESPONSE_CANCEL,
        NULL
    );

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *label = gtk_label_new(NULL);
    char markup[256];
    snprintf(markup, sizeof(markup), "<big><b>Press a button/axis for: %s</b></big>\n\n(5 second timeout or Cancel)",
             prompt);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_container_add(GTK_CONTAINER(content), label);

    gtk_widget_show_all(dialog);

    // Detect input with timeout (non-blocking)
    DetectedInput input = LauncherGamepad_DetectInput(gamepads[0].controller, 5000);

    const char *captured_name = NULL;
    if (input.type == INPUT_TYPE_BUTTON) {
        captured_name = LauncherGamepad_ButtonToString(input.button);
    } else if (input.type == INPUT_TYPE_AXIS) {
        captured_name = LauncherGamepad_AxisToString(input.axis, input.axis_value);
    }

    if (captured_name) {
        // Update the appropriate variable
        if (var_ptr) {
            // Individual variable (cheats, system controls, save states)
            if (*var_ptr) free(*var_ptr);
            *var_ptr = strdup(captured_name);
            gtk_button_set_label(GTK_BUTTON(button), captured_name);
        } else {
            // Indexed control (SNES buttons)
            if (g_gamepad_controls[index]) free(g_gamepad_controls[index]);
            g_gamepad_controls[index] = strdup(captured_name);

            // Update button label
            char btn_label[128];
            snprintf(btn_label, sizeof(btn_label), "%s: %s", kControlNames[index], captured_name);
            gtk_button_set_label(GTK_BUTTON(button), btn_label);
        }
    }

    LauncherGamepad_Close(gamepads[0].controller);
    gtk_widget_destroy(dialog);
}

// Create GamepadMap tab with interactive buttons
static GtkWidget* create_gamepadmap_tab(const Config *config) {
    (void)config;

    // Create notebook for subtabs
    GtkWidget *notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);

    // --- Controls Subtab ---
    GtkWidget *controls_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(controls_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    GtkWidget *controls_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(controls_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(controls_grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(controls_grid), 10);

    GtkWidget *controls_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(controls_title), "<b>SNES Controller</b>");
    gtk_widget_set_halign(controls_title, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(controls_grid), controls_title, 0, 0, 3, 1);

    for (int i = 0; i < NUM_SNES_BUTTONS; i++) {
        // Create label for control name
        char label_text[64];
        snprintf(label_text, sizeof(label_text), "%s:", kControlNames[i]);
        GtkWidget *label = gtk_label_new(label_text);
        gtk_widget_set_halign(label, GTK_ALIGN_END);
        gtk_grid_attach(GTK_GRID(controls_grid), label, 0, i + 1, 1, 1);

        // Create button showing the gamepad button
        const char *btn = g_gamepad_controls[i] ? g_gamepad_controls[i] : "(not set)";
        GtkWidget *button = gtk_button_new_with_label(btn);
        gtk_widget_set_size_request(button, BUTTON_WIDTH, BUTTON_HEIGHT);
        g_signal_connect(button, "clicked", G_CALLBACK(on_gamepad_button_clicked), GINT_TO_POINTER(i));
        gtk_grid_attach(GTK_GRID(controls_grid), button, 1, i + 1, 1, 1);

        // Add Clear button
        GtkWidget *clear_button = gtk_button_new_with_label("Clear");
        gtk_widget_set_size_request(clear_button, CLEAR_BUTTON_WIDTH, BUTTON_HEIGHT);
        ClearButtonData *clear_data = malloc(sizeof(ClearButtonData));
        if (clear_data) {
            clear_data->button = button;
            clear_data->variable = &g_gamepad_controls[i];
            g_signal_connect(clear_button, "clicked", G_CALLBACK(on_clear_button_clicked), clear_data);
            g_signal_connect_swapped(clear_button, "destroy", G_CALLBACK(free), clear_data);
        }
        gtk_grid_attach(GTK_GRID(controls_grid), clear_button, 2, i + 1, 1, 1);
    }

    gtk_container_add(GTK_CONTAINER(controls_scroll), controls_grid);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), controls_scroll, gtk_label_new("Controls"));

    // --- Save States Subtab ---
    GtkWidget *states_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(states_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    g_widgets.gamepad_states_grid = gtk_grid_new();
    GtkWidget *states_grid = g_widgets.gamepad_states_grid;
    gtk_grid_set_row_spacing(GTK_GRID(states_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(states_grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(states_grid), 10);

    GtkWidget *states_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(states_title), "<b>Save States</b>");
    gtk_widget_set_halign(states_title, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(states_grid), states_title, 0, 0, 2, 1);

    int row = 1;
    const char *state_labels[] = {"Load", "Save", "Replay"};
    char **state_arrays[] = {g_gamepad_load, g_gamepad_save, g_gamepad_replay};

    for (int type = 0; type < 3; type++) {
        GtkWidget *type_label = gtk_label_new(NULL);
        char markup[64];
        snprintf(markup, sizeof(markup), "<b>%s:</b>", state_labels[type]);
        gtk_label_set_markup(GTK_LABEL(type_label), markup);
        gtk_widget_set_halign(type_label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(states_grid), type_label, 0, row++, 3, 1);

        for (int i = 0; i < NUM_SAVE_SLOTS; i++) {
            char slot_label[64];
            snprintf(slot_label, sizeof(slot_label), "%s Slot %d:", state_labels[type], i + 1);
            GtkWidget *label = gtk_label_new(slot_label);
            gtk_widget_set_halign(label, GTK_ALIGN_END);
            gtk_grid_attach(GTK_GRID(states_grid), label, 0, row, 1, 1);

            const char *value;
            if (!state_arrays[type][i] || !*state_arrays[type][i]) {
                value = "(not set)";
            } else {
                value = state_arrays[type][i];
            }
            GtkWidget *button = gtk_button_new_with_label(value);
            gtk_widget_set_size_request(button, BUTTON_WIDTH, BUTTON_HEIGHT);
            g_signal_connect(button, "clicked", G_CALLBACK(on_gamepad_button_clicked), NULL);
            g_object_set_data(G_OBJECT(button), "variable_ptr", &state_arrays[type][i]);
            g_object_set_data(G_OBJECT(button), "prompt", slot_label);
            gtk_grid_attach(GTK_GRID(states_grid), button, 1, row, 1, 1);
            g_object_set_data(G_OBJECT(states_grid), g_strdup_printf("state_%d_%d", type, i), button);

            // Add Clear button
            GtkWidget *clear_button = gtk_button_new_with_label("Clear");
            gtk_widget_set_size_request(clear_button, CLEAR_BUTTON_WIDTH, BUTTON_HEIGHT);
            ClearButtonData *clear_data = malloc(sizeof(ClearButtonData));
            if (clear_data) {
                clear_data->button = button;
                clear_data->variable = &state_arrays[type][i];
                g_signal_connect(clear_button, "clicked", G_CALLBACK(on_clear_button_clicked), clear_data);
                g_signal_connect_swapped(clear_button, "destroy", G_CALLBACK(free), clear_data);
            }
            gtk_grid_attach(GTK_GRID(states_grid), clear_button, 2, row, 1, 1);

            row++;
        }
    }

    gtk_container_add(GTK_CONTAINER(states_scroll), states_grid);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), states_scroll, gtk_label_new("Save States"));

    // --- Cheats Subtab ---
    GtkWidget *cheats_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cheats_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    g_widgets.gamepad_cheats_grid = gtk_grid_new();
    GtkWidget *cheats_grid = g_widgets.gamepad_cheats_grid;
    gtk_grid_set_row_spacing(GTK_GRID(cheats_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(cheats_grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(cheats_grid), 10);

    GtkWidget *cheats_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(cheats_title), "<b>Cheats</b>");
    gtk_widget_set_halign(cheats_title, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(cheats_grid), cheats_title, 0, 0, 3, 1);

    row = 1;
    struct { const char *label; char **ptr; } cheats[] = {
        {"Refill Health & Magic:", &g_gamepad_cheat_life},
        {"Set key count to 1:", &g_gamepad_cheat_keys},
        {"Toggle Walk Through Walls:", &g_gamepad_cheat_walkthrough}
    };

    for (int i = 0; i < 3; i++) {
        GtkWidget *label = gtk_label_new(cheats[i].label);
        gtk_widget_set_halign(label, GTK_ALIGN_END);
        gtk_grid_attach(GTK_GRID(cheats_grid), label, 0, row, 1, 1);

        const char *value;
        if (!*cheats[i].ptr || !**cheats[i].ptr) {
            value = "(not set)";
        } else {
            value = *cheats[i].ptr;
        }
        GtkWidget *button = gtk_button_new_with_label(value);
        gtk_widget_set_size_request(button, BUTTON_WIDTH, BUTTON_HEIGHT);
        g_signal_connect(button, "clicked", G_CALLBACK(on_gamepad_button_clicked), NULL);
        g_object_set_data(G_OBJECT(button), "variable_ptr", cheats[i].ptr);
        g_object_set_data(G_OBJECT(button), "prompt", (gpointer)cheats[i].label);
        gtk_grid_attach(GTK_GRID(cheats_grid), button, 1, row, 1, 1);
        g_object_set_data(G_OBJECT(cheats_grid), g_strdup_printf("cheat_%d", i), button);

        // Add Clear button
        GtkWidget *clear_button = gtk_button_new_with_label("Clear");
        gtk_widget_set_size_request(clear_button, CLEAR_BUTTON_WIDTH, BUTTON_HEIGHT);
        ClearButtonData *clear_data = malloc(sizeof(ClearButtonData));
        if (clear_data) {
            clear_data->button = button;
            clear_data->variable = cheats[i].ptr;
            g_signal_connect(clear_button, "clicked", G_CALLBACK(on_clear_button_clicked), clear_data);
            g_signal_connect_swapped(clear_button, "destroy", G_CALLBACK(free), clear_data);
        }
        gtk_grid_attach(GTK_GRID(cheats_grid), clear_button, 2, row, 1, 1);

        row++;
    }

    gtk_container_add(GTK_CONTAINER(cheats_scroll), cheats_grid);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), cheats_scroll, gtk_label_new("Cheats"));

    // --- System Subtab ---
    GtkWidget *system_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(system_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    g_widgets.gamepad_system_grid = gtk_grid_new();
    GtkWidget *system_grid = g_widgets.gamepad_system_grid;
    gtk_grid_set_row_spacing(GTK_GRID(system_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(system_grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(system_grid), 10);

    GtkWidget *system_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(system_title), "<b>System Controls</b>");
    gtk_widget_set_halign(system_title, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(system_grid), system_title, 0, 0, 3, 1);

    row = 1;
    struct { const char *label; char **ptr; } system_keys[] = {
        {"Toggle Fullscreen:", &g_gamepad_fullscreen},
        {"Reset:", &g_gamepad_reset},
        {"Pause (Dimmed):", &g_gamepad_pause_dimmed},
        {"Pause:", &g_gamepad_pause},
        {"Turbo:", &g_gamepad_turbo},
        {"Replay Turbo:", &g_gamepad_replay_turbo},
        {"Window Bigger:", &g_gamepad_window_bigger},
        {"Window Smaller:", &g_gamepad_window_smaller},
        {"Volume Up:", &g_gamepad_volume_up},
        {"Volume Down:", &g_gamepad_volume_down},
        {"Display FPS:", &g_gamepad_display_perf},
        {"Toggle Renderer:", &g_gamepad_toggle_renderer},
        {"Stop Replay:", &g_gamepad_stop_replay},
        {"Clear input recording log (debug):", &g_gamepad_clear_keylog}
    };

    for (int i = 0; i < NUM_SYSTEM_KEYS; i++) {
        GtkWidget *label = gtk_label_new(system_keys[i].label);
        gtk_widget_set_halign(label, GTK_ALIGN_END);
        gtk_grid_attach(GTK_GRID(system_grid), label, 0, row, 1, 1);

        const char *value;
        if (!*system_keys[i].ptr || !**system_keys[i].ptr) {
            value = "(not set)";
        } else {
            value = *system_keys[i].ptr;
        }
        GtkWidget *button = gtk_button_new_with_label(value);
        gtk_widget_set_size_request(button, BUTTON_WIDTH, BUTTON_HEIGHT);
        g_signal_connect(button, "clicked", G_CALLBACK(on_gamepad_button_clicked), NULL);
        g_object_set_data(G_OBJECT(button), "variable_ptr", system_keys[i].ptr);
        g_object_set_data(G_OBJECT(button), "prompt", (gpointer)system_keys[i].label);
        gtk_grid_attach(GTK_GRID(system_grid), button, 1, row, 1, 1);
        g_object_set_data(G_OBJECT(system_grid), g_strdup_printf("system_%d", i), button);

        // Add Clear button
        GtkWidget *clear_button = gtk_button_new_with_label("Clear");
        gtk_widget_set_size_request(clear_button, CLEAR_BUTTON_WIDTH, BUTTON_HEIGHT);
        ClearButtonData *clear_data = malloc(sizeof(ClearButtonData));
        if (clear_data) {
            clear_data->button = button;
            clear_data->variable = system_keys[i].ptr;
            g_signal_connect(clear_button, "clicked", G_CALLBACK(on_clear_button_clicked), clear_data);
            g_signal_connect_swapped(clear_button, "destroy", G_CALLBACK(free), clear_data);
        }
        gtk_grid_attach(GTK_GRID(system_grid), clear_button, 2, row, 1, 1);

        row++;
    }

    gtk_container_add(GTK_CONTAINER(system_scroll), system_grid);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), system_scroll, gtk_label_new("System"));

    return notebook;
}

// Create main window with tabs
GtkWidget* LauncherUI_CreateWindow(Config *config) {
    // Create main window
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Zelda3 Launcher");
    gtk_window_set_default_size(GTK_WINDOW(window), LAUNCHER_DEFAULT_WIDTH, LAUNCHER_DEFAULT_HEIGHT);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    // Set window type hint to ensure it appears on top (especially on macOS)
    gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);

    // Create vertical box container
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Create notebook (tabs)
    GtkWidget *notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

    // Add tabs
    GtkWidget *general_tab = create_general_tab(config);
    GtkWidget *graphics_tab = create_graphics_tab(config);
    GtkWidget *sound_tab = create_sound_tab(config);
    GtkWidget *features_tab = create_features_tab(config);
    GtkWidget *keymap_tab = create_keymap_tab(config);
    GtkWidget *gamepad_tab = create_gamepadmap_tab(config);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), general_tab, gtk_label_new("General"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), graphics_tab, gtk_label_new("Graphics"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), sound_tab, gtk_label_new("Sound"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), features_tab, gtk_label_new("Features"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), keymap_tab, gtk_label_new("Keyboard"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), gamepad_tab, gtk_label_new("Gamepad"));

    return window;
}

// Update config from UI widgets
void LauncherUI_UpdateConfigFromUI(Config *config) {
    // General - Language
    GtkTreeIter iter;
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(g_widgets.language_combo), &iter)) {
        GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(g_widgets.language_combo));
        gchar *lang_code;
        gtk_tree_model_get(model, &iter, 1, &lang_code, -1);
        if (config->language) free((void*)config->language);
        config->language = (lang_code && *lang_code) ? strdup(lang_code) : NULL;
        g_free(lang_code);
    }

    // Graphics
    config->output_method = gtk_combo_box_get_active(GTK_COMBO_BOX(g_widgets.output_method));

    // Window size: Auto (0) or Custom (1)
    int window_size_mode = gtk_combo_box_get_active(GTK_COMBO_BOX(g_widgets.window_size_mode));
    if (window_size_mode == 0) {  // Auto
        config->window_width = 0;
        config->window_height = 0;
    } else {  // Custom
        config->window_width = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(g_widgets.window_width));
        config->window_height = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(g_widgets.window_height));
    }
    config->window_scale = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(g_widgets.window_scale));

    // Read fullscreen from radio button group
    // GTK radio button groups are stored in reverse order (last added = index 0).
    // Our buttons were added: Windowed(0), Fullscreen(1), Borderless(2)
    // So the group order is: [Borderless, Fullscreen, Windowed] at indices [0, 1, 2]
    // We need to reverse the index to get the correct config value.
    GSList *fs_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(g_widgets.fullscreen));
    for (int i = 0; i < 3; i++) {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_slist_nth_data(fs_group, i)))) {
            config->fullscreen = 2 - i;
            break;
        }
    }

    config->extended_aspect_ratio = gtk_combo_box_get_active(GTK_COMBO_BOX(g_widgets.aspect_ratio));
    config->ignore_aspect_ratio = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.ignore_aspect_ratio));
    config->extend_y = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.extend_y));
    config->linear_filtering = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.linear_filtering));
    config->new_renderer = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.new_renderer));
    config->enhanced_mode7 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.enhanced_mode7));
    config->no_sprite_limits = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.no_sprite_limits));

    // Shader path
    const char *shader_text = gtk_entry_get_text(GTK_ENTRY(g_widgets.shader_path_entry));
    if (config->shader) free((void*)config->shader);
    config->shader = (shader_text && *shader_text) ? strdup(shader_text) : NULL;

    // Sound
    config->enable_audio = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.enable_audio));

    // Map freq index to value
    int freq_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(g_widgets.audio_freq));
    const uint16 freqs[] = {11025, 22050, 32000, 44100, 48000};
    config->audio_freq = freqs[freq_idx];

    // Read audio channels from radio button group (1=Mono, 2=Stereo)
    GSList *ch_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(g_widgets.audio_channels));
    config->audio_channels = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_slist_nth_data(ch_group, 0))) ? 2 : 1;

    // Map samples index to value
    int samples_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(g_widgets.audio_samples));
    const int samples[] = {256, 512, 1024, 2048, 4096};
    config->audio_samples = samples[samples_idx];
    config->resume_msu = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.resume_msu));
    config->msuvolume = (int)gtk_range_get_value(GTK_RANGE(g_widgets.msu_volume_spin));

    // MSU enable: Disabled, PCM, PCM Deluxe, Opuz, Opuz Deluxe
    int msu_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(g_widgets.enable_msu));
    const int msu_values[] = {
        0,                                                    // Disabled
        kMsuEnabled_Msu,                                      // PCM
        kMsuEnabled_Msu | kMsuEnabled_MsuDeluxe,             // PCM Deluxe
        kMsuEnabled_Msu | kMsuEnabled_Opuz,                  // Opuz
        kMsuEnabled_Msu | kMsuEnabled_MsuDeluxe | kMsuEnabled_Opuz  // Opuz Deluxe
    };
    config->enable_msu = msu_values[msu_idx];

    // MSU path
    const char *msu_path_text = gtk_entry_get_text(GTK_ENTRY(g_widgets.msu_path_entry));
    if (config->msu_path) free((void*)config->msu_path);
    config->msu_path = (msu_path_text && *msu_path_text) ? strdup(msu_path_text) : NULL;

    // Features
    config->features0 = 0;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.feat_switch_lr)))
        config->features0 |= kFeatures0_SwitchLR;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.feat_switch_lr_limit)))
        config->features0 |= kFeatures0_SwitchLRLimit;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.feat_turn_dash)))
        config->features0 |= kFeatures0_TurnWhileDashing;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.feat_mirror_dw)))
        config->features0 |= kFeatures0_MirrorToDarkworld;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.feat_sword_collect)))
        config->features0 |= kFeatures0_CollectItemsWithSword;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.feat_sword_pots)))
        config->features0 |= kFeatures0_BreakPotsWithSword;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.feat_more_bombs)))
        config->features0 |= kFeatures0_MoreActiveBombs;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.feat_more_rupees)))
        config->features0 |= kFeatures0_CarryMoreRupees;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.feat_cancel_bird)))
        config->features0 |= kFeatures0_CancelBirdTravel;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.feat_no_beep)))
        config->features0 |= kFeatures0_DisableLowHealthBeep;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.feat_skip_intro)))
        config->features0 |= kFeatures0_SkipIntroOnKeypress;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.feat_yellow_items)))
        config->features0 |= kFeatures0_ShowMaxItemsInYellow;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.feat_misc_bugs)))
        config->features0 |= kFeatures0_MiscBugFixes;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.feat_game_bugs)))
        config->features0 |= kFeatures0_GameChangingBugFixes;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.feat_pokemode)))
        config->features0 |= kFeatures0_Pokemode;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.feat_zelda_helps)))
        config->features0 |= kFeatures0_PrincessZeldaHelps;

    // Read keyboard save state bindings (30 buttons)
    char **state_arrays[] = {g_kbd_load, g_kbd_save, g_kbd_replay};
    for (int type = 0; type < 3; type++) {
        for (int i = 0; i < NUM_SAVE_SLOTS; i++) {
            char key[32];
            snprintf(key, sizeof(key), "state_%d_%d", type, i);
            GtkWidget *button = g_object_get_data(G_OBJECT(g_widgets.kbd_states_grid), key);
            if (button) {
                if (state_arrays[type][i]) free(state_arrays[type][i]);
                state_arrays[type][i] = strdup(gtk_button_get_label(GTK_BUTTON(button)));
            }
        }
    }

    // Read keyboard cheat bindings (3 buttons)
    char **cheat_ptrs[] = {&g_kbd_cheat_life, &g_kbd_cheat_keys, &g_kbd_cheat_walkthrough};
    for (int i = 0; i < 3; i++) {
        char key[32];
        snprintf(key, sizeof(key), "cheat_%d", i);
        GtkWidget *button = g_object_get_data(G_OBJECT(g_widgets.kbd_cheats_grid), key);
        if (button) {
            if (*cheat_ptrs[i]) free(*cheat_ptrs[i]);
            *cheat_ptrs[i] = strdup(gtk_button_get_label(GTK_BUTTON(button)));
        }
    }

    // Read keyboard system bindings (14 buttons)
    char **system_ptrs[] = {
        &g_kbd_fullscreen,
        &g_kbd_reset,
        &g_kbd_pause_dimmed,
        &g_kbd_pause,
        &g_kbd_turbo,
        &g_kbd_replay_turbo,
        &g_kbd_window_bigger,
        &g_kbd_window_smaller,
        &g_kbd_volume_up,
        &g_kbd_volume_down,
        &g_kbd_display_perf,
        &g_kbd_toggle_renderer,
        &g_kbd_stop_replay,
        &g_kbd_clear_keylog
    };
    for (int i = 0; i < NUM_SYSTEM_KEYS; i++) {
        char key[32];
        snprintf(key, sizeof(key), "system_%d", i);
        GtkWidget *button = g_object_get_data(G_OBJECT(g_widgets.kbd_system_grid), key);
        if (button) {
            if (*system_ptrs[i]) free(*system_ptrs[i]);
            *system_ptrs[i] = strdup(gtk_button_get_label(GTK_BUTTON(button)));
        }
    }

    // Read gamepad save state bindings (30 buttons)
    char **gamepad_state_arrays[] = {g_gamepad_load, g_gamepad_save, g_gamepad_replay};
    for (int type = 0; type < 3; type++) {
        for (int i = 0; i < NUM_SAVE_SLOTS; i++) {
            char key[32];
            snprintf(key, sizeof(key), "state_%d_%d", type, i);
            GtkWidget *button = g_object_get_data(G_OBJECT(g_widgets.gamepad_states_grid), key);
            if (button) {
                if (gamepad_state_arrays[type][i]) free(gamepad_state_arrays[type][i]);
                gamepad_state_arrays[type][i] = strdup(gtk_button_get_label(GTK_BUTTON(button)));
            }
        }
    }

    // Read gamepad cheat bindings (3 buttons)
    char **gamepad_cheat_ptrs[] = {&g_gamepad_cheat_life, &g_gamepad_cheat_keys, &g_gamepad_cheat_walkthrough};
    for (int i = 0; i < 3; i++) {
        char key[32];
        snprintf(key, sizeof(key), "cheat_%d", i);
        GtkWidget *button = g_object_get_data(G_OBJECT(g_widgets.gamepad_cheats_grid), key);
        if (button) {
            if (*gamepad_cheat_ptrs[i]) free(*gamepad_cheat_ptrs[i]);
            *gamepad_cheat_ptrs[i] = strdup(gtk_button_get_label(GTK_BUTTON(button)));
        }
    }

    // Read gamepad system bindings (14 buttons)
    char **gamepad_system_ptrs[] = {
        &g_gamepad_fullscreen,
        &g_gamepad_reset,
        &g_gamepad_pause_dimmed,
        &g_gamepad_pause,
        &g_gamepad_turbo,
        &g_gamepad_replay_turbo,
        &g_gamepad_window_bigger,
        &g_gamepad_window_smaller,
        &g_gamepad_volume_up,
        &g_gamepad_volume_down,
        &g_gamepad_display_perf,
        &g_gamepad_toggle_renderer,
        &g_gamepad_stop_replay,
        &g_gamepad_clear_keylog
    };
    for (int i = 0; i < NUM_SYSTEM_KEYS; i++) {
        char key[32];
        snprintf(key, sizeof(key), "system_%d", i);
        GtkWidget *button = g_object_get_data(G_OBJECT(g_widgets.gamepad_system_grid), key);
        if (button) {
            if (*gamepad_system_ptrs[i]) free(*gamepad_system_ptrs[i]);
            *gamepad_system_ptrs[i] = strdup(gtk_button_get_label(GTK_BUTTON(button)));
        }
    }
}
