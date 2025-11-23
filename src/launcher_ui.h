#pragma once

#include <gtk/gtk.h>
#include "config.h"

// Control mapping storage (12 SNES controls: Up, Down, Left, Right, Select, Start, A, B, X, Y, L, R)
// These are stored separately from Config since they're just pass-through strings to INI
extern char *g_kbd_controls[12];
extern char *g_gamepad_controls[12];

// Keyboard save states (10 slots each)
extern char *g_kbd_load[10];
extern char *g_kbd_save[10];
extern char *g_kbd_replay[10];

// Keyboard cheats
extern char *g_kbd_cheat_life;
extern char *g_kbd_cheat_keys;
extern char *g_kbd_cheat_walkthrough;

// Keyboard system controls
extern char *g_kbd_clear_keylog;
extern char *g_kbd_stop_replay;
extern char *g_kbd_fullscreen;
extern char *g_kbd_reset;
extern char *g_kbd_pause_dimmed;
extern char *g_kbd_pause;
extern char *g_kbd_turbo;
extern char *g_kbd_replay_turbo;
extern char *g_kbd_window_bigger;
extern char *g_kbd_window_smaller;
extern char *g_kbd_volume_up;
extern char *g_kbd_volume_down;

// Quick save/load bindings (gamepad only)
extern char *g_gamepad_save;
extern char *g_gamepad_load;

// Parse/format control strings for INI
void LauncherUI_ParseControlString(const char *str, char **controls);
void LauncherUI_ParseGamepadControlString(const char *str, char **controls);
char* LauncherUI_FormatControlString(char **controls);

// Create the main launcher window with all tabs
// Returns: GtkWidget* for the main window
// config: Current configuration to display
GtkWidget* LauncherUI_CreateWindow(Config *config);

// Update the config structure from UI widget values
// Must be called before saving config
// config: Config structure to update
void LauncherUI_UpdateConfigFromUI(Config *config);
