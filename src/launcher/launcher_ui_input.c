// Input mapping tab implementation for Zelda3 Launcher
// This file contains keyboard and gamepad mapping UI code
// Extracted from launcher_ui.c to improve maintainability

#include "launcher_ui_internal.h"
#include "launcher_gamepad.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// Global grid widget references (accessed by launcher_ui.c for config update)
// ============================================================================

GtkWidget *g_kbd_states_grid = NULL;
GtkWidget *g_kbd_cheats_grid = NULL;
GtkWidget *g_kbd_system_grid = NULL;
GtkWidget *g_gamepad_states_grid = NULL;
GtkWidget *g_gamepad_cheats_grid = NULL;
GtkWidget *g_gamepad_system_grid = NULL;

// ============================================================================
// Keyboard capture state
// ============================================================================

static int g_captured_control_index = -1;
static GtkWidget *g_capture_dialog = NULL;
static bool g_key_captured = false;

// ============================================================================
// Data structure for clear button handler
// ============================================================================

typedef struct {
    GtkWidget *button;  // The binding button to update
    char **variable;    // Pointer to the variable to clear
} ClearButtonData;

// ============================================================================
// Keyboard capture handlers
// ============================================================================

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
    static const struct {
        const char *gdk;
        const char *sdl;
    } key_mapping[] = {
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
        {NULL, NULL}  // Sentinel
    };

    // Map GDK key name to SDL scancode name
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
            strncpy(key_name, mapped_name, sizeof(key_name) - 1);
        } else if (strlen(gdk_name) == 1 && gdk_name[0] >= 'a' && gdk_name[0] <= 'z') {
            // Capitalize single letters
            key_name[0] = gdk_name[0] - 'a' + 'A';
            key_name[1] = '\0';
        } else {
            // For other keys, use GDK name as-is (F1, Return, etc.)
            strncpy(key_name, gdk_name, sizeof(key_name) - 1);
        }
    }

    if (key_name[0]) {
        // Build full key string with modifiers
        char full_key[128] = {0};

        if (event->state & GDK_CONTROL_MASK) strcat(full_key, "Ctrl+");
        if (event->state & GDK_SHIFT_MASK) strcat(full_key, "Shift+");
        if (event->state & GDK_MOD1_MASK) strcat(full_key, "Alt+");

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
            if (*var_ptr) free(*var_ptr);
            *var_ptr = strdup(full_key);
        }

        gtk_button_set_label(GTK_BUTTON(button), full_key);
        g_key_captured = true;
        gtk_dialog_response(GTK_DIALOG(g_capture_dialog), GTK_RESPONSE_OK);
    }

    return TRUE;
}

// Generic keyboard button click handler
static void on_key_button_clicked(GtkWidget *button, gpointer user_data) {
    const char *prompt = (const char*)user_data;

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
    snprintf(markup, sizeof(markup),
             "<big><b>Press a key for: %s</b></big>\n\n"
             "(supports Ctrl+, Shift+, Alt+ modifiers)\n(or Cancel to abort)", prompt);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_container_add(GTK_CONTAINER(content), label);

    g_signal_connect(g_capture_dialog, "key-press-event", G_CALLBACK(on_key_press_capture), button);

    gtk_widget_show_all(g_capture_dialog);
    gtk_dialog_run(GTK_DIALOG(g_capture_dialog));
    gtk_widget_destroy(g_capture_dialog);
    g_capture_dialog = NULL;
}

// Clear button click handler
static void on_clear_button_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    ClearButtonData *data = (ClearButtonData*)user_data;

    if (data->variable && *data->variable) {
        free(*data->variable);
    }
    *data->variable = strdup("");
    gtk_button_set_label(GTK_BUTTON(data->button), "(not set)");
}

// Keyboard button click handler for controls
static void on_keyboard_button_clicked(GtkWidget *button, gpointer user_data) {
    int index = GPOINTER_TO_INT(user_data);
    g_captured_control_index = index;
    g_key_captured = false;
    on_key_button_clicked(button, (gpointer)kControlNames[index]);
}

// ============================================================================
// Gamepad capture handler
// ============================================================================

