#include "launcher_ui.h"
#include "../config.h"
#include "../game_features.h"
#include "launcher_gamepad.h"
#include "../logging.h"
#include "restool_lib.h"
#ifdef HAVE_OPUS_ENCODER
#include "opus_encoder_lib.h"
#endif
#include <gtk/gtk.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

// Language ROM entry for multi-language support
typedef struct {
    char path[512];
    char lang_code[16];
    char lang_name[64];
    bool valid;
} LanguageRomEntry;

#define MAX_LANGUAGE_ROMS 12
static LanguageRomEntry g_lang_roms[MAX_LANGUAGE_ROMS];
static int g_lang_rom_count = 0;
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
    // Main window buttons (set from launcher_main.c)
    GtkWidget *launch_button;

    // General tab
    GtkWidget *dat_status_label;
    GtkWidget *language_combo;
    GtkWidget *rom_path_entry;
    GtkWidget *make_dat_status;
    GtkWidget *lang_roms_listbox;
    GtkWidget *lang_roms_browse_btn;
    GtkWidget *lang_roms_clear_all_btn;
    GtkWidget *make_dat_btn;

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
    GtkWidget *custom_aspect_box;
    GtkWidget *custom_aspect_w;
    GtkWidget *custom_aspect_h;
    GtkWidget *ignore_aspect_ratio;
    GtkWidget *extend_y;
    GtkWidget *linear_filtering;
    GtkWidget *new_renderer;
    GtkWidget *enhanced_mode7;
    GtkWidget *no_sprite_limits;
    GtkWidget *display_perf_title;
    GtkWidget *disable_frame_delay;
    GtkWidget *dim_flashes;
    GtkWidget *link_graphics_entry;

    // Sound tab
    GtkWidget *enable_audio;
    GtkWidget *audio_freq;
    GtkWidget *audio_channels;
    GtkWidget *audio_samples;
    GtkWidget *enable_msu;
    GtkWidget *resume_msu;
    GtkWidget *msu_volume_spin;

    // Features tab
    GtkWidget *feat_autosave;
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
    GtkWidget *msu_info_label;
    GtkWidget *shader_path_entry;
#ifdef HAVE_OPUS_ENCODER
    GtkWidget *encode_opus_btn;
#endif
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

// Signal handler for aspect ratio dropdown
static void on_aspect_ratio_changed(GtkComboBox *combo, gpointer user_data) {
    (void)user_data;
    int mode = gtk_combo_box_get_active(combo);
    bool is_custom = (mode == 4);  // 0=4:3, 1=16:9, 2=16:10, 3=18:9, 4=Custom

    if (is_custom) {
        // Show all children explicitly since no_show_all is set
        gtk_widget_show(g_widgets.custom_aspect_box);
        gtk_widget_show(g_widgets.custom_aspect_w);
        gtk_widget_show(g_widgets.custom_aspect_h);
        gtk_container_foreach(GTK_CONTAINER(g_widgets.custom_aspect_box),
                              (GtkCallback)gtk_widget_show, NULL);
    } else {
        gtk_widget_hide(g_widgets.custom_aspect_box);
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

// Signal handler for Link graphics browse button
static void on_link_graphics_browse_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;

    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Select Link Sprite File",
        NULL,
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Open", GTK_RESPONSE_ACCEPT,
        NULL);

    // Add file filter for ZSPR files
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "ZSPR Sprite Files (*.zspr)");
    gtk_file_filter_add_pattern(filter, "*.zspr");
    gtk_file_filter_add_pattern(filter, "*.ZSPR");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    // Add "All files" filter
    GtkFileFilter *filter_all = gtk_file_filter_new();
    gtk_file_filter_set_name(filter_all, "All files");
    gtk_file_filter_add_pattern(filter_all, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_all);

    // Set current file if one is already set
    const char *current_path = gtk_entry_get_text(GTK_ENTRY(g_widgets.link_graphics_entry));
    if (current_path && *current_path) {
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), current_path);
    }

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_text(GTK_ENTRY(g_widgets.link_graphics_entry), filename);
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

// MSU detection result
typedef struct {
    int format_flags;     // kMsuEnabled_* combination
    int file_count;       // Number of MSU files found
    char prefix[256];     // Detected prefix (e.g., "ALttP-msu-Deluxe-")
} MsuScanResult;

// Case-insensitive string suffix check
static bool str_ends_with_ci(const char *str, const char *suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return false;
    const char *str_suffix = str + str_len - suffix_len;
    for (size_t i = 0; i < suffix_len; i++) {
        if (tolower((unsigned char)str_suffix[i]) != tolower((unsigned char)suffix[i]))
            return false;
    }
    return true;
}

// Extract track number from filename like "alttp_msu-42.pcm" -> 42
// Returns 0 if no track number found
static int extract_track_number(const char *filename) {
    // Find the last digit sequence before the extension
    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;

    // Walk backwards from extension to find digits
    const char *p = ext - 1;
    while (p >= filename && *p >= '0' && *p <= '9') p--;
    p++;  // Point to first digit

    if (p >= ext) return 0;  // No digits found
    return atoi(p);
}

// Extract prefix from filename like "alttp_msu-42.pcm" -> "alttp_msu-"
static void extract_prefix(const char *filename, char *out, size_t out_size) {
    const char *ext = strrchr(filename, '.');
    if (!ext) {
        out[0] = '\0';
        return;
    }

    // Find where digits start (walk backwards from extension)
    const char *p = ext - 1;
    while (p >= filename && *p >= '0' && *p <= '9') p--;
    p++;  // Point to first digit

    size_t prefix_len = p - filename;
    if (prefix_len >= out_size) prefix_len = out_size - 1;
    memcpy(out, filename, prefix_len);
    out[prefix_len] = '\0';
}

// Get format name for display
static const char* get_msu_format_name(int flags) {
    bool is_opuz = (flags & kMsuEnabled_Opuz) != 0;
    bool is_deluxe = (flags & kMsuEnabled_MsuDeluxe) != 0;

    if (is_opuz && is_deluxe) return "Opuz Deluxe";
    if (is_opuz) return "Opuz";
    if (is_deluxe) return "PCM Deluxe";
    return "PCM";
}

// Convert format flags to dropdown index
static int flags_to_dropdown_index(int flags) {
    bool is_opuz = (flags & kMsuEnabled_Opuz) != 0;
    bool is_deluxe = (flags & kMsuEnabled_MsuDeluxe) != 0;

    if (is_opuz && is_deluxe) return 4;  // Opuz Deluxe
    if (is_opuz) return 3;               // Opuz
    if (is_deluxe) return 2;             // PCM Deluxe
    return 1;                            // PCM
}

#ifdef HAVE_OPUS_ENCODER
// Encoding state for progress tracking
typedef struct {
    GtkWidget *dialog;
    GtkWidget *progress_bar;
    GtkWidget *status_label;
    bool keep_pcm_files;
    bool cancelled;
    int current_file;
    int total_files;
    int success_count;
    char folder_path[512];
    char prefix[256];
} EncodingState;

static EncodingState g_encoding_state;

// Progress callback for opus encoder
static bool on_encoding_progress(float progress, void *user_data) {
    EncodingState *state = (EncodingState *)user_data;

    // Calculate overall progress: file progress within total files
    float overall = ((float)(state->current_file - 1) + progress) / (float)state->total_files;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(state->progress_bar), overall);

    // Process GTK events to keep UI responsive
    while (gtk_events_pending()) gtk_main_iteration();

    return !state->cancelled;
}

// Progress dialog cancel handler
static void on_progress_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    (void)dialog;
    (void)user_data;
    if (response_id == GTK_RESPONSE_CANCEL || response_id == GTK_RESPONSE_DELETE_EVENT) {
        g_encoding_state.cancelled = true;
    }
}

