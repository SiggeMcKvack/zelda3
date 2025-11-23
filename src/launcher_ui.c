#include "launcher_ui.h"
#include "config.h"
#include "features.h"
#include "launcher_gamepad.h"
#include "logging.h"
#include <gtk/gtk.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

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

// Quick save/load (gamepad only)
char *g_gamepad_save = NULL;
char *g_gamepad_load = NULL;

// Control names for UI
static const char *kControlNames[12] = {
    "Up", "Down", "Left", "Right", "Select", "Start", "A", "B", "X", "Y", "L", "R"
};

// Widget references for updating config
static struct {
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

    // Gamepad quick save/load
    GtkWidget *gamepad_save_entry;
    GtkWidget *gamepad_load_entry;

    // Keyboard subtab grids (for accessing entry widgets)
    GtkWidget *kbd_states_grid;
    GtkWidget *kbd_cheats_grid;
    GtkWidget *kbd_system_grid;
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

// Helper: Create labeled spin button
static GtkWidget* create_spin_button_with_label(GtkWidget *grid, int row,
                                                  const char *label_text,
                                                  double min, double max, double step) {
    GtkWidget *label = gtk_label_new(label_text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);

    GtkWidget *spin = gtk_spin_button_new_with_range(min, max, step);
    gtk_grid_attach(GTK_GRID(grid), spin, 1, row, 1, 1);

    return spin;
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
        const char *defaults[12] = {
            "Up", "Down", "Left", "Right", "Right Shift", "Return",
            "X", "Z", "S", "A", "C", "V"
        };
        for (int i = 0; i < 12; i++) {
            controls[i] = strdup(defaults[i]);
        }
        return;
    }

    // Parse comma-separated values (preserves empty values between commas)
    const char *p = str;
    int i = 0;

