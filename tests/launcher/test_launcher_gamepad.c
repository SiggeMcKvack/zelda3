/*
 * Unit tests for src/launcher/launcher_gamepad.c
 */

/* Must be defined before any SDL headers are included (including via launcher_gamepad.h) */
#define SDL_MAIN_HANDLED

#include "unity.h"
#include "test_utils.h"

#include "launcher_gamepad.h"
#include <SDL.h>
#include <string.h>

/* ===========================================================================
 * Test setup/teardown
 * ===========================================================================*/

void setUp(void) {
    /* Called before each test */
}

void tearDown(void) {
    /* Called after each test */
}

/* ===========================================================================
 * Tests for LauncherGamepad_ButtonToString()
 * ===========================================================================*/

void test_button_to_string_a(void) {
    TEST_ASSERT_EQUAL_STRING("A", LauncherGamepad_ButtonToString(SDL_CONTROLLER_BUTTON_A));
}

void test_button_to_string_b(void) {
    TEST_ASSERT_EQUAL_STRING("B", LauncherGamepad_ButtonToString(SDL_CONTROLLER_BUTTON_B));
}

void test_button_to_string_x(void) {
    TEST_ASSERT_EQUAL_STRING("X", LauncherGamepad_ButtonToString(SDL_CONTROLLER_BUTTON_X));
}

void test_button_to_string_y(void) {
    TEST_ASSERT_EQUAL_STRING("Y", LauncherGamepad_ButtonToString(SDL_CONTROLLER_BUTTON_Y));
}

void test_button_to_string_back(void) {
    TEST_ASSERT_EQUAL_STRING("Back", LauncherGamepad_ButtonToString(SDL_CONTROLLER_BUTTON_BACK));
}

void test_button_to_string_guide(void) {
    TEST_ASSERT_EQUAL_STRING("Guide", LauncherGamepad_ButtonToString(SDL_CONTROLLER_BUTTON_GUIDE));
}

void test_button_to_string_start(void) {
    TEST_ASSERT_EQUAL_STRING("Start", LauncherGamepad_ButtonToString(SDL_CONTROLLER_BUTTON_START));
}

void test_button_to_string_left_stick(void) {
    TEST_ASSERT_EQUAL_STRING("L3", LauncherGamepad_ButtonToString(SDL_CONTROLLER_BUTTON_LEFTSTICK));
}

void test_button_to_string_right_stick(void) {
    TEST_ASSERT_EQUAL_STRING("R3", LauncherGamepad_ButtonToString(SDL_CONTROLLER_BUTTON_RIGHTSTICK));
}

void test_button_to_string_left_shoulder(void) {
    TEST_ASSERT_EQUAL_STRING("Lb", LauncherGamepad_ButtonToString(SDL_CONTROLLER_BUTTON_LEFTSHOULDER));
}

void test_button_to_string_right_shoulder(void) {
    TEST_ASSERT_EQUAL_STRING("Rb", LauncherGamepad_ButtonToString(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));
}

void test_button_to_string_dpad(void) {
    TEST_ASSERT_EQUAL_STRING("DpadUp", LauncherGamepad_ButtonToString(SDL_CONTROLLER_BUTTON_DPAD_UP));
    TEST_ASSERT_EQUAL_STRING("DpadDown", LauncherGamepad_ButtonToString(SDL_CONTROLLER_BUTTON_DPAD_DOWN));
    TEST_ASSERT_EQUAL_STRING("DpadLeft", LauncherGamepad_ButtonToString(SDL_CONTROLLER_BUTTON_DPAD_LEFT));
    TEST_ASSERT_EQUAL_STRING("DpadRight", LauncherGamepad_ButtonToString(SDL_CONTROLLER_BUTTON_DPAD_RIGHT));
}

void test_button_to_string_invalid(void) {
    TEST_ASSERT_NULL(LauncherGamepad_ButtonToString(SDL_CONTROLLER_BUTTON_MAX));
    TEST_ASSERT_NULL(LauncherGamepad_ButtonToString((SDL_GameControllerButton)-1));
}

/* ===========================================================================
 * Tests for LauncherGamepad_AxisToString()
 * ===========================================================================*/

void test_axis_to_string_left_stick_positive(void) {
    const char *result = LauncherGamepad_AxisToString(SDL_CONTROLLER_AXIS_LEFTX, 1);
    TEST_ASSERT_EQUAL_STRING("LeftX+", result);

    result = LauncherGamepad_AxisToString(SDL_CONTROLLER_AXIS_LEFTY, 1);
    TEST_ASSERT_EQUAL_STRING("LeftY+", result);
}