// Get list of PCM track numbers in folder
static int* get_pcm_track_numbers(const char *folder_path, const char *prefix, int *count) {
    DIR *dir = opendir(folder_path);
    if (!dir) {
        *count = 0;
        return NULL;
    }

    // First pass: count PCM files
    int file_count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!str_ends_with_ci(entry->d_name, ".pcm")) continue;
        if (strncmp(entry->d_name, prefix, strlen(prefix)) != 0) continue;
        file_count++;
    }

    if (file_count == 0) {
        closedir(dir);
        *count = 0;
        return NULL;
    }

    int *tracks = malloc(file_count * sizeof(int));
    if (!tracks) {
        closedir(dir);
        *count = 0;
        return NULL;
    }

    // Second pass: collect track numbers
    rewinddir(dir);
    int idx = 0;
    while ((entry = readdir(dir)) != NULL && idx < file_count) {
        if (!str_ends_with_ci(entry->d_name, ".pcm")) continue;
        if (strncmp(entry->d_name, prefix, strlen(prefix)) != 0) continue;
        int track = extract_track_number(entry->d_name);
        if (track > 0) {
            tracks[idx++] = track;
        }
    }
    closedir(dir);

    *count = idx;
    return tracks;
}

// Move PCM files to pcm_original subdirectory
static bool move_pcm_files_to_backup(const char *folder_path, const char *prefix) {
    char backup_dir[600];
    snprintf(backup_dir, sizeof(backup_dir), "%s/pcm_original", folder_path);

    // Create backup directory
#ifdef _WIN32
    _mkdir(backup_dir);
#else
    mkdir(backup_dir, 0755);
#endif

    DIR *dir = opendir(folder_path);
    if (!dir) return false;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!str_ends_with_ci(entry->d_name, ".pcm")) continue;
        if (strncmp(entry->d_name, prefix, strlen(prefix)) != 0) continue;

        char src_path[1024], dst_path[1024];
        snprintf(src_path, sizeof(src_path), "%s/%s", folder_path, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", backup_dir, entry->d_name);

        rename(src_path, dst_path);
    }
    closedir(dir);
    return true;
}

// Delete PCM files from folder
static bool delete_pcm_files(const char *folder_path, const char *prefix) {
    DIR *dir = opendir(folder_path);
    if (!dir) return false;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!str_ends_with_ci(entry->d_name, ".pcm")) continue;
        if (strncmp(entry->d_name, prefix, strlen(prefix)) != 0) continue;

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", folder_path, entry->d_name);
        remove(path);
    }
    closedir(dir);
    return true;
}

// Forward declaration
static MsuScanResult scan_msu_folder(const char *folder_path);

// Encode PCM files to Opus
// Returns the list of track numbers that were encoded (caller must free)
static int* encode_pcm_files(const char *folder_path, const char *prefix, int *out_track_count) {
    // Get list of track numbers from the original folder
    int track_count;
    int *tracks = get_pcm_track_numbers(folder_path, prefix, &track_count);

    if (!tracks || track_count == 0) {
        free(tracks);
        *out_track_count = 0;
        return NULL;
    }

    g_encoding_state.total_files = track_count;
    g_encoding_state.current_file = 0;
    g_encoding_state.success_count = 0;

    // Encode each track from the original folder
    for (int i = 0; i < track_count && !g_encoding_state.cancelled; i++) {
        g_encoding_state.current_file = i + 1;

        // Update status label
        char status[128];
        snprintf(status, sizeof(status), "Encoding track %d of %d...", i + 1, track_count);
        gtk_label_set_text(GTK_LABEL(g_encoding_state.status_label), status);
        while (gtk_events_pending()) gtk_main_iteration();

        // Build paths - read from original folder, write to same folder
        char input_path[1024], output_path[1024];
        snprintf(input_path, sizeof(input_path), "%s/%s%d.pcm", folder_path, prefix, tracks[i]);
        snprintf(output_path, sizeof(output_path), "%s/%s%d.opuz", folder_path, prefix, tracks[i]);

        // Encode with progress callback
        OpusEncoderOptionsEx opts = {
            .bitrate = 128000,
            .has_repeat = OpusEncoder_TrackHasRepeat(tracks[i]),
            .callback = on_encoding_progress,
            .callback_data = &g_encoding_state
        };

        int result = OpusEncoder_EncodeFileEx(input_path, output_path, &opts);
        if (result == OPUS_ENC_OK) {
            g_encoding_state.success_count++;
        } else if (result == OPUS_ENC_ERR_CANCELLED) {
            break;
        }
    }

    *out_track_count = track_count;
    return tracks;
}

