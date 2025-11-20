#pragma once

#include <SDL.h>
#include <stdbool.h>

// Gamepad detection API for launcher
// Uses SDL2 GameController API

// Gamepad information structure
typedef struct {
    int device_index;           // SDL joystick device index
    const char *name;           // Controller name
    SDL_GameController *controller;  // SDL controller handle
} GamepadInfo;

// Detected input type
typedef enum {
    INPUT_TYPE_NONE,
    INPUT_TYPE_BUTTON,
    INPUT_TYPE_AXIS,
} InputType;

// Detected input information
typedef struct {
    InputType type;
    union {
        SDL_GameControllerButton button;
        SDL_GameControllerAxis axis;
    };
    int axis_value;  // For axis: -1 (negative), 0 (center), +1 (positive)
} DetectedInput;

// Get list of connected gamepads
// Returns: Number of gamepads found
// gamepads: Array to fill with gamepad info (can be NULL to just get count)
// max_gamepads: Size of gamepads array
int LauncherGamepad_ListControllers(GamepadInfo *gamepads, int max_gamepads);

// Detect a single button/axis press
// Blocking call that waits for input or timeout
// controller: SDL controller to monitor
// timeout_ms: Timeout in milliseconds (0 = no timeout)
// Returns: Detected input, or INPUT_TYPE_NONE on timeout/error
DetectedInput LauncherGamepad_DetectInput(SDL_GameController *controller, int timeout_ms);

// Convert SDL button enum to config string (e.g., "A", "B", "DpadUp")
// Returns: Static string, or NULL if invalid
const char* LauncherGamepad_ButtonToString(SDL_GameControllerButton button);

// Convert SDL axis enum to config string (e.g., "LeftX+", "RightY-")
// Returns: Static string, or NULL if invalid
// axis_value: -1 for negative, +1 for positive
const char* LauncherGamepad_AxisToString(SDL_GameControllerAxis axis, int axis_value);

// Close a gamepad controller
void LauncherGamepad_Close(SDL_GameController *controller);