void test_axis_to_string_left_stick_negative(void) {
    const char *result = LauncherGamepad_AxisToString(SDL_CONTROLLER_AXIS_LEFTX, -1);
    TEST_ASSERT_EQUAL_STRING("LeftX-", result);

    result = LauncherGamepad_AxisToString(SDL_CONTROLLER_AXIS_LEFTY, -1);
    TEST_ASSERT_EQUAL_STRING("LeftY-", result);
}

void test_axis_to_string_right_stick(void) {
    const char *result = LauncherGamepad_AxisToString(SDL_CONTROLLER_AXIS_RIGHTX, 1);
    TEST_ASSERT_EQUAL_STRING("RightX+", result);

    result = LauncherGamepad_AxisToString(SDL_CONTROLLER_AXIS_RIGHTX, -1);
    TEST_ASSERT_EQUAL_STRING("RightX-", result);

    result = LauncherGamepad_AxisToString(SDL_CONTROLLER_AXIS_RIGHTY, 1);
    TEST_ASSERT_EQUAL_STRING("RightY+", result);

    result = LauncherGamepad_AxisToString(SDL_CONTROLLER_AXIS_RIGHTY, -1);
    TEST_ASSERT_EQUAL_STRING("RightY-", result);
}

void test_axis_to_string_triggers(void) {
    /* Triggers don't have +/- suffix since they're 0-1 range */
    const char *result = LauncherGamepad_AxisToString(SDL_CONTROLLER_AXIS_TRIGGERLEFT, 1);
    TEST_ASSERT_EQUAL_STRING("L2", result);

    result = LauncherGamepad_AxisToString(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 1);
    TEST_ASSERT_EQUAL_STRING("R2", result);
}

void test_axis_to_string_invalid(void) {
    TEST_ASSERT_NULL(LauncherGamepad_AxisToString(SDL_CONTROLLER_AXIS_MAX, 1));
    TEST_ASSERT_NULL(LauncherGamepad_AxisToString((SDL_GameControllerAxis)-1, 1));
}

void test_axis_to_string_multiple_calls(void) {
    /* Test that rotating buffers work correctly */
    const char *a = LauncherGamepad_AxisToString(SDL_CONTROLLER_AXIS_LEFTX, 1);
    const char *b = LauncherGamepad_AxisToString(SDL_CONTROLLER_AXIS_LEFTX, -1);
    const char *c = LauncherGamepad_AxisToString(SDL_CONTROLLER_AXIS_RIGHTX, 1);
    const char *d = LauncherGamepad_AxisToString(SDL_CONTROLLER_AXIS_RIGHTX, -1);

    /* All should be different pointers and have correct values */
    TEST_ASSERT_EQUAL_STRING("LeftX+", a);
    TEST_ASSERT_EQUAL_STRING("LeftX-", b);
    TEST_ASSERT_EQUAL_STRING("RightX+", c);
    TEST_ASSERT_EQUAL_STRING("RightX-", d);
}

/* ===========================================================================
 * Test runner
 * ===========================================================================*/

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    /* LauncherGamepad_ButtonToString tests */
    RUN_TEST(test_button_to_string_a);
    RUN_TEST(test_button_to_string_b);
    RUN_TEST(test_button_to_string_x);
    RUN_TEST(test_button_to_string_y);
    RUN_TEST(test_button_to_string_back);
    RUN_TEST(test_button_to_string_guide);
    RUN_TEST(test_button_to_string_start);
    RUN_TEST(test_button_to_string_left_stick);
    RUN_TEST(test_button_to_string_right_stick);
    RUN_TEST(test_button_to_string_left_shoulder);
    RUN_TEST(test_button_to_string_right_shoulder);
    RUN_TEST(test_button_to_string_dpad);
    RUN_TEST(test_button_to_string_invalid);

    /* LauncherGamepad_AxisToString tests */
    RUN_TEST(test_axis_to_string_left_stick_positive);
    RUN_TEST(test_axis_to_string_left_stick_negative);
    RUN_TEST(test_axis_to_string_right_stick);
    RUN_TEST(test_axis_to_string_triggers);
    RUN_TEST(test_axis_to_string_invalid);
    RUN_TEST(test_axis_to_string_multiple_calls);

    return UNITY_END();
}