// Signal handler for "Encode PCM to Opus" button
static void on_encode_opus_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;

    // Get MSU path and extract folder/prefix
    const char *msu_path = gtk_entry_get_text(GTK_ENTRY(g_widgets.msu_path_entry));
    if (!msu_path || !*msu_path) return;

    // Parse folder and prefix from full path (e.g., "/path/to/folder/alttp_msu-")
    char folder_path[512];
    char prefix[256];
    strncpy(folder_path, msu_path, sizeof(folder_path) - 1);
    folder_path[sizeof(folder_path) - 1] = '\0';

    char *last_slash = strrchr(folder_path, '/');
    if (last_slash) {
        strncpy(prefix, last_slash + 1, sizeof(prefix) - 1);
        prefix[sizeof(prefix) - 1] = '\0';
        *last_slash = '\0';
    } else {
        // No slash found, use current directory
        strncpy(prefix, folder_path, sizeof(prefix) - 1);
        prefix[sizeof(prefix) - 1] = '\0';
        strcpy(folder_path, ".");
    }

    // Re-scan to verify PCM files exist
    MsuScanResult scan = scan_msu_folder(folder_path);
    if (!(scan.format_flags & kMsuEnabled_Msu) || (scan.format_flags & kMsuEnabled_Opuz)) {
        // No PCM files or already Opus
        return;
    }

    // Show confirmation dialog
    GtkWidget *dialog = gtk_message_dialog_new(
        NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
        "The encoding will take a couple of minutes.\n\n"
        "Do you want to keep the PCM files after encoding has finished?");
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Keep PCM files", GTK_RESPONSE_YES);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Remove PCM files", GTK_RESPONSE_NO);

    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    if (response == GTK_RESPONSE_CANCEL || response == GTK_RESPONSE_DELETE_EVENT) return;

    bool keep_pcm = (response == GTK_RESPONSE_YES);

    // Store state
    strncpy(g_encoding_state.folder_path, folder_path, sizeof(g_encoding_state.folder_path) - 1);
    g_encoding_state.folder_path[sizeof(g_encoding_state.folder_path) - 1] = '\0';
    strncpy(g_encoding_state.prefix, prefix, sizeof(g_encoding_state.prefix) - 1);
    g_encoding_state.prefix[sizeof(g_encoding_state.prefix) - 1] = '\0';
    g_encoding_state.keep_pcm_files = keep_pcm;
    g_encoding_state.cancelled = false;

    // Create progress dialog
    GtkWidget *progress_dialog = gtk_dialog_new_with_buttons(
        "Encoding PCM to Opus", NULL, GTK_DIALOG_MODAL,
        "Cancel", GTK_RESPONSE_CANCEL, NULL);
    g_signal_connect(progress_dialog, "response",
                     G_CALLBACK(on_progress_dialog_response), NULL);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(progress_dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);

    GtkWidget *status_label = gtk_label_new("Starting encoding...");
    gtk_box_pack_start(GTK_BOX(content), status_label, FALSE, FALSE, 10);

    GtkWidget *progress_bar = gtk_progress_bar_new();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
    gtk_widget_set_size_request(progress_bar, 300, -1);
    gtk_box_pack_start(GTK_BOX(content), progress_bar, FALSE, FALSE, 10);

    gtk_widget_show_all(progress_dialog);

    // Store dialog widgets
    g_encoding_state.dialog = progress_dialog;
    g_encoding_state.progress_bar = progress_bar;
    g_encoding_state.status_label = status_label;

    // Run encoding (returns list of track numbers for cleanup)
    int track_count;
    int *tracks = encode_pcm_files(folder_path, prefix, &track_count);

    // Only handle PCM files if encoding completed successfully (all files encoded, not cancelled)
    bool encoding_complete = !g_encoding_state.cancelled &&
                             g_encoding_state.success_count == track_count &&
                             track_count > 0;

    if (encoding_complete) {
        if (keep_pcm) {
            // Move PCM files to backup folder
            move_pcm_files_to_backup(folder_path, prefix);
        } else {
            // Delete PCM files
            delete_pcm_files(folder_path, prefix);
        }
    }

    free(tracks);

    // Update dialog to show completion status
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 1.0);

    // Update status label with result
    char completion_msg[256];
    if (g_encoding_state.cancelled) {
        snprintf(completion_msg, sizeof(completion_msg),
                 "Encoding cancelled.\n%d of %d tracks were encoded.",
                 g_encoding_state.success_count, track_count);
    } else if (g_encoding_state.success_count == track_count) {
        snprintf(completion_msg, sizeof(completion_msg),
                 "Encoding complete!\n%d tracks successfully encoded to Opus.",
                 g_encoding_state.success_count);
    } else {
        snprintf(completion_msg, sizeof(completion_msg),
                 "Encoding finished with errors.\n%d of %d tracks encoded.",
                 g_encoding_state.success_count, track_count);
    }
    gtk_label_set_text(GTK_LABEL(status_label), completion_msg);

    // Change Cancel button to OK
    GtkWidget *cancel_btn = gtk_dialog_get_widget_for_response(GTK_DIALOG(progress_dialog),
                                                                GTK_RESPONSE_CANCEL);
    if (cancel_btn) {
        gtk_button_set_label(GTK_BUTTON(cancel_btn), "OK");
    }

    // Disconnect the cancel handler so clicking OK just closes
    g_signal_handlers_disconnect_by_func(progress_dialog,
                                         G_CALLBACK(on_progress_dialog_response), NULL);

    // Wait for user to click OK
    gtk_dialog_run(GTK_DIALOG(progress_dialog));

    // Cleanup progress dialog
    gtk_widget_destroy(progress_dialog);

    // Re-scan and update UI
    MsuScanResult result = scan_msu_folder(folder_path);

    // Update MSU path entry with new prefix if format detected
    if (result.file_count > 0 && result.prefix[0]) {
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", folder_path, result.prefix);
        gtk_entry_set_text(GTK_ENTRY(g_widgets.msu_path_entry), full_path);
    }

    // Update dropdown to detected format
    if (result.format_flags & kMsuEnabled_Opuz) {
        int msu_idx = flags_to_dropdown_index(result.format_flags);
        gtk_combo_box_set_active(GTK_COMBO_BOX(g_widgets.enable_msu), msu_idx);
    }

    // Update info label
    char info_msg[256];
    if (g_encoding_state.cancelled) {
        snprintf(info_msg, sizeof(info_msg), "Encoding cancelled. %d of %d tracks encoded.",
                 g_encoding_state.success_count, track_count);
    } else if (g_encoding_state.success_count == track_count) {
        snprintf(info_msg, sizeof(info_msg), "%d tracks encoded to Opus",
                 g_encoding_state.success_count);
    } else {
        snprintf(info_msg, sizeof(info_msg), "%d of %d tracks encoded (some failed)",
                 g_encoding_state.success_count, track_count);
    }
    gtk_label_set_text(GTK_LABEL(g_widgets.msu_info_label), info_msg);

    // Disable encode button only if all files were successfully encoded
    if (encoding_complete) {
        gtk_widget_set_sensitive(g_widgets.encode_opus_btn, FALSE);
    }
}
#endif // HAVE_OPUS_ENCODER

// Scan MSU folder and detect format, deluxe status, and file count
static MsuScanResult scan_msu_folder(const char *folder_path) {
    MsuScanResult result = {0, 0, ""};
    DIR *dir = opendir(folder_path);
    if (!dir) return result;

    int max_track = 0;
    bool format_detected = false;
    bool is_opuz = false;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;

        // Check for .pcm or .opuz extension
        bool is_pcm_file = str_ends_with_ci(name, ".pcm");
        bool is_opuz_file = str_ends_with_ci(name, ".opuz");
        if (!is_pcm_file && !is_opuz_file) continue;

        // Extract track number
        int track = extract_track_number(name);
        if (track <= 0) continue;

        // Count this file
        result.file_count++;

        // Track highest track number for Deluxe detection
        if (track > max_track) max_track = track;

        // On first valid file, extract prefix and detect format
        if (!format_detected) {
            extract_prefix(name, result.prefix, sizeof(result.prefix));

            // Read file header to verify format
            char filepath[1024];
            snprintf(filepath, sizeof(filepath), "%s/%s", folder_path, name);
            FILE *f = fopen(filepath, "rb");
            if (f) {
                uint8_t header[4];
                if (fread(header, 1, 4, f) == 4) {
                    // Check for OPUZ magic (0x4F50555A)
                    if (header[0] == 'O' && header[1] == 'P' &&
                        header[2] == 'U' && header[3] == 'Z') {
                        is_opuz = true;
                    }
                    // MSU1 magic (0x4D535531) is PCM - default
                }
                fclose(f);
            }
            format_detected = true;
        }
    }
    closedir(dir);

    // Build format flags
    if (result.file_count > 0) {
        result.format_flags = kMsuEnabled_Msu;
        if (is_opuz) result.format_flags |= kMsuEnabled_Opuz;
        if (max_track > 47) result.format_flags |= kMsuEnabled_MsuDeluxe;
    }

    return result;
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

        // Scan the folder for MSU files
        MsuScanResult result = scan_msu_folder(folder);

        char full_path[1024];
        if (result.file_count > 0 && result.prefix[0]) {
            // Build full path with detected prefix
            snprintf(full_path, sizeof(full_path), "%s/%s", folder, result.prefix);
            gtk_entry_set_text(GTK_ENTRY(g_widgets.msu_path_entry), full_path);

            // Update dropdown to detected format
            int msu_idx = flags_to_dropdown_index(result.format_flags);
            gtk_combo_box_set_active(GTK_COMBO_BOX(g_widgets.enable_msu), msu_idx);

            // Update info label
            char info_msg[256];
            snprintf(info_msg, sizeof(info_msg), "%d MSU track%s detected (%s)",
                     result.file_count,
                     result.file_count != 1 ? "s" : "",
                     get_msu_format_name(result.format_flags));
            gtk_label_set_text(GTK_LABEL(g_widgets.msu_info_label), info_msg);

#ifdef HAVE_OPUS_ENCODER
            // Enable encode button only if PCM files detected (not already Opus)
            bool has_pcm = (result.format_flags & kMsuEnabled_Msu) &&
                           !(result.format_flags & kMsuEnabled_Opuz);
            gtk_widget_set_sensitive(g_widgets.encode_opus_btn, has_pcm);
#endif
        } else {
            // Fallback to default prefix
            snprintf(full_path, sizeof(full_path), "%s/alttp_msu-", folder);
            gtk_entry_set_text(GTK_ENTRY(g_widgets.msu_path_entry), full_path);
            gtk_label_set_text(GTK_LABEL(g_widgets.msu_info_label), "No MSU files detected");
#ifdef HAVE_OPUS_ENCODER
            gtk_widget_set_sensitive(g_widgets.encode_opus_btn, FALSE);
#endif
        }

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
        {"fr-c", "French (Canada)"},
        {"en", "English (EU)"},
        {"es", "Spanish"},
        {"pl", "Polish"},
        {"pt", "Portuguese (Brazil)"},
        {"nl", "Dutch"},
        {"sv", "Swedish"},
        {"redux", "English (Redux)"},
        {"retrans-kal", "English (Kaleidoscope)"},
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

    bool dat_exists = Restool_DatFileExists(exe_dir);

    if (dat_exists) {
        gtk_label_set_markup(GTK_LABEL(g_widgets.dat_status_label),
            "<span foreground='green'>zelda3_assets.dat found</span>");
    } else {
        gtk_label_set_markup(GTK_LABEL(g_widgets.dat_status_label),
            "<span foreground='red'>zelda3_assets.dat not found - create from ROM below</span>");
    }

    // Enable/disable launch button based on dat file existence
    if (g_widgets.launch_button) {
        gtk_widget_set_sensitive(g_widgets.launch_button, dat_exists);
    }
}

