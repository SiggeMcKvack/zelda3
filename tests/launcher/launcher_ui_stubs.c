/*
 * Stub implementations of launcher_ui.c functions and globals
 * Used for testing config_writer.c without GTK dependencies.
 */

#include <stdlib.h>
#include <string.h>

/* Stub globals - all NULL/empty for testing */
char *g_kbd_controls[12] = {NULL};
char *g_kbd_load[10] = {NULL};
char *g_kbd_save[10] = {NULL};
char *g_kbd_replay[10] = {NULL};
char *g_kbd_cheat_life = NULL;
char *g_kbd_cheat_keys = NULL;
char *g_kbd_cheat_walkthrough = NULL;
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

char *g_gamepad_controls[12] = {NULL};
char *g_gamepad_load[10] = {NULL};
char *g_gamepad_save[10] = {NULL};
char *g_gamepad_replay[10] = {NULL};
char *g_gamepad_cheat_life = NULL;
char *g_gamepad_cheat_keys = NULL;
char *g_gamepad_cheat_walkthrough = NULL;
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

/* Stub implementation of LauncherUI_FormatControlString */
char* LauncherUI_FormatControlString(char **controls, int count) {
    /* Return empty string for testing */
    return strdup("");
}

/* Stub implementation of LauncherUI_ParseControlString */
void LauncherUI_ParseControlString(const char *str, char **controls) {
    /* Do nothing */
    (void)str;
    (void)controls;
}

/* Stub implementation of LauncherUI_ParseGamepadControlString */
void LauncherUI_ParseGamepadControlString(const char *str, char **controls) {
    /* Do nothing */
    (void)str;
    (void)controls;
}
