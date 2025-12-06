// Internal header for launcher_ui implementation files
// This header is NOT part of the public API - only include in launcher_ui*.c files
#pragma once

#include "launcher_ui.h"
#include <gtk/gtk.h>
#include <stdbool.h>

// ============================================================================
// Constants shared between launcher_ui.c and launcher_ui_input.c
// ============================================================================

#define NUM_STATE_TYPES         3   // Load, Save, Replay
#define NUM_CHEAT_KEYS          3
#define NUM_SYSTEM_KEYS         14

#define BUTTON_WIDTH            150
#define BUTTON_HEIGHT           35
#define CLEAR_BUTTON_WIDTH      80

// Control names for UI display
extern const char *kControlNames[NUM_SNES_BUTTONS];

// ============================================================================
// Shared helper functions
// ============================================================================

// Get display value for a string, returns "(not set)" for NULL/empty
static inline const char* get_display_value(const char *value) {
    return (value && *value) ? value : "(not set)";
}

// Create standard grid with consistent spacing/margins (defined in launcher_ui.c)
GtkWidget* create_standard_grid(void);

// ============================================================================
// Widget grid references for input tabs
// Defined in launcher_ui_input.c, accessed by launcher_ui.c for config update
// ============================================================================

// Keyboard tab grids (for accessing buttons during config update)
extern GtkWidget *g_kbd_states_grid;
extern GtkWidget *g_kbd_cheats_grid;
extern GtkWidget *g_kbd_system_grid;

// Gamepad tab grids (for accessing buttons during config update)
extern GtkWidget *g_gamepad_states_grid;
extern GtkWidget *g_gamepad_cheats_grid;
extern GtkWidget *g_gamepad_system_grid;

// ============================================================================
// Input tab creation functions (implemented in launcher_ui_input.c)
// ============================================================================

// Create the keyboard mapping tab with all subtabs
GtkWidget* create_keymap_tab(const Config *config);

// Create the gamepad mapping tab with all subtabs
GtkWidget* create_gamepadmap_tab(const Config *config);
