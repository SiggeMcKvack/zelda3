// Definitions for the GTK-free keyboard/gamepad control-mapping storage.
// See keymap_state.h. Relocated out of launcher_ui.c so the INI reader/writer
// can be built without GTK.
#include "keymap_state.h"
#include <stddef.h>

// Control mappings (12 SNES controls)
char *g_kbd_controls[12] = {NULL};
char *g_gamepad_controls[12] = {NULL};

// Keyboard save states (10 slots each)
char *g_kbd_load[10] = {NULL};
char *g_kbd_save[10] = {NULL};
char *g_kbd_replay[10] = {NULL};
char *g_kbd_load_ref[13] = {NULL};
char *g_kbd_replay_ref[13] = {NULL};

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
char *g_gamepad_load_ref[13] = {NULL};
char *g_gamepad_replay_ref[13] = {NULL};

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
