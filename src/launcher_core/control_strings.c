// GTK-free implementation of the control-string parse/format helpers.
// See control_strings.h. Relocated verbatim out of launcher_ui.c.
#include "control_strings.h"
#include "keymap_state.h"  // NUM_SNES_BUTTONS
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Internal: Parse comma-separated control string into array with given defaults
static void parse_control_string_internal(const char *str, char **controls,
                                          const char **defaults) {
    if (!str || !*str) {
        // Use defaults if no string provided
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

// Default keyboard mappings
static const char *kDefaultKeyboardControls[NUM_SNES_BUTTONS] = {
    "Up", "Down", "Left", "Right", "Right Shift", "Return",
    "X", "Z", "S", "A", "C", "V"
};

// Default gamepad mappings (Xbox-style controller layout)
static const char *kDefaultGamepadControls[NUM_SNES_BUTTONS] = {
    "DpadUp", "DpadDown", "DpadLeft", "DpadRight", "Back", "Start",
    "B", "A", "Y", "X", "L1", "R1"
};

// Parse comma-separated control string into array (keyboard controls)
void LauncherUI_ParseControlString(const char *str, char **controls) {
    parse_control_string_internal(str, controls, kDefaultKeyboardControls);
}

// Parse comma-separated control string into array (gamepad controls)
void LauncherUI_ParseGamepadControlString(const char *str, char **controls) {
    parse_control_string_internal(str, controls, kDefaultGamepadControls);
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