static void on_gamepad_button_clicked(GtkWidget *button, gpointer user_data) {
    char **var_ptr = (char**)g_object_get_data(G_OBJECT(button), "variable_ptr");
    const char *prompt = (const char*)g_object_get_data(G_OBJECT(button), "prompt");
    int index = -1;

    if (!var_ptr) {
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
    snprintf(markup, sizeof(markup),
             "<big><b>Press a button/axis for: %s</b></big>\n\n(5 second timeout or Cancel)",
             prompt);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_container_add(GTK_CONTAINER(content), label);

    gtk_widget_show_all(dialog);

    DetectedInput input = LauncherGamepad_DetectInput(gamepads[0].controller, 5000);

    const char *captured_name = NULL;
    if (input.type == INPUT_TYPE_BUTTON) {
        captured_name = LauncherGamepad_ButtonToString(input.button);
    } else if (input.type == INPUT_TYPE_AXIS) {
        captured_name = LauncherGamepad_AxisToString(input.axis, input.axis_value);
    }

    if (captured_name) {
        if (var_ptr) {
            if (*var_ptr) free(*var_ptr);
            *var_ptr = strdup(captured_name);
            gtk_button_set_label(GTK_BUTTON(button), captured_name);
        } else {
            if (g_gamepad_controls[index]) free(g_gamepad_controls[index]);
            g_gamepad_controls[index] = strdup(captured_name);
            char btn_label[128];
            snprintf(btn_label, sizeof(btn_label), "%s: %s", kControlNames[index], captured_name);
            gtk_button_set_label(GTK_BUTTON(button), btn_label);
        }
    }

    LauncherGamepad_Close(gamepads[0].controller);
    gtk_widget_destroy(dialog);
}

// ============================================================================
// Helper to add a binding button with clear button
// ============================================================================

// Add a binding row with label, button, and clear button
// - grid_key: if non-NULL, stores button in grid's data for later retrieval
// - attach_var_to_button: if true, stores variable_ptr in button's data
// - prompt: if non-NULL, stores in button's data (used by gamepad handler)
static void add_binding_row(GtkWidget *grid, int row, const char *label_text,
                            const char *value, GCallback handler, gpointer handler_data,
                            char **variable_ptr, const char *grid_key,
                            bool attach_var_to_button, const char *prompt) {
    // Label (left-aligned, expands to fill available space)
    GtkWidget *label = gtk_label_new(label_text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);

    // Binding button (fixed width, no expansion)
    GtkWidget *button = gtk_button_new_with_label(get_display_value(value));
    gtk_widget_set_size_request(button, BUTTON_WIDTH, BUTTON_HEIGHT);
    gtk_widget_set_hexpand(button, FALSE);
    g_signal_connect(button, "clicked", handler, handler_data);
    gtk_grid_attach(GTK_GRID(grid), button, 1, row, 1, 1);

    if (grid_key) {
        g_object_set_data(G_OBJECT(grid), g_strdup(grid_key), button);
    }
    if (attach_var_to_button && variable_ptr) {
        g_object_set_data(G_OBJECT(button), "variable_ptr", variable_ptr);
    }
    if (prompt) {
        g_object_set_data(G_OBJECT(button), "prompt", (gpointer)prompt);
    }

    // Clear button
    GtkWidget *clear_button = gtk_button_new_with_label("Clear");
    gtk_widget_set_size_request(clear_button, CLEAR_BUTTON_WIDTH, BUTTON_HEIGHT);
    ClearButtonData *clear_data = malloc(sizeof(ClearButtonData));
    if (clear_data) {
        clear_data->button = button;
        clear_data->variable = variable_ptr;
        g_signal_connect(clear_button, "clicked", G_CALLBACK(on_clear_button_clicked), clear_data);
        g_signal_connect_swapped(clear_button, "destroy", G_CALLBACK(free), clear_data);
    }
    gtk_grid_attach(GTK_GRID(grid), clear_button, 2, row, 1, 1);
}

// ============================================================================
// Keyboard tab creation
// ============================================================================

GtkWidget* create_keymap_tab(const Config *config) {
    (void)config;

    GtkWidget *notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);

    // --- Controls Subtab ---
    GtkWidget *controls_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(controls_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    GtkWidget *controls_grid = create_standard_grid();

    GtkWidget *controls_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(controls_title), "<b>SNES Controller</b>");
    gtk_widget_set_halign(controls_title, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(controls_grid), controls_title, 0, 0, 3, 1);

    for (int i = 0; i < NUM_SNES_BUTTONS; i++) {
        char label_text[64];
        snprintf(label_text, sizeof(label_text), "%s:", kControlNames[i]);
        add_binding_row(controls_grid, i + 1, label_text, g_kbd_controls[i],
                        G_CALLBACK(on_keyboard_button_clicked), GINT_TO_POINTER(i),
                        &g_kbd_controls[i], NULL, false, NULL);
    }

    gtk_container_add(GTK_CONTAINER(controls_scroll), controls_grid);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), controls_scroll, gtk_label_new("Controls"));

    // --- Save States Subtab ---
    GtkWidget *states_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(states_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    g_kbd_states_grid = create_standard_grid();

    GtkWidget *states_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(states_title), "<b>Save States</b>");
    gtk_widget_set_halign(states_title, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(g_kbd_states_grid), states_title, 0, 0, 2, 1);

    int row = 1;
    const char *state_labels[] = {"Load", "Save", "Replay"};
    char **state_arrays[] = {g_kbd_load, g_kbd_save, g_kbd_replay};

    for (int type = 0; type < 3; type++) {
        GtkWidget *type_label = gtk_label_new(NULL);
        char markup[64];
        snprintf(markup, sizeof(markup), "<b>%s:</b>", state_labels[type]);
        gtk_label_set_markup(GTK_LABEL(type_label), markup);
        gtk_widget_set_halign(type_label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(g_kbd_states_grid), type_label, 0, row++, 3, 1);

        for (int i = 0; i < NUM_SAVE_SLOTS; i++) {
            char slot_label[64], grid_key[32];
            snprintf(slot_label, sizeof(slot_label), "%s Slot %d:", state_labels[type], i + 1);
            snprintf(grid_key, sizeof(grid_key), "state_%d_%d", type, i);
            add_binding_row(g_kbd_states_grid, row++, slot_label, state_arrays[type][i],
                            G_CALLBACK(on_key_button_clicked), (gpointer)slot_label,
                            &state_arrays[type][i], grid_key, true, NULL);
        }
    }

    gtk_container_add(GTK_CONTAINER(states_scroll), g_kbd_states_grid);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), states_scroll, gtk_label_new("Save States"));

    // --- Cheats Subtab ---
    GtkWidget *cheats_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cheats_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    g_kbd_cheats_grid = create_standard_grid();

    GtkWidget *cheats_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(cheats_title), "<b>Cheats</b>");
    gtk_widget_set_halign(cheats_title, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(g_kbd_cheats_grid), cheats_title, 0, 0, 3, 1);

    struct { const char *label; char **ptr; } cheats[] = {
        {"Refill Health & Magic:", &g_kbd_cheat_life},
        {"Set key count to 1:", &g_kbd_cheat_keys},
        {"Toggle Walk Through Walls:", &g_kbd_cheat_walkthrough}
    };

    for (int i = 0; i < 3; i++) {
        char grid_key[32];
        snprintf(grid_key, sizeof(grid_key), "cheat_%d", i);
        add_binding_row(g_kbd_cheats_grid, i + 1, cheats[i].label, *cheats[i].ptr,
                        G_CALLBACK(on_key_button_clicked), (gpointer)cheats[i].label,
                        cheats[i].ptr, grid_key, true, NULL);
    }

    gtk_container_add(GTK_CONTAINER(cheats_scroll), g_kbd_cheats_grid);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), cheats_scroll, gtk_label_new("Cheats"));

    // --- System Subtab ---
    GtkWidget *system_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(system_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    g_kbd_system_grid = create_standard_grid();

    GtkWidget *system_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(system_title), "<b>System Controls</b>");
    gtk_widget_set_halign(system_title, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(g_kbd_system_grid), system_title, 0, 0, 3, 1);

    struct { const char *label; char **ptr; } system_keys[] = {
        {"Toggle Fullscreen:", &g_kbd_fullscreen},
        {"Reset:", &g_kbd_reset},
        {"Pause (Dimmed):", &g_kbd_pause_dimmed},
        {"Pause:", &g_kbd_pause},
        {"Turbo:", &g_kbd_turbo},
        {"Replay Turbo:", &g_kbd_replay_turbo},
        {"Window Bigger:", &g_kbd_window_bigger},
        {"Window Smaller:", &g_kbd_window_smaller},
        {"Volume Up:", &g_kbd_volume_up},
        {"Volume Down:", &g_kbd_volume_down},
        {"Display FPS:", &g_kbd_display_perf},
        {"Toggle Renderer:", &g_kbd_toggle_renderer},
        {"Stop Replay:", &g_kbd_stop_replay},
        {"Clear input recording log (debug):", &g_kbd_clear_keylog}
    };

    for (int i = 0; i < NUM_SYSTEM_KEYS; i++) {
        char grid_key[32];
        snprintf(grid_key, sizeof(grid_key), "system_%d", i);
        add_binding_row(g_kbd_system_grid, i + 1, system_keys[i].label, *system_keys[i].ptr,
                        G_CALLBACK(on_key_button_clicked), (gpointer)system_keys[i].label,
                        system_keys[i].ptr, grid_key, true, NULL);
    }

    gtk_container_add(GTK_CONTAINER(system_scroll), g_kbd_system_grid);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), system_scroll, gtk_label_new("System"));

    return notebook;
}