    while (*p && i < 12) {
        // Find next comma or end of string
        const char *comma = strchr(p, ',');
        const char *end = comma ? comma : p + strlen(p);

        // Extract substring
        size_t len = end - p;
        char *token = malloc(len + 1);
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
    while (i < 12) {
        controls[i++] = strdup("");
    }
}

// Parse comma-separated control string into array (gamepad controls)
void LauncherUI_ParseGamepadControlString(const char *str, char **controls) {
    if (!str || !*str) {
        // Set gamepad defaults if no string
        // Mapping: Up, Down, Left, Right, Select, Start, A, B, X, Y, L, R
        // Xbox-style controller layout (positional mapping for SNES buttons)
        const char *defaults[12] = {
            "DpadUp", "DpadDown", "DpadLeft", "DpadRight", "Back", "Start",
            "B", "A", "Y", "X", "L1", "R1"
        };
        for (int i = 0; i < 12; i++) {
            controls[i] = strdup(defaults[i]);
        }
        return;
    }

    // Parse comma-separated values
    char *copy = strdup(str);
    char *token = strtok(copy, ",");
    int i = 0;
    while (token && i < 12) {
        // Trim whitespace
        while (isspace(*token)) token++;
        char *end = token + strlen(token) - 1;
        while (end > token && isspace(*end)) end--;
        end[1] = '\0';

        controls[i++] = strdup(token);
        token = strtok(NULL, ",");
    }

    // Fill remaining with empty strings
    while (i < 12) {
        controls[i++] = strdup("");
    }

    free(copy);
}

// Format control array into comma-separated string
char* LauncherUI_FormatControlString(char **controls) {
    size_t total_len = 0;
    for (int i = 0; i < 10; i++) {
        if (controls[i]) {
            total_len += strlen(controls[i]);
        }
        if (i > 0) total_len += 2;  // +2 for ", "
    }

    char *result = malloc(total_len + 1);
    result[0] = '\0';

    for (int i = 0; i < 10; i++) {
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

    g_widgets.extend_y = create_checkbox(grid, row++, "Extend Y (240 lines)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.extend_y), config->extend_y);

    g_widgets.linear_filtering = create_checkbox(grid, row++, "Use linear filtering for smoother pixels");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.linear_filtering), config->linear_filtering);

    g_widgets.new_renderer = create_checkbox(grid, row++, "Use optimized SNES PPU renderer");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.new_renderer), config->new_renderer);

    g_widgets.enhanced_mode7 = create_checkbox(grid, row++, "Display the world map with higher resolution (Enhanced Mode 7)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.enhanced_mode7), config->enhanced_mode7);

    g_widgets.no_sprite_limits = create_checkbox(grid, row++, "Disable SNES sprite limit (8 sprites per scanline)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_widgets.no_sprite_limits), config->no_sprite_limits);

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
    gtk_label_set_markup(GTK_LABEL(controls_title), "<b>SNES Controller Buttons</b>\nClick button to remap");
    gtk_grid_attach(GTK_GRID(controls_grid), controls_title, 0, 0, 2, 1);

    for (int i = 0; i < 12; i++) {
        // Create label for control name
        char label_text[64];
        snprintf(label_text, sizeof(label_text), "%s:", kControlNames[i]);
        GtkWidget *label = gtk_label_new(label_text);
        gtk_widget_set_halign(label, GTK_ALIGN_END);
        gtk_grid_attach(GTK_GRID(controls_grid), label, 0, i + 1, 1, 1);

        // Create button showing only the key
        const char *key = g_kbd_controls[i] ? g_kbd_controls[i] : "(not set)";
        GtkWidget *button = gtk_button_new_with_label(key);
        gtk_widget_set_size_request(button, 150, 35);
        g_signal_connect(button, "clicked", G_CALLBACK(on_keyboard_button_clicked), GINT_TO_POINTER(i));
        gtk_grid_attach(GTK_GRID(controls_grid), button, 1, i + 1, 1, 1);
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
    gtk_label_set_markup(GTK_LABEL(states_title), "<b>Save State Keys (F1-F10)</b>");
    gtk_grid_attach(GTK_GRID(states_grid), states_title, 0, 0, 2, 1);

    int row = 1;
    const char *state_labels[] = {"Load", "Save", "Replay"};
    char **state_arrays[] = {g_kbd_load, g_kbd_save, g_kbd_replay};
    const char *defaults[][10] = {
        {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10"},
        {"Shift+F1", "Shift+F2", "Shift+F3", "Shift+F4", "Shift+F5", "Shift+F6", "Shift+F7", "Shift+F8", "Shift+F9", "Shift+F10"},
        {"Ctrl+F1", "Ctrl+F2", "Ctrl+F3", "Ctrl+F4", "Ctrl+F5", "Ctrl+F6", "Ctrl+F7", "Ctrl+F8", "Ctrl+F9", "Ctrl+F10"}
    };

    for (int type = 0; type < 3; type++) {
        GtkWidget *type_label = gtk_label_new(NULL);
        char markup[64];
        snprintf(markup, sizeof(markup), "<b>%s:</b>", state_labels[type]);
        gtk_label_set_markup(GTK_LABEL(type_label), markup);
        gtk_widget_set_halign(type_label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(states_grid), type_label, 0, row++, 3, 1);

        for (int i = 0; i < 10; i++) {
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
            gtk_widget_set_size_request(button, 150, 35);
            g_signal_connect(button, "clicked", G_CALLBACK(on_key_button_clicked), slot_label);
            gtk_grid_attach(GTK_GRID(states_grid), button, 1, row, 1, 1);
            g_object_set_data(G_OBJECT(states_grid), g_strdup_printf("state_%d_%d", type, i), button);

            // Attach variable pointer to button for key capture
            g_object_set_data(G_OBJECT(button), "variable_ptr", &state_arrays[type][i]);

            // Add Clear button
            GtkWidget *clear_button = gtk_button_new_with_label("Clear");
            gtk_widget_set_size_request(clear_button, 80, 35);
            ClearButtonData *clear_data = malloc(sizeof(ClearButtonData));
            clear_data->button = button;
            clear_data->variable = &state_arrays[type][i];
            g_signal_connect(clear_button, "clicked", G_CALLBACK(on_clear_button_clicked), clear_data);
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
    gtk_label_set_markup(GTK_LABEL(cheats_title), "<b>Cheat Keys</b>");
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
        gtk_widget_set_size_request(button, 150, 35);
        g_signal_connect(button, "clicked", G_CALLBACK(on_key_button_clicked), (gpointer)cheats[i].label);
        gtk_grid_attach(GTK_GRID(cheats_grid), button, 1, row, 1, 1);
        g_object_set_data(G_OBJECT(cheats_grid), g_strdup_printf("cheat_%d", i), button);

        // Attach variable pointer to button for key capture
        g_object_set_data(G_OBJECT(button), "variable_ptr", cheats[i].ptr);

        // Add Clear button
        GtkWidget *clear_button = gtk_button_new_with_label("Clear");
        gtk_widget_set_size_request(clear_button, 80, 35);
        ClearButtonData *clear_data = malloc(sizeof(ClearButtonData));
        clear_data->button = button;
        clear_data->variable = cheats[i].ptr;
        g_signal_connect(clear_button, "clicked", G_CALLBACK(on_clear_button_clicked), clear_data);
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
    gtk_label_set_markup(GTK_LABEL(system_title), "<b>System Control Keys</b>");
    gtk_grid_attach(GTK_GRID(system_grid), system_title, 0, 0, 3, 1);

    row = 1;
    struct { const char *label; char **ptr; const char *def; } system_keys[] = {
        {"Clear Key Log:", &g_kbd_clear_keylog, "K"},
        {"Stop Replay:", &g_kbd_stop_replay, "L"},
        {"Toggle Fullscreen:", &g_kbd_fullscreen, "Alt+Return"},
        {"Reset:", &g_kbd_reset, "Ctrl+R"},
        {"Pause (Dimmed):", &g_kbd_pause_dimmed, "P"},
        {"Pause:", &g_kbd_pause, "Shift+P"},
        {"Turbo:", &g_kbd_turbo, "Tab"},
        {"Replay Turbo:", &g_kbd_replay_turbo, "T"},
        {"Window Bigger:", &g_kbd_window_bigger, "Ctrl+Up"},
        {"Window Smaller:", &g_kbd_window_smaller, "Ctrl+Down"},
        {"Volume Up:", &g_kbd_volume_up, "Shift+="},
        {"Volume Down:", &g_kbd_volume_down, "Shift+-"}
    };

    for (int i = 0; i < 12; i++) {
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
        gtk_widget_set_size_request(button, 150, 35);
        g_signal_connect(button, "clicked", G_CALLBACK(on_key_button_clicked), (gpointer)system_keys[i].label);
        gtk_grid_attach(GTK_GRID(system_grid), button, 1, row, 1, 1);
        g_object_set_data(G_OBJECT(system_grid), g_strdup_printf("system_%d", i), button);

        // Attach variable pointer to button for key capture
        g_object_set_data(G_OBJECT(button), "variable_ptr", system_keys[i].ptr);

        // Add Clear button
        GtkWidget *clear_button = gtk_button_new_with_label("Clear");
        gtk_widget_set_size_request(clear_button, 80, 35);
        ClearButtonData *clear_data = malloc(sizeof(ClearButtonData));
        clear_data->button = button;
        clear_data->variable = system_keys[i].ptr;
        g_signal_connect(clear_button, "clicked", G_CALLBACK(on_clear_button_clicked), clear_data);
        gtk_grid_attach(GTK_GRID(system_grid), clear_button, 2, row, 1, 1);

        row++;
    }

    gtk_container_add(GTK_CONTAINER(system_scroll), system_grid);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), system_scroll, gtk_label_new("System"));

    return notebook;
}

// Gamepad button click handler
static void on_gamepad_button_clicked(GtkWidget *button, gpointer user_data) {
    int index = GPOINTER_TO_INT(user_data);

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
             kControlNames[index]);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_container_add(GTK_CONTAINER(content), label);

    gtk_widget_show_all(dialog);

    // Detect input with timeout (non-blocking)
    DetectedInput input = LauncherGamepad_DetectInput(gamepads[0].controller, 5000);

    if (input.type == INPUT_TYPE_BUTTON) {
        const char *btn_name = LauncherGamepad_ButtonToString(input.button);
        if (btn_name) {
            if (g_gamepad_controls[index]) free(g_gamepad_controls[index]);
            g_gamepad_controls[index] = strdup(btn_name);

            // Update button label
            char btn_label[128];
            snprintf(btn_label, sizeof(btn_label), "%s: %s", kControlNames[index], btn_name);
            gtk_button_set_label(GTK_BUTTON(button), btn_label);
        }
    } else if (input.type == INPUT_TYPE_AXIS) {
        const char *axis_name = LauncherGamepad_AxisToString(input.axis, input.axis_value);
        if (axis_name) {
            if (g_gamepad_controls[index]) free(g_gamepad_controls[index]);
            g_gamepad_controls[index] = strdup(axis_name);

            // Update button label
            char btn_label[128];
            snprintf(btn_label, sizeof(btn_label), "%s: %s", kControlNames[index], axis_name);
            gtk_button_set_label(GTK_BUTTON(button), btn_label);
        }
    }

    LauncherGamepad_Close(gamepads[0].controller);
    gtk_widget_destroy(dialog);
}

// Create GamepadMap tab with interactive buttons
static GtkWidget* create_gamepadmap_tab(const Config *config) {
    (void)config;

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);

    // Title
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<b>Gamepad Controls</b>\nClick a button to remap");
    gtk_label_set_justify(GTK_LABEL(title), GTK_JUSTIFY_CENTER);
    gtk_grid_attach(GTK_GRID(grid), title, 0, 0, 2, 1);

    // Create button for each control
    for (int i = 0; i < 12; i++) {
        char label[128];
        const char *btn = g_gamepad_controls[i] ? g_gamepad_controls[i] : "(not set)";
        snprintf(label, sizeof(label), "%s: %s", kControlNames[i], btn);

        GtkWidget *button = gtk_button_new_with_label(label);
        gtk_widget_set_size_request(button, 200, 40);
        g_signal_connect(button, "clicked", G_CALLBACK(on_gamepad_button_clicked), GINT_TO_POINTER(i));

        gtk_grid_attach(GTK_GRID(grid), button, i / 6, i % 6 + 1, 1, 1);
    }

    // Quick save/load section
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_grid_attach(GTK_GRID(grid), separator, 0, 7, 2, 1);

    GtkWidget *quick_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(quick_label), "<b>Quick Save/Load Bindings</b>");
    gtk_label_set_justify(GTK_LABEL(quick_label), GTK_JUSTIFY_CENTER);
    gtk_grid_attach(GTK_GRID(grid), quick_label, 0, 8, 2, 1);

    // Quick Save entry
    GtkWidget *save_label = gtk_label_new("Quick Save:");
    gtk_widget_set_halign(save_label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), save_label, 0, 9, 1, 1);

    g_widgets.gamepad_save_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(g_widgets.gamepad_save_entry), g_gamepad_save ? g_gamepad_save : "L2+R3");
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_widgets.gamepad_save_entry), "e.g., L2+R3");
    gtk_grid_attach(GTK_GRID(grid), g_widgets.gamepad_save_entry, 1, 9, 1, 1);

    // Quick Load entry
    GtkWidget *load_label = gtk_label_new("Quick Load:");
    gtk_widget_set_halign(load_label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), load_label, 0, 10, 1, 1);

    g_widgets.gamepad_load_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(g_widgets.gamepad_load_entry), g_gamepad_load ? g_gamepad_load : "L2+L3");
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_widgets.gamepad_load_entry), "e.g., L2+L3");
    gtk_grid_attach(GTK_GRID(grid), g_widgets.gamepad_load_entry, 1, 10, 1, 1);

    gtk_container_add(GTK_CONTAINER(scroll), grid);
    return scroll;
}

