// GTK-free helpers to parse/format the comma-separated control-mapping strings
// used in zelda3.ini ([KeyMap] / [GamepadMap]). Operate on the arrays declared
// in keymap_state.h. Relocated out of launcher_ui.c so the INI reader/writer
// can be built without GTK. The LauncherUI_ prefix is kept so existing callers
// (config_reader.c, config_writer.c, launcher_ui*.c) are unchanged.
#pragma once

// Parse a comma-separated control string into an array (keyboard defaults).
void LauncherUI_ParseControlString(const char *str, char **controls);

// Parse a comma-separated control string into an array (gamepad defaults).
void LauncherUI_ParseGamepadControlString(const char *str, char **controls);

// Format a control array into a comma-separated string. Caller frees the result.
char* LauncherUI_FormatControlString(char **controls, int count);