void LauncherUI_SetLaunchButton(GtkWidget *button) {
    g_widgets.launch_button = button;
    // Update sensitivity immediately
    char exe_dir[512];
    LauncherUI_GetExecutableDir(exe_dir, sizeof(exe_dir));
    gtk_widget_set_sensitive(button, Restool_DatFileExists(exe_dir));
}

// ============================================================================
// Language ROM Management
// ============================================================================

// Forward declaration
static void refresh_lang_roms_list(void);

// Add a language ROM after validation
static void add_language_rom(const char *path) {
    if (g_lang_rom_count >= MAX_LANGUAGE_ROMS) {
        LogWarn("Maximum number of language ROMs reached (%d)", MAX_LANGUAGE_ROMS);
        return;
    }

    // Validate ROM using restool library
    RestoolRomInfo info;
    if (!Restool_IdentifyRom(path, &info)) {
        LogWarn("Failed to read ROM: %s", path);
        return;
    }

    // Check for US ROM (should use base ROM field instead)
    if (info.valid && strcmp(info.lang_code, "us") == 0) {
        GtkWidget *dialog = gtk_message_dialog_new(NULL,
            GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
            "US ROM detected.\n\nPlease use the 'ROM File' field above for the base USA ROM.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    // Check for duplicate language (replace if found)
    for (int i = 0; i < g_lang_rom_count; i++) {
        if (strcmp(g_lang_roms[i].lang_code, info.lang_code) == 0) {
            // Replace existing entry
            strncpy(g_lang_roms[i].path, path, sizeof(g_lang_roms[i].path) - 1);
            strncpy(g_lang_roms[i].lang_code, info.lang_code, sizeof(g_lang_roms[i].lang_code) - 1);
            strncpy(g_lang_roms[i].lang_name, info.lang_name, sizeof(g_lang_roms[i].lang_name) - 1);
            g_lang_roms[i].valid = info.valid;
            refresh_lang_roms_list();
            return;
        }
    }

    // Add new entry
    LanguageRomEntry *entry = &g_lang_roms[g_lang_rom_count++];
    strncpy(entry->path, path, sizeof(entry->path) - 1);
    strncpy(entry->lang_code, info.lang_code, sizeof(entry->lang_code) - 1);
    strncpy(entry->lang_name, info.lang_name, sizeof(entry->lang_name) - 1);
    entry->valid = info.valid;

    refresh_lang_roms_list();
}

// Clear a single language ROM entry
static void on_lang_rom_clear_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    int index = GPOINTER_TO_INT(user_data);

    if (index < 0 || index >= g_lang_rom_count) return;

    // Remove entry by shifting remaining entries
    for (int i = index; i < g_lang_rom_count - 1; i++) {
        g_lang_roms[i] = g_lang_roms[i + 1];
    }
    g_lang_rom_count--;

    refresh_lang_roms_list();
}

// Clear all language ROM entries
static void on_lang_roms_clear_all_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;

    g_lang_rom_count = 0;
    refresh_lang_roms_list();
}

// Browse for multiple language ROM files
static void on_lang_roms_browse_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;

    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Select Language ROM Files",
        NULL,
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Open", GTK_RESPONSE_ACCEPT,
        NULL);

    // Enable multi-file selection
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);

    // Add file filter for ROM files
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "SNES ROM files (*.sfc, *.smc)");
    gtk_file_filter_add_pattern(filter, "*.sfc");
    gtk_file_filter_add_pattern(filter, "*.smc");
    gtk_file_filter_add_pattern(filter, "*.SFC");
    gtk_file_filter_add_pattern(filter, "*.SMC");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    // Add "All files" filter
    GtkFileFilter *filter_all = gtk_file_filter_new();
    gtk_file_filter_set_name(filter_all, "All files");
    gtk_file_filter_add_pattern(filter_all, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_all);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        GSList *filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
        for (GSList *iter = filenames; iter != NULL; iter = iter->next) {
            add_language_rom((const char *)iter->data);
            g_free(iter->data);
        }
        g_slist_free(filenames);
    }

    gtk_widget_destroy(dialog);
}

// Drag-drop handler for ROM path entry
static void on_rom_path_drag_received(GtkWidget *widget,
                                       GdkDragContext *context,
                                       gint x, gint y,
                                       GtkSelectionData *data,
                                       guint info,
                                       guint time,
                                       gpointer user_data) {
    (void)widget; (void)x; (void)y; (void)info; (void)user_data;

    if (gtk_selection_data_get_length(data) < 0) {
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }

    gchar **uris = gtk_selection_data_get_uris(data);
    if (!uris) {
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }

    // Only use the first file
    if (uris[0]) {
        gchar *filename = g_filename_from_uri(uris[0], NULL, NULL);
        if (filename) {
            const char *ext = strrchr(filename, '.');
            if (ext && (strcasecmp(ext, ".sfc") == 0 || strcasecmp(ext, ".smc") == 0)) {
                gtk_entry_set_text(GTK_ENTRY(g_widgets.rom_path_entry), filename);
            }
            g_free(filename);
        }
    }

    g_strfreev(uris);
    gtk_drag_finish(context, TRUE, FALSE, time);
}

static void on_lang_roms_drag_received(GtkWidget *widget,
                                        GdkDragContext *context,
                                        gint x, gint y,
                                        GtkSelectionData *data,
                                        guint info,
                                        guint time,
                                        gpointer user_data) {
    (void)widget; (void)x; (void)y; (void)info; (void)user_data;

    if (gtk_selection_data_get_length(data) < 0) {
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }

    gchar **uris = gtk_selection_data_get_uris(data);
    if (!uris) {
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }

    for (int i = 0; uris[i] != NULL; i++) {
        gchar *filename = g_filename_from_uri(uris[i], NULL, NULL);
        if (filename) {
            // Filter for ROM file extensions
            const char *ext = strrchr(filename, '.');
            if (ext && (strcasecmp(ext, ".sfc") == 0 || strcasecmp(ext, ".smc") == 0)) {
                add_language_rom(filename);
            }
            g_free(filename);
        }
    }
    g_strfreev(uris);

    gtk_drag_finish(context, TRUE, FALSE, time);
}

// Shader file drag handler
static void on_shader_drag_received(GtkWidget *widget, GdkDragContext *context,
                                    gint x, gint y, GtkSelectionData *data,
                                    guint info, guint time, gpointer user_data) {
    (void)x; (void)y; (void)info; (void)user_data;
    gchar **uris = gtk_selection_data_get_uris(data);
    if (uris && uris[0]) {
        gchar *path = g_filename_from_uri(uris[0], NULL, NULL);
        if (path) {
            // Filter for .glsl/.glslp files
            if (g_str_has_suffix(path, ".glsl") || g_str_has_suffix(path, ".glslp") ||
                g_str_has_suffix(path, ".GLSL") || g_str_has_suffix(path, ".GLSLP")) {
                gtk_entry_set_text(GTK_ENTRY(widget), path);
            }
            g_free(path);
        }
        g_strfreev(uris);
    }
    gtk_drag_finish(context, TRUE, FALSE, time);
}

