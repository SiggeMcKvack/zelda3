// GTK-free storage for the keyboard/gamepad control-mapping strings.
//
// These are pass-through strings that are serialized to/from zelda3.ini
// ([KeyMap] / [GamepadMap]). They were originally defined in launcher_ui.c,
// which forced the INI reader/writer to pull in GTK. They live here so the
// config read/write code (and the future Tauri FFI layer) can be built
// without any GTK dependency. See control_strings.h for the parse/format
// helpers that operate on these arrays.
#pragma once

// Array size constants
#define NUM_SNES_BUTTONS        12
#define NUM_SAVE_SLOTS          10
#define NUM_REF_SLOTS           13  // reference saves (saves/ref chapters)

// Control mappings (12 SNES controls: Up, Down, Left, Right, Select, Start, A, B, X, Y, L, R)
// Stored separately from Config since they're just pass-through strings to INI.
extern char *g_kbd_controls[12];
extern char *g_gamepad_controls[12];

// Keyboard save states (10 slots each)
extern char *g_kbd_load[10];
extern char *g_kbd_save[10];
extern char *g_kbd_replay[10];
extern char *g_kbd_load_ref[13];
extern char *g_kbd_replay_ref[13];

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
extern char *g_kbd_display_perf;
extern char *g_kbd_toggle_renderer;

// Gamepad save states (10 slots each)
extern char *g_gamepad_load[10];
extern char *g_gamepad_save[10];
extern char *g_gamepad_replay[10];
extern char *g_gamepad_load_ref[13];
extern char *g_gamepad_replay_ref[13];

// Gamepad cheats
extern char *g_gamepad_cheat_life;
extern char *g_gamepad_cheat_keys;
extern char *g_gamepad_cheat_walkthrough;

// Gamepad system controls
extern char *g_gamepad_clear_keylog;
extern char *g_gamepad_stop_replay;
extern char *g_gamepad_fullscreen;
extern char *g_gamepad_reset;
extern char *g_gamepad_pause_dimmed;
extern char *g_gamepad_pause;
extern char *g_gamepad_turbo;
extern char *g_gamepad_replay_turbo;
extern char *g_gamepad_window_bigger;
extern char *g_gamepad_window_smaller;
extern char *g_gamepad_volume_up;
extern char *g_gamepad_volume_down;
extern char *g_gamepad_display_perf;
extern char *g_gamepad_toggle_renderer;