// Create main window with tabs
GtkWidget* LauncherUI_CreateWindow(Config *config) {
    // Create main window
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Zelda3 Launcher");
    gtk_window_set_default_size(GTK_WINDOW(window), 700, 550);
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
    GtkWidget *graphics_tab = create_graphics_tab(config);
    GtkWidget *sound_tab = create_sound_tab(config);
    GtkWidget *features_tab = create_features_tab(config);
    GtkWidget *keymap_tab = create_keymap_tab(config);
    GtkWidget *gamepad_tab = create_gamepadmap_tab(config);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), graphics_tab, gtk_label_new("Graphics"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), sound_tab, gtk_label_new("Sound"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), features_tab, gtk_label_new("Features"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), keymap_tab, gtk_label_new("Keyboard"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), gamepad_tab, gtk_label_new("Gamepad"));

    return window;
}

// Update config from UI widgets
void LauncherUI_UpdateConfigFromUI(Config *config) {
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
    GSList *fs_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(g_widgets.fullscreen));
    for (int i = 0; i < 3; i++) {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_slist_nth_data(fs_group, i)))) {
            config->fullscreen = 2 - i;  // Reverse index
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

    // Gamepad quick save/load bindings
    if (g_gamepad_save) free(g_gamepad_save);
    g_gamepad_save = strdup(gtk_entry_get_text(GTK_ENTRY(g_widgets.gamepad_save_entry)));

    if (g_gamepad_load) free(g_gamepad_load);
    g_gamepad_load = strdup(gtk_entry_get_text(GTK_ENTRY(g_widgets.gamepad_load_entry)));

    // Read keyboard save state bindings (30 buttons)
    char **state_arrays[] = {g_kbd_load, g_kbd_save, g_kbd_replay};
    for (int type = 0; type < 3; type++) {
        for (int i = 0; i < 10; i++) {
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

    // Read keyboard system bindings (12 buttons)
    char **system_ptrs[] = {
        &g_kbd_clear_keylog,
        &g_kbd_stop_replay,
        &g_kbd_fullscreen,
        &g_kbd_reset,
        &g_kbd_pause_dimmed,
        &g_kbd_pause,
        &g_kbd_turbo,
        &g_kbd_replay_turbo,
        &g_kbd_window_bigger,
        &g_kbd_window_smaller,
        &g_kbd_volume_up,
        &g_kbd_volume_down
    };
    for (int i = 0; i < 12; i++) {
        char key[32];
        snprintf(key, sizeof(key), "system_%d", i);
        GtkWidget *button = g_object_get_data(G_OBJECT(g_widgets.kbd_system_grid), key);
        if (button) {
            if (*system_ptrs[i]) free(*system_ptrs[i]);
            *system_ptrs[i] = strdup(gtk_button_get_label(GTK_BUTTON(button)));
        }
    }
}