// Link sprite drag handler
static void on_link_graphics_drag_received(GtkWidget *widget, GdkDragContext *context,
                                           gint x, gint y, GtkSelectionData *data,
                                           guint info, guint time, gpointer user_data) {
    (void)x; (void)y; (void)info; (void)user_data;
    gchar **uris = gtk_selection_data_get_uris(data);
    if (uris && uris[0]) {
        gchar *path = g_filename_from_uri(uris[0], NULL, NULL);
        if (path) {
            // Filter for .zspr files
            if (g_str_has_suffix(path, ".zspr") || g_str_has_suffix(path, ".ZSPR")) {
                gtk_entry_set_text(GTK_ENTRY(widget), path);
            }
            g_free(path);
        }
        g_strfreev(uris);
    }
    gtk_drag_finish(context, TRUE, FALSE, time);
}

// Refresh the language ROM list UI
static void refresh_lang_roms_list(void) {
    if (!g_widgets.lang_roms_listbox) return;

    // Clear existing rows
    GList *children = gtk_container_get_children(GTK_CONTAINER(g_widgets.lang_roms_listbox));
    for (GList *iter = children; iter != NULL; iter = iter->next) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);

    // Create CSS provider for compact rows (applied once)
    static GtkCssProvider *row_css = NULL;
    if (!row_css) {
        row_css = gtk_css_provider_new();
        gtk_css_provider_load_from_data(row_css,
            "row { padding: 4px 6px; min-height: 0; }"
            "row button { padding: 0 4px; min-height: 0; min-width: 0; }", -1, NULL);
    }

    // Add rows for each language ROM
    for (int i = 0; i < g_lang_rom_count; i++) {
        GtkWidget *row = gtk_list_box_row_new();
        gtk_style_context_add_provider(gtk_widget_get_style_context(row),
            GTK_STYLE_PROVIDER(row_css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_widget_set_margin_top(hbox, 0);
        gtk_widget_set_margin_bottom(hbox, 0);

        // Status label with color coding
        GtkWidget *label = gtk_label_new(NULL);
        char markup[256];
        if (g_lang_roms[i].valid) {
            snprintf(markup, sizeof(markup),
                "<span foreground='#006400'>%s - %s</span>",
                g_lang_roms[i].lang_code, g_lang_roms[i].lang_name);
        } else {
            snprintf(markup, sizeof(markup),
                "<span foreground='red'>%s - %s (invalid SHA1)</span>",
                g_lang_roms[i].lang_code, g_lang_roms[i].lang_name);
        }
        gtk_label_set_markup(GTK_LABEL(label), markup);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

        // Compact clear button for this row
        GtkWidget *clear_btn = gtk_button_new_with_label("×");
        gtk_widget_set_tooltip_text(clear_btn, "Remove");
        gtk_style_context_add_provider(gtk_widget_get_style_context(clear_btn),
            GTK_STYLE_PROVIDER(row_css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_signal_connect(clear_btn, "clicked", G_CALLBACK(on_lang_rom_clear_clicked),
                         GINT_TO_POINTER(i));
        gtk_box_pack_end(GTK_BOX(hbox), clear_btn, FALSE, FALSE, 0);

        gtk_container_add(GTK_CONTAINER(row), hbox);
        gtk_container_add(GTK_CONTAINER(g_widgets.lang_roms_listbox), row);
    }

    // Show placeholder if empty
    if (g_lang_rom_count == 0) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new("Drag ROMs here or use Browse...");
        gtk_widget_set_margin_top(label, 10);
        gtk_widget_set_margin_bottom(label, 10);
        gtk_container_add(GTK_CONTAINER(row), label);
        gtk_container_add(GTK_CONTAINER(g_widgets.lang_roms_listbox), row);
    }

    gtk_widget_show_all(g_widgets.lang_roms_listbox);
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

// Cleanup temporary dialogue files after asset creation
static void cleanup_dialogue_files(const char *exe_dir) {
    for (int i = 0; i < g_lang_rom_count; i++) {
        if (!g_lang_roms[i].valid) continue;
        if (strcmp(g_lang_roms[i].lang_code, "us") == 0) continue;

        char path[512];
        snprintf(path, sizeof(path), "%s/dialogue_%s.txt",
                 exe_dir, g_lang_roms[i].lang_code);
        remove(path);
    }
}

// Helper function to get error message from restool error code
static const char *get_restool_error_message(int result) {
    switch (result) {
        case RESTOOL_OK:              return NULL;
        case RESTOOL_ERR_ROM_LOAD:    return "Error: Failed to read ROM file";
        case RESTOOL_ERR_ROM_INVALID: return "Error: Unrecognized ROM (SHA1 mismatch)";
        case RESTOOL_ERR_ROM_NOT_US:  return "Error: Base ROM must be USA version";
        case RESTOOL_ERR_EXTRACT:     return "Error: Asset extraction failed";
        case RESTOOL_ERR_WRITE:       return "Error: Failed to write output file";
        case RESTOOL_ERR_DIALOGUE:    return "Error: Dialogue processing failed";
        case RESTOOL_ERR_MEMORY:      return "Error: Out of memory";
        default:                      return "Error: Unknown error";
    }
}

// Signal handler for Create DAT button
static void on_make_dat_clicked(GtkButton *button, gpointer user_data) {
    (void)user_data;

    const char *rom_path = gtk_entry_get_text(GTK_ENTRY(g_widgets.rom_path_entry));
    LogInfo("MakeDat: Starting DAT creation");
    LogInfo("MakeDat: Base ROM path: %s", rom_path ? rom_path : "(null)");

    if (!rom_path || !*rom_path) {
        LogWarn("MakeDat: No ROM file selected");
        gtk_label_set_text(GTK_LABEL(g_widgets.make_dat_status), "Error: No ROM file selected");
        return;
    }

    gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);

    // Get directory where launcher lives (output directory for assets)
    char exe_dir[512];
    LauncherUI_GetExecutableDir(exe_dir, sizeof(exe_dir));
    LogInfo("MakeDat: Executable directory: %s", exe_dir);
    LogInfo("MakeDat: Language ROMs count: %d", g_lang_rom_count);
    for (int i = 0; i < g_lang_rom_count; i++) {
        LogInfo("MakeDat: Lang ROM %d: %s (%s) valid=%d path=%s",
                i, g_lang_roms[i].lang_code, g_lang_roms[i].lang_name,
                g_lang_roms[i].valid, g_lang_roms[i].path);
    }

    int result;
    bool extraction_failed = false;

    // Step 1: Extract dialogue from each valid language ROM using restool library
    for (int i = 0; i < g_lang_rom_count && !extraction_failed; i++) {
        if (!g_lang_roms[i].valid) continue;
        if (strcmp(g_lang_roms[i].lang_code, "us") == 0) continue;

        // Update status
        char status[256];
        snprintf(status, sizeof(status), "Extracting dialogue: %s...",
                 g_lang_roms[i].lang_name);
        gtk_label_set_text(GTK_LABEL(g_widgets.make_dat_status), status);
        while (gtk_events_pending()) gtk_main_iteration();

        LogInfo("MakeDat: Extracting dialogue from %s", g_lang_roms[i].path);
        result = Restool_ExtractDialogue(g_lang_roms[i].path, exe_dir);
        LogInfo("MakeDat: Dialogue extraction result: %d", result);

        if (result != RESTOOL_OK) {
            LogError("MakeDat: Dialogue extraction failed for %s (result=%d)",
                     g_lang_roms[i].lang_name, result);
            char error[256];
            snprintf(error, sizeof(error), "Error extracting %s dialogue",
                     g_lang_roms[i].lang_name);
            gtk_label_set_text(GTK_LABEL(g_widgets.make_dat_status), error);
            extraction_failed = true;
        }
    }

    if (extraction_failed) {
        cleanup_dialogue_files(exe_dir);
        gtk_widget_set_sensitive(GTK_WIDGET(button), TRUE);
        return;
    }

    // Step 2: Build language list for compilation
    char languages[256] = "";
    for (int i = 0; i < g_lang_rom_count; i++) {
        if (!g_lang_roms[i].valid) continue;
        if (strcmp(g_lang_roms[i].lang_code, "us") == 0) continue;

        if (languages[0] != '\0') strcat(languages, ",");
        strcat(languages, g_lang_roms[i].lang_code);
    }

    // Step 3: Compile assets using restool library
    LogInfo("MakeDat: Languages: '%s'", languages[0] ? languages : "(US only)");
    gtk_label_set_text(GTK_LABEL(g_widgets.make_dat_status), "Compiling assets...");
    while (gtk_events_pending()) gtk_main_iteration();

    char output_path[600];
    snprintf(output_path, sizeof(output_path), "%s/zelda3_assets.dat", exe_dir);

    RestoolCompileOptions options = {
        .us_rom_path = rom_path,
        .output_path = output_path,
        .languages = languages[0] ? languages : NULL,
        .dialogue_dir = exe_dir,
        .sprites_from_png = false
    };

    LogInfo("MakeDat: Compiling assets from %s to %s", rom_path, output_path);
    result = Restool_CompileAssetsEx(&options);
    LogInfo("MakeDat: Compile result: %d", result);

    // Step 4: Cleanup temporary dialogue files
    cleanup_dialogue_files(exe_dir);

    if (result == RESTOOL_OK) {
        // Verify created DAT file
        FILE *dat_check = fopen(output_path, "rb");
        if (dat_check) {
            fseek(dat_check, 0, SEEK_END);
            long dat_size = ftell(dat_check);
            fseek(dat_check, 80, SEEK_SET);
            uint32_t num_assets = 0;
            fread(&num_assets, 4, 1, dat_check);
            fclose(dat_check);
            LogInfo("MakeDat: Created DAT file: %s (size=%ld, assets=%u)",
                    output_path, dat_size, num_assets);
        } else {
            LogWarn("MakeDat: Could not verify created DAT file at %s", output_path);
        }

        if (languages[0] != '\0') {
            gtk_label_set_text(GTK_LABEL(g_widgets.make_dat_status),
                "Success! Created multi-language zelda3_assets.dat");
        } else {
            gtk_label_set_text(GTK_LABEL(g_widgets.make_dat_status),
                "Success! Created zelda3_assets.dat");
        }
        // Refresh DAT status and language dropdown
        update_dat_status();
        refresh_language_dropdown(NULL);
        LogInfo("MakeDat: DAT creation successful, refreshing UI");
    } else {
        LogError("MakeDat: DAT creation failed (result=%d)", result);
        const char *error_msg = get_restool_error_message(result);
        gtk_label_set_text(GTK_LABEL(g_widgets.make_dat_status), error_msg);
    }

    gtk_widget_set_sensitive(GTK_WIDGET(button), TRUE);
}

// Populate language dropdown with languages available in DAT file
static void refresh_language_dropdown(const char *current_lang) {
    LogInfo("RefreshLang: Refreshing language dropdown (current=%s)",
            current_lang ? current_lang : "(null)");

    // Get available languages from DAT file
    char exe_dir[512];
    LauncherUI_GetExecutableDir(exe_dir, sizeof(exe_dir));
    LogInfo("RefreshLang: Reading languages from %s", exe_dir);

    char available_langs[16][16];
    int num_langs = Restool_GetDatLanguages(exe_dir, available_langs, 16);
    LogInfo("RefreshLang: DatReader returned %d languages", num_langs);

    for (int i = 0; i < num_langs; i++) {
        LogInfo("RefreshLang: Language %d: '%s'", i, available_langs[i]);
    }

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
    gtk_widget_set_hexpand(rom_hbox, TRUE);
    g_widgets.rom_path_entry = gtk_entry_new();
    // Enable drag-drop for ROM file
    static GtkTargetEntry rom_target_entries[] = {
        { "text/uri-list", 0, 0 }
    };
    gtk_drag_dest_set(g_widgets.rom_path_entry,
                      GTK_DEST_DEFAULT_ALL,
                      rom_target_entries, 1,
                      GDK_ACTION_COPY);
    g_signal_connect(g_widgets.rom_path_entry, "drag-data-received",
                     G_CALLBACK(on_rom_path_drag_received), NULL);
    gtk_box_pack_start(GTK_BOX(rom_hbox), g_widgets.rom_path_entry, TRUE, TRUE, 0);

    GtkWidget *rom_browse_btn = gtk_button_new_with_label("Browse...");
    gtk_widget_set_size_request(rom_browse_btn, 75, -1);  // Fixed width for alignment
    g_signal_connect(rom_browse_btn, "clicked", G_CALLBACK(on_rom_browse_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(rom_hbox), rom_browse_btn, FALSE, FALSE, 0);

    gtk_grid_attach(GTK_GRID(grid), rom_hbox, 1, row++, 1, 1);

    // === Language ROMs Section (optional, for multi-language) ===
    GtkWidget *lang_roms_label = gtk_label_new("Language ROMs:");
    gtk_widget_set_halign(lang_roms_label, GTK_ALIGN_START);
    gtk_widget_set_valign(lang_roms_label, GTK_ALIGN_START);  // Top-align label
    gtk_grid_attach(GTK_GRID(grid), lang_roms_label, 0, row, 1, 1);

    // Horizontal box: scrolled listbox on left, buttons on right
    GtkWidget *lang_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_hexpand(lang_hbox, TRUE);

    // Create scrolled window with listbox for language ROMs
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled, -1, 150);  // Taller box
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);

    g_widgets.lang_roms_listbox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(g_widgets.lang_roms_listbox),
                                    GTK_SELECTION_NONE);
    gtk_container_add(GTK_CONTAINER(scrolled), g_widgets.lang_roms_listbox);

    // Enable drag-drop for file URIs
    static GtkTargetEntry target_entries[] = {
        { "text/uri-list", 0, 0 }
    };
    gtk_drag_dest_set(g_widgets.lang_roms_listbox,
                      GTK_DEST_DEFAULT_ALL,
                      target_entries, 1,
                      GDK_ACTION_COPY);
    g_signal_connect(g_widgets.lang_roms_listbox, "drag-data-received",
                     G_CALLBACK(on_lang_roms_drag_received), NULL);

    gtk_box_pack_start(GTK_BOX(lang_hbox), scrolled, TRUE, TRUE, 0);

    // Vertical button box on the right side (fixed width for alignment)
    GtkWidget *lang_btn_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_valign(lang_btn_vbox, GTK_ALIGN_START);  // Top-align buttons
    gtk_widget_set_size_request(lang_btn_vbox, 75, -1);  // Fixed width for alignment

    g_widgets.lang_roms_browse_btn = gtk_button_new_with_label("Browse...");
    g_signal_connect(g_widgets.lang_roms_browse_btn, "clicked",
                     G_CALLBACK(on_lang_roms_browse_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(lang_btn_vbox), g_widgets.lang_roms_browse_btn, FALSE, FALSE, 0);

    g_widgets.lang_roms_clear_all_btn = gtk_button_new_with_label("Clear");
    g_signal_connect(g_widgets.lang_roms_clear_all_btn, "clicked",
                     G_CALLBACK(on_lang_roms_clear_all_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(lang_btn_vbox), g_widgets.lang_roms_clear_all_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(lang_hbox), lang_btn_vbox, FALSE, FALSE, 0);

    gtk_grid_attach(GTK_GRID(grid), lang_hbox, 1, row++, 1, 1);

    // Initialize the language ROM list display
    refresh_lang_roms_list();

    // Create DAT button - left-aligned, natural width
    g_widgets.make_dat_btn = gtk_button_new_with_label("Create Asset File");
    g_signal_connect(g_widgets.make_dat_btn, "clicked", G_CALLBACK(on_make_dat_clicked), NULL);
    gtk_widget_set_halign(g_widgets.make_dat_btn, GTK_ALIGN_START);

    gtk_grid_attach(GTK_GRID(grid), g_widgets.make_dat_btn, 1, row++, 1, 1);

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
    const char *aspect_ratios[] = {"4:3", "16:9", "16:10", "18:9", "Custom"};
    g_widgets.aspect_ratio = create_combo_box_with_label(grid, row++,
        "Aspect Ratio:", aspect_ratios, 5);
    gtk_combo_box_set_active(GTK_COMBO_BOX(g_widgets.aspect_ratio), config->extended_aspect_ratio);

    // Custom aspect ratio input fields (shown only when "Custom" is selected)
    g_widgets.custom_aspect_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    g_widgets.custom_aspect_w = gtk_entry_new();
    g_widgets.custom_aspect_h = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(g_widgets.custom_aspect_w), 5);
    gtk_entry_set_width_chars(GTK_ENTRY(g_widgets.custom_aspect_h), 5);
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_widgets.custom_aspect_w), "W");
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_widgets.custom_aspect_h), "H");

    // Set initial values if custom ratio was configured
    if (config->custom_aspect_w > 0 && config->custom_aspect_h > 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", config->custom_aspect_w);
        gtk_entry_set_text(GTK_ENTRY(g_widgets.custom_aspect_w), buf);
        snprintf(buf, sizeof(buf), "%d", config->custom_aspect_h);
        gtk_entry_set_text(GTK_ENTRY(g_widgets.custom_aspect_h), buf);
    }

    gtk_box_pack_start(GTK_BOX(g_widgets.custom_aspect_box), g_widgets.custom_aspect_w, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(g_widgets.custom_aspect_box), gtk_label_new(":"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(g_widgets.custom_aspect_box), g_widgets.custom_aspect_h, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), g_widgets.custom_aspect_box, 1, row++, 1, 1);

    // Connect signal handler and set initial visibility
    g_signal_connect(g_widgets.aspect_ratio, "changed",
                     G_CALLBACK(on_aspect_ratio_changed), NULL);

    // Set no_show_all so parent's show_all doesn't override our visibility
    gtk_widget_set_no_show_all(g_widgets.custom_aspect_box, TRUE);
    gtk_widget_set_no_show_all(g_widgets.custom_aspect_w, TRUE);
    gtk_widget_set_no_show_all(g_widgets.custom_aspect_h, TRUE);
    if (config->extended_aspect_ratio == 4) {
        // Show all children explicitly
        gtk_widget_show(g_widgets.custom_aspect_box);
        gtk_widget_show(g_widgets.custom_aspect_w);
        gtk_widget_show(g_widgets.custom_aspect_h);
        gtk_container_foreach(GTK_CONTAINER(g_widgets.custom_aspect_box),
                              (GtkCallback)gtk_widget_show, NULL);
    }

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

    g_widgets.display_perf_title = create_checkbox(grid, row++, "Show FPS in window title");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.display_perf_title), config->display_perf_title);

    g_widgets.disable_frame_delay = create_checkbox(grid, row++, "Disable frame delay (for 60Hz displays only)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.disable_frame_delay), config->disable_frame_delay);

    g_widgets.dim_flashes = create_checkbox(grid, row++, "Dim flashes (accessibility - lessens flashing effects)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.dim_flashes), config->features0 & kFeatures0_DimFlashes);

    // Shader file path
    GtkWidget *shader_label = gtk_label_new("Shader File:");
    gtk_widget_set_halign(shader_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), shader_label, 0, row, 1, 1);

    GtkWidget *shader_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    g_widgets.shader_path_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(g_widgets.shader_path_entry),
                       config->shader ? config->shader : "");
    gtk_drag_dest_set(g_widgets.shader_path_entry,
                      GTK_DEST_DEFAULT_ALL,
                      NULL, 0, GDK_ACTION_COPY);
    gtk_drag_dest_add_uri_targets(g_widgets.shader_path_entry);
    g_signal_connect(g_widgets.shader_path_entry, "drag-data-received",
                     G_CALLBACK(on_shader_drag_received), NULL);
    gtk_box_pack_start(GTK_BOX(shader_hbox), g_widgets.shader_path_entry, TRUE, TRUE, 0);

    GtkWidget *shader_browse_btn = gtk_button_new_with_label("Browse...");
    g_signal_connect(shader_browse_btn, "clicked",
                     G_CALLBACK(on_shader_path_browse_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(shader_hbox), shader_browse_btn, FALSE, FALSE, 0);

    gtk_grid_attach(GTK_GRID(grid), shader_hbox, 1, row, 1, 1);
    row++;

    // Link sprite file path
    GtkWidget *link_gfx_label = gtk_label_new("Link Sprite File:");
    gtk_widget_set_halign(link_gfx_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), link_gfx_label, 0, row, 1, 1);

    GtkWidget *link_gfx_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    g_widgets.link_graphics_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(g_widgets.link_graphics_entry),
                       config->link_graphics ? config->link_graphics : "");
    gtk_drag_dest_set(g_widgets.link_graphics_entry,
                      GTK_DEST_DEFAULT_ALL,
                      NULL, 0, GDK_ACTION_COPY);
    gtk_drag_dest_add_uri_targets(g_widgets.link_graphics_entry);
    g_signal_connect(g_widgets.link_graphics_entry, "drag-data-received",
                     G_CALLBACK(on_link_graphics_drag_received), NULL);
    gtk_box_pack_start(GTK_BOX(link_gfx_hbox), g_widgets.link_graphics_entry, TRUE, TRUE, 0);

    GtkWidget *link_gfx_browse_btn = gtk_button_new_with_label("Browse...");
    g_signal_connect(link_gfx_browse_btn, "clicked",
                     G_CALLBACK(on_link_graphics_browse_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(link_gfx_hbox), link_gfx_browse_btn, FALSE, FALSE, 0);

    gtk_grid_attach(GTK_GRID(grid), link_gfx_hbox, 1, row, 1, 1);
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

    // MSU info label (shows detection results)
    g_widgets.msu_info_label = gtk_label_new("");
    gtk_widget_set_halign(g_widgets.msu_info_label, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(g_widgets.msu_info_label), 0.0);
    gtk_grid_attach(GTK_GRID(grid), g_widgets.msu_info_label, 1, row, 1, 1);
    row++;

#ifdef HAVE_OPUS_ENCODER
    // Encode PCM to Opus button
    g_widgets.encode_opus_btn = gtk_button_new_with_label("Encode PCM files to Opus...");
    g_signal_connect(g_widgets.encode_opus_btn, "clicked",
                     G_CALLBACK(on_encode_opus_clicked), NULL);
    gtk_widget_set_halign(g_widgets.encode_opus_btn, GTK_ALIGN_START);
    gtk_widget_set_sensitive(g_widgets.encode_opus_btn, FALSE);  // Disabled until PCM detected
    gtk_grid_attach(GTK_GRID(grid), g_widgets.encode_opus_btn, 1, row, 1, 1);
    row++;
#endif

    return grid;
}