// ============================================================================
// Gamepad tab creation
// ============================================================================

GtkWidget* create_gamepadmap_tab(const Config *config) {
    (void)config;

    GtkWidget *notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);

    // --- Controls Subtab ---
    GtkWidget *controls_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(controls_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    GtkWidget *controls_grid = create_standard_grid();

    GtkWidget *controls_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(controls_title), "<b>SNES Controller</b>");
    gtk_widget_set_halign(controls_title, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(controls_grid), controls_title, 0, 0, 3, 1);

    for (int i = 0; i < NUM_SNES_BUTTONS; i++) {
        char label_text[64];
        snprintf(label_text, sizeof(label_text), "%s:", kControlNames[i]);
        add_binding_row(controls_grid, i + 1, label_text, g_gamepad_controls[i],
                        G_CALLBACK(on_gamepad_button_clicked), GINT_TO_POINTER(i),
                        &g_gamepad_controls[i], NULL, false, NULL);
    }

    gtk_container_add(GTK_CONTAINER(controls_scroll), controls_grid);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), controls_scroll, gtk_label_new("Controls"));

    // --- Save States Subtab ---
    GtkWidget *states_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(states_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    g_gamepad_states_grid = create_standard_grid();

    GtkWidget *states_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(states_title), "<b>Save States</b>");
    gtk_widget_set_halign(states_title, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(g_gamepad_states_grid), states_title, 0, 0, 2, 1);

    int row = 1;
    const char *state_labels[] = {"Load", "Save", "Replay"};
    char **state_arrays[] = {g_gamepad_load, g_gamepad_save, g_gamepad_replay};

    for (int type = 0; type < 3; type++) {
        GtkWidget *type_label = gtk_label_new(NULL);
        char markup[64];
        snprintf(markup, sizeof(markup), "<b>%s:</b>", state_labels[type]);
        gtk_label_set_markup(GTK_LABEL(type_label), markup);
        gtk_widget_set_halign(type_label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(g_gamepad_states_grid), type_label, 0, row++, 3, 1);

        for (int i = 0; i < NUM_SAVE_SLOTS; i++) {
            char slot_label[64], grid_key[32];
            snprintf(slot_label, sizeof(slot_label), "%s Slot %d:", state_labels[type], i + 1);
            snprintf(grid_key, sizeof(grid_key), "state_%d_%d", type, i);
            add_binding_row(g_gamepad_states_grid, row++, slot_label, state_arrays[type][i],
                            G_CALLBACK(on_gamepad_button_clicked), NULL,
                            &state_arrays[type][i], grid_key, true, slot_label);
        }
    }

    gtk_container_add(GTK_CONTAINER(states_scroll), g_gamepad_states_grid);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), states_scroll, gtk_label_new("Save States"));

    // --- Cheats Subtab ---
    GtkWidget *cheats_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cheats_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    g_gamepad_cheats_grid = create_standard_grid();

    GtkWidget *cheats_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(cheats_title), "<b>Cheats</b>");
    gtk_widget_set_halign(cheats_title, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(g_gamepad_cheats_grid), cheats_title, 0, 0, 3, 1);

    struct { const char *label; char **ptr; } cheats[] = {
        {"Refill Health & Magic:", &g_gamepad_cheat_life},
        {"Set key count to 1:", &g_gamepad_cheat_keys},
        {"Toggle Walk Through Walls:", &g_gamepad_cheat_walkthrough}
    };

    for (int i = 0; i < 3; i++) {
        char grid_key[32];
        snprintf(grid_key, sizeof(grid_key), "cheat_%d", i);
        add_binding_row(g_gamepad_cheats_grid, i + 1, cheats[i].label, *cheats[i].ptr,
                        G_CALLBACK(on_gamepad_button_clicked), NULL,
                        cheats[i].ptr, grid_key, true, cheats[i].label);
    }

    gtk_container_add(GTK_CONTAINER(cheats_scroll), g_gamepad_cheats_grid);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), cheats_scroll, gtk_label_new("Cheats"));

    // --- System Subtab ---
    GtkWidget *system_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(system_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    g_gamepad_system_grid = create_standard_grid();

    GtkWidget *system_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(system_title), "<b>System Controls</b>");
    gtk_widget_set_halign(system_title, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(g_gamepad_system_grid), system_title, 0, 0, 3, 1);

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
        char grid_key[32];
        snprintf(grid_key, sizeof(grid_key), "system_%d", i);
        add_binding_row(g_gamepad_system_grid, i + 1, system_keys[i].label, *system_keys[i].ptr,
                        G_CALLBACK(on_gamepad_button_clicked), NULL,
                        system_keys[i].ptr, grid_key, true, system_keys[i].label);
    }

    gtk_container_add(GTK_CONTAINER(system_scroll), g_gamepad_system_grid);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), system_scroll, gtk_label_new("System"));

    return notebook;
}
