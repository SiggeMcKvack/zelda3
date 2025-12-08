/*
 * Unit tests for src/launcher/launcher_ui.c string utilities
 *
 * Note: Most launcher_ui functions require GTK, so we only test
 * the pure string manipulation functions here.
 */

#include "unity.h"
#include "test_utils.h"

#include "launcher_ui.h"
#include <string.h>
#include <stdlib.h>

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
 * Tests for LauncherUI_FormatControlString()
 * ===========================================================================*/

void test_format_control_string_full_array(void) {
    char *controls[4] = {"Up", "Down", "Left", "Right"};
    char *result = LauncherUI_FormatControlString(controls, 4);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("Up, Down, Left, Right", result);
    free(result);
}

void test_format_control_string_with_nulls(void) {
    char *controls[4] = {"Up", NULL, "Left", NULL};
    char *result = LauncherUI_FormatControlString(controls, 4);

    TEST_ASSERT_NOT_NULL(result);
    /* NULL entries become empty strings in comma list */
    TEST_ASSERT_STRING_CONTAINS(result, "Up");
    TEST_ASSERT_STRING_CONTAINS(result, "Left");
    free(result);
}

void test_format_control_string_single(void) {
    char *controls[1] = {"A"};
    char *result = LauncherUI_FormatControlString(controls, 1);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("A", result);
    free(result);
}

void test_format_control_string_empty(void) {
    char *controls[0] = {};
    char *result = LauncherUI_FormatControlString(controls, 0);

    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("", result);
    free(result);
}

/* ===========================================================================
 * Test runner
 * ===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* LauncherUI_FormatControlString tests */
    RUN_TEST(test_format_control_string_full_array);
    RUN_TEST(test_format_control_string_with_nulls);
    RUN_TEST(test_format_control_string_single);
    RUN_TEST(test_format_control_string_empty);

    return UNITY_END();
}