// Helper: Add a section header to the grid
static void add_section_header(GtkWidget *grid, int *row, const char *title, bool first) {
    GtkWidget *label = gtk_label_new(NULL);
    char markup[128];
    snprintf(markup, sizeof(markup), "<b>%s</b>", title);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    if (!first)
        gtk_widget_set_margin_top(label, 12);
    gtk_grid_attach(GTK_GRID(grid), label, 0, (*row)++, 2, 1);
}

// Create Features tab
static GtkWidget* create_features_tab(const Config *config) {
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);

    int row = 0;
    uint32 features = config->features0;

    // === SAVE Section ===
    add_section_header(grid, &row, "Save", true);

    g_widgets.feat_autosave = create_checkbox(grid, row++, "Autosave on quit and reload on start");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_autosave), config->autosave);

    // === CONTROLS Section ===
    add_section_header(grid, &row, "Controls", false);

    g_widgets.feat_switch_lr = create_checkbox(grid, row++, "Item switching with L/R shoulder buttons");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_switch_lr),
        features & kFeatures0_SwitchLR);

    g_widgets.feat_switch_lr_limit = create_checkbox(grid, row++, "Limit L/R item switching to first 4 items only");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_switch_lr_limit),
        features & kFeatures0_SwitchLRLimit);

    g_widgets.feat_turn_dash = create_checkbox(grid, row++, "Allow Link to turn while dashing");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_turn_dash),
        features & kFeatures0_TurnWhileDashing);

    // === GAMEPLAY Section ===
    add_section_header(grid, &row, "Gameplay", false);

    g_widgets.feat_skip_intro = create_checkbox(grid, row++, "Skip intro on any keypress");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_skip_intro),
        features & kFeatures0_SkipIntroOnKeypress);

    g_widgets.feat_mirror_dw = create_checkbox(grid, row++, "Allow magic mirror to warp to the Dark World");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_mirror_dw),
        features & kFeatures0_MirrorToDarkworld);

    g_widgets.feat_sword_collect = create_checkbox(grid, row++, "Collect items (hearts, rupees) with sword");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_sword_collect),
        features & kFeatures0_CollectItemsWithSword);

    g_widgets.feat_more_bombs = create_checkbox(grid, row++, "Allow more active bombs (4 instead of 2)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_more_bombs),
        features & kFeatures0_MoreActiveBombs);

    g_widgets.feat_more_rupees = create_checkbox(grid, row++, "Increase rupee capacity to 9999");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_more_rupees),
        features & kFeatures0_CarryMoreRupees);

    g_widgets.feat_cancel_bird = create_checkbox(grid, row++, "Cancel bird travel by pressing X");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_cancel_bird),
        features & kFeatures0_CancelBirdTravel);

    const char *sword_levels[] = {"Disabled", "Level 1 (Wooden)", "Level 2 (Master)", "Level 3 (Tempered)", "Level 4 (Golden)"};
    g_widgets.feat_sword_pots = create_combo_box_with_label(grid, row++, "Break pots with sword:", sword_levels, 5);
    gtk_combo_box_set_active(GTK_COMBO_BOX(g_widgets.feat_sword_pots), config->break_pots_min_sword);

    // === INTERFACE Section ===
    add_section_header(grid, &row, "Interface", false);

    g_widgets.feat_yellow_items = create_checkbox(grid, row++, "Highlight maxed items in yellow");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_yellow_items),
        features & kFeatures0_ShowMaxItemsInYellow);

    g_widgets.feat_no_beep = create_checkbox(grid, row++, "Disable low health beep sound");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_no_beep),
        features & kFeatures0_DisableLowHealthBeep);

    // === BUG FIXES Section ===
    add_section_header(grid, &row, "Bug Fixes", false);

    g_widgets.feat_misc_bugs = create_checkbox(grid, row++, "Fix misc bugs from original game");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_misc_bugs),
        features & kFeatures0_MiscBugFixes);

    g_widgets.feat_game_bugs = create_checkbox(grid, row++, "Fix bugs that change gameplay");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_game_bugs),
        features & kFeatures0_GameChangingBugFixes);

    // === EXPERIMENTAL Section ===
    add_section_header(grid, &row, "Experimental", false);

    g_widgets.feat_pokemode = create_checkbox(grid, row++, "Pokemode");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.feat_pokemode),
        features & kFeatures0_Pokemode);

    g_widgets.feat_zelda_helps = create_checkbox(grid, row++, "Princess Zelda helps in battle");
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

