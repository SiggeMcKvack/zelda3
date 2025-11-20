#include "launcher_gamepad.h"
#include "logging.h"
#include <SDL.h>
#include <stdbool.h>
#include <string.h>

// List all connected game controllers
int LauncherGamepad_ListControllers(GamepadInfo *gamepads, int max_gamepads) {
    int num_joysticks = SDL_NumJoysticks();
    int count = 0;

    for (int i = 0; i < num_joysticks && count < max_gamepads; i++) {
        if (SDL_IsGameController(i)) {
            if (gamepads) {
                SDL_GameController *controller = SDL_GameControllerOpen(i);
                if (controller) {
                    gamepads[count].device_index = i;
                    gamepads[count].name = SDL_GameControllerName(controller);
                    gamepads[count].controller = controller;
                    LogInfo("Found gamepad %d: %s", i, gamepads[count].name);
                } else {
                    LogWarn("Failed to open controller %d", i);
                    continue;
                }
            }
            count++;
        }
    }

    LogInfo("Found %d gamepad(s)", count);
    return count;
}

// Detect button or axis input with timeout
DetectedInput LauncherGamepad_DetectInput(SDL_GameController *controller, int timeout_ms) {
    DetectedInput result = { .type = INPUT_TYPE_NONE };

    if (!controller) {
        LogError("LauncherGamepad_DetectInput: NULL controller");
        return result;
    }

    Uint32 start_time = SDL_GetTicks();

    // Main detection loop
    while (timeout_ms == 0 || (SDL_GetTicks() - start_time) < (Uint32)timeout_ms) {
        SDL_Event event;

        // Poll all events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                // Check if this event is from our controller
                SDL_GameController *event_controller = SDL_GameControllerFromInstanceID(event.cbutton.which);
                if (event_controller == controller) {
                    result.type = INPUT_TYPE_BUTTON;
                    result.button = (SDL_GameControllerButton)event.cbutton.button;
                    LogInfo("Detected button: %s", SDL_GameControllerGetStringForButton(result.button));
                    return result;
                }
            } else if (event.type == SDL_CONTROLLERAXISMOTION) {
                // Check if this event is from our controller
                SDL_GameController *event_controller = SDL_GameControllerFromInstanceID(event.caxis.which);
                if (event_controller == controller) {
                    // Axis threshold (ignore small movements)
                    const int AXIS_THRESHOLD = 16384;  // ~50% deflection

                    if (event.caxis.value > AXIS_THRESHOLD) {
                        result.type = INPUT_TYPE_AXIS;
                        result.axis = (SDL_GameControllerAxis)event.caxis.axis;
                        result.axis_value = 1;  // Positive
                        LogInfo("Detected axis: %s+", SDL_GameControllerGetStringForAxis(result.axis));
                        return result;
                    } else if (event.caxis.value < -AXIS_THRESHOLD) {
                        result.type = INPUT_TYPE_AXIS;
                        result.axis = (SDL_GameControllerAxis)event.caxis.axis;
                        result.axis_value = -1;  // Negative
                        LogInfo("Detected axis: %s-", SDL_GameControllerGetStringForAxis(result.axis));
                        return result;
                    }
                }
            }
        }

        // Small delay to avoid busy-waiting
        SDL_Delay(10);
    }

    // Timeout
    LogInfo("Input detection timed out");
    return result;
}

// Convert button enum to config string
const char* LauncherGamepad_ButtonToString(SDL_GameControllerButton button) {
    switch (button) {
        case SDL_CONTROLLER_BUTTON_A: return "A";
        case SDL_CONTROLLER_BUTTON_B: return "B";
        case SDL_CONTROLLER_BUTTON_X: return "X";
        case SDL_CONTROLLER_BUTTON_Y: return "Y";
        case SDL_CONTROLLER_BUTTON_BACK: return "Back";
        case SDL_CONTROLLER_BUTTON_GUIDE: return "Guide";
        case SDL_CONTROLLER_BUTTON_START: return "Start";
        case SDL_CONTROLLER_BUTTON_LEFTSTICK: return "L3";
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return "R3";
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return "Lb";
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "Rb";
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return "DpadUp";
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return "DpadDown";
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return "DpadLeft";
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return "DpadRight";
        default: return NULL;
    }
}

// Convert axis enum to config string with direction
const char* LauncherGamepad_AxisToString(SDL_GameControllerAxis axis, int axis_value) {
    // Static buffers for return values (cycling to allow multiple calls)
    static char buffers[4][32];
    static int buffer_index = 0;

    char *buf = buffers[buffer_index];
    buffer_index = (buffer_index + 1) % 4;

    const char *axis_name = NULL;
    switch (axis) {
        case SDL_CONTROLLER_AXIS_LEFTX: axis_name = "LeftX"; break;
        case SDL_CONTROLLER_AXIS_LEFTY: axis_name = "LeftY"; break;
        case SDL_CONTROLLER_AXIS_RIGHTX: axis_name = "RightX"; break;
        case SDL_CONTROLLER_AXIS_RIGHTY: axis_name = "RightY"; break;
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT: axis_name = "L2"; break;
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: axis_name = "R2"; break;
        default: return NULL;
    }

    // For triggers, don't add direction suffix (they're 0-1 range)
    if (axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ||
        axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
        snprintf(buf, 32, "%s", axis_name);
    } else {
        // For sticks, add +/- suffix
        char dir = axis_value > 0 ? '+' : '-';
        snprintf(buf, 32, "%s%c", axis_name, dir);
    }

    return buf;
}

// Close gamepad controller
void LauncherGamepad_Close(SDL_GameController *controller) {
    if (controller) {
        SDL_GameControllerClose(controller);
    }
}