// Helper to convert button label to storable value (NULL if "(not set)")
static char* label_to_value(const char *label) {
    if (!label || strcmp(label, "(not set)") == 0) return NULL;
    return strdup(label);
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

    // Read custom aspect ratio values if "Custom" is selected
    if (config->extended_aspect_ratio == 4) {
        const char *w_text = gtk_entry_get_text(GTK_ENTRY(g_widgets.custom_aspect_w));
        const char *h_text = gtk_entry_get_text(GTK_ENTRY(g_widgets.custom_aspect_h));
        config->custom_aspect_w = (w_text && *w_text) ? atoi(w_text) : 0;
        config->custom_aspect_h = (h_text && *h_text) ? atoi(h_text) : 0;
    } else {
        config->custom_aspect_w = 0;
        config->custom_aspect_h = 0;
    }

    config->ignore_aspect_ratio = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.ignore_aspect_ratio));
    config->extend_y = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.extend_y));
    config->linear_filtering = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.linear_filtering));
    config->new_renderer = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.new_renderer));
    config->enhanced_mode7 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.enhanced_mode7));
    config->no_sprite_limits = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.no_sprite_limits));
    config->display_perf_title = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.display_perf_title));
    config->disable_frame_delay = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.disable_frame_delay));

    // Shader path
    const char *shader_text = gtk_entry_get_text(GTK_ENTRY(g_widgets.shader_path_entry));
    if (config->shader) free((void*)config->shader);
    config->shader = (shader_text && *shader_text) ? strdup(shader_text) : NULL;

    // Link graphics path
    const char *link_gfx_text = gtk_entry_get_text(GTK_ENTRY(g_widgets.link_graphics_entry));
    if (config->link_graphics) free((void*)config->link_graphics);
    config->link_graphics = (link_gfx_text && *link_gfx_text) ? strdup(link_gfx_text) : NULL;

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
    config->autosave = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.feat_autosave));
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
    config->break_pots_min_sword = gtk_combo_box_get_active(GTK_COMBO_BOX(g_widgets.feat_sword_pots));
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
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_widgets.dim_flashes)))
        config->features0 |= kFeatures0_DimFlashes;

    // Read keyboard save state bindings (30 buttons)
    char **state_arrays[] = {g_kbd_load, g_kbd_save, g_kbd_replay};
    for (int type = 0; type < 3; type++) {
        for (int i = 0; i < NUM_SAVE_SLOTS; i++) {
            char key[32];
            snprintf(key, sizeof(key), "state_%d_%d", type, i);
            GtkWidget *button = g_object_get_data(G_OBJECT(g_widgets.kbd_states_grid), key);
            if (button) {
                if (state_arrays[type][i]) free(state_arrays[type][i]);
                state_arrays[type][i] = label_to_value(gtk_button_get_label(GTK_BUTTON(button)));
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
            *cheat_ptrs[i] = label_to_value(gtk_button_get_label(GTK_BUTTON(button)));
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
            *system_ptrs[i] = label_to_value(gtk_button_get_label(GTK_BUTTON(button)));
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
                gamepad_state_arrays[type][i] = label_to_value(gtk_button_get_label(GTK_BUTTON(button)));
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
            *gamepad_cheat_ptrs[i] = label_to_value(gtk_button_get_label(GTK_BUTTON(button)));
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
            *gamepad_system_ptrs[i] = label_to_value(gtk_button_get_label(GTK_BUTTON(button)));
        }
    }
}
