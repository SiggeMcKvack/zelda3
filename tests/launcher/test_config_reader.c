/*
 * Unit tests for src/launcher/config_reader.c
 */

#include "unity.h"
#include "test_utils.h"

#include "config_reader_internal.h"
#include "config_reader.h"
#include "config_writer.h"
#include "../config.h"

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
 * Tests for trim_whitespace()
 * ===========================================================================*/

void test_trim_whitespace_leading(void) {
    char str[] = "   hello";
    char *result = trim_whitespace(str);
    TEST_ASSERT_EQUAL_STRING("hello", result);
}

void test_trim_whitespace_trailing(void) {
    char str[] = "hello   ";
    char *result = trim_whitespace(str);
    TEST_ASSERT_EQUAL_STRING("hello", result);
}

void test_trim_whitespace_both(void) {
    char str[] = "  hello world  ";
    char *result = trim_whitespace(str);
    TEST_ASSERT_EQUAL_STRING("hello world", result);
}

void test_trim_whitespace_empty(void) {
    char str[] = "   ";
    char *result = trim_whitespace(str);
    TEST_ASSERT_EQUAL_STRING("", result);
}

void test_trim_whitespace_no_whitespace(void) {
    char str[] = "hello";
    char *result = trim_whitespace(str);
    TEST_ASSERT_EQUAL_STRING("hello", result);
}

void test_trim_whitespace_tabs_and_newlines(void) {
    char str[] = "\t\n  hello \t\n ";
    char *result = trim_whitespace(str);
    TEST_ASSERT_EQUAL_STRING("hello", result);
}

/* ===========================================================================
 * Tests for parse_bool()
 * ===========================================================================*/

void test_parse_bool_one(void) {
    TEST_ASSERT_EQUAL_INT(1, parse_bool("1"));
}

void test_parse_bool_true_lowercase(void) {
    TEST_ASSERT_EQUAL_INT(1, parse_bool("true"));
}

void test_parse_bool_true_capitalized(void) {
    TEST_ASSERT_EQUAL_INT(1, parse_bool("True"));
}

void test_parse_bool_zero(void) {
    TEST_ASSERT_EQUAL_INT(0, parse_bool("0"));
}

void test_parse_bool_false(void) {
    TEST_ASSERT_EQUAL_INT(0, parse_bool("false"));
}

void test_parse_bool_invalid(void) {
    TEST_ASSERT_EQUAL_INT(0, parse_bool("yes"));
    TEST_ASSERT_EQUAL_INT(0, parse_bool("TRUE"));
    TEST_ASSERT_EQUAL_INT(0, parse_bool(""));
}

/* ===========================================================================
 * Tests for parse_int()
 * ===========================================================================*/

void test_parse_int_positive(void) {
    TEST_ASSERT_EQUAL_INT(42, parse_int("42"));
    TEST_ASSERT_EQUAL_INT(12345, parse_int("12345"));
}

void test_parse_int_negative(void) {
    TEST_ASSERT_EQUAL_INT(-1, parse_int("-1"));
    TEST_ASSERT_EQUAL_INT(-999, parse_int("-999"));
}

void test_parse_int_zero(void) {
    TEST_ASSERT_EQUAL_INT(0, parse_int("0"));
}

void test_parse_int_invalid(void) {
    TEST_ASSERT_EQUAL_INT(0, parse_int("abc"));
    TEST_ASSERT_EQUAL_INT(0, parse_int(""));
}

/* ===========================================================================
 * Tests for parse_string()
 * ===========================================================================*/

void test_parse_string_normal(void) {
    char *result = parse_string("hello");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("hello", result);
    free(result);
}

void test_parse_string_null(void) {
    char *result = parse_string(NULL);
    TEST_ASSERT_NULL(result);
}

void test_parse_string_empty(void) {
    char *result = parse_string("");
    TEST_ASSERT_NULL(result);
}

/* ===========================================================================
 * Tests for parse_aspect_ratio()
 * ===========================================================================*/

void test_parse_aspect_ratio_4_3(void) {
    int w = 0, h = 0;
    TEST_ASSERT_EQUAL_INT(0, parse_aspect_ratio("4:3", &w, &h));
}

void test_parse_aspect_ratio_16_9(void) {
    int w = 0, h = 0;
    TEST_ASSERT_EQUAL_INT(1, parse_aspect_ratio("16:9", &w, &h));
}

void test_parse_aspect_ratio_16_10(void) {
    int w = 0, h = 0;
    TEST_ASSERT_EQUAL_INT(2, parse_aspect_ratio("16:10", &w, &h));
}

void test_parse_aspect_ratio_18_9(void) {
    int w = 0, h = 0;
    TEST_ASSERT_EQUAL_INT(3, parse_aspect_ratio("18:9", &w, &h));
}

void test_parse_aspect_ratio_custom(void) {
    int w = 0, h = 0;
    int result = parse_aspect_ratio("25:14", &w, &h);
    TEST_ASSERT_EQUAL_INT(4, result);  /* Custom */
    TEST_ASSERT_EQUAL_INT(25, w);
    TEST_ASSERT_EQUAL_INT(14, h);
}

void test_parse_aspect_ratio_original(void) {
    int w = 0, h = 0;
    TEST_ASSERT_EQUAL_INT(0, parse_aspect_ratio("original", &w, &h));
}

void test_parse_aspect_ratio_with_extend_y(void) {
    int w = 0, h = 0;
    /* "extend_y, 16:9" should return 1 (16:9) */
    TEST_ASSERT_EQUAL_INT(1, parse_aspect_ratio("extend_y, 16:9", &w, &h));
}

/* ===========================================================================
 * Tests for parse_output_method()
 * ===========================================================================*/

void test_parse_output_method_sdl(void) {
    TEST_ASSERT_EQUAL_INT(kOutputMethod_SDL, parse_output_method("SDL"));
}

void test_parse_output_method_sdl_software(void) {
    TEST_ASSERT_EQUAL_INT(kOutputMethod_SDLSoftware, parse_output_method("SDL-Software"));
}

void test_parse_output_method_opengl(void) {
    TEST_ASSERT_EQUAL_INT(kOutputMethod_OpenGL, parse_output_method("OpenGL"));
}

void test_parse_output_method_opengl_es(void) {
    TEST_ASSERT_EQUAL_INT(kOutputMethod_OpenGL_ES, parse_output_method("OpenGL ES"));
}

void test_parse_output_method_vulkan(void) {
    TEST_ASSERT_EQUAL_INT(kOutputMethod_Vulkan, parse_output_method("Vulkan"));
}

void test_parse_output_method_invalid(void) {
    /* Invalid values should default to SDL */
    TEST_ASSERT_EQUAL_INT(kOutputMethod_SDL, parse_output_method("Invalid"));
    TEST_ASSERT_EQUAL_INT(kOutputMethod_SDL, parse_output_method(""));
}

/* ===========================================================================
 * Test runner
 * ===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* trim_whitespace tests */
    RUN_TEST(test_trim_whitespace_leading);
    RUN_TEST(test_trim_whitespace_trailing);
    RUN_TEST(test_trim_whitespace_both);
    RUN_TEST(test_trim_whitespace_empty);
    RUN_TEST(test_trim_whitespace_no_whitespace);
    RUN_TEST(test_trim_whitespace_tabs_and_newlines);

    /* parse_bool tests */
    RUN_TEST(test_parse_bool_one);
    RUN_TEST(test_parse_bool_true_lowercase);
    RUN_TEST(test_parse_bool_true_capitalized);
    RUN_TEST(test_parse_bool_zero);
    RUN_TEST(test_parse_bool_false);
    RUN_TEST(test_parse_bool_invalid);

    /* parse_int tests */
    RUN_TEST(test_parse_int_positive);
    RUN_TEST(test_parse_int_negative);
    RUN_TEST(test_parse_int_zero);
    RUN_TEST(test_parse_int_invalid);

    /* parse_string tests */
    RUN_TEST(test_parse_string_normal);
    RUN_TEST(test_parse_string_null);
    RUN_TEST(test_parse_string_empty);

    /* parse_aspect_ratio tests */
    RUN_TEST(test_parse_aspect_ratio_4_3);
    RUN_TEST(test_parse_aspect_ratio_16_9);
    RUN_TEST(test_parse_aspect_ratio_16_10);
    RUN_TEST(test_parse_aspect_ratio_18_9);
    RUN_TEST(test_parse_aspect_ratio_custom);
    RUN_TEST(test_parse_aspect_ratio_original);
    RUN_TEST(test_parse_aspect_ratio_with_extend_y);

    /* parse_output_method tests */
    RUN_TEST(test_parse_output_method_sdl);
    RUN_TEST(test_parse_output_method_sdl_software);
    RUN_TEST(test_parse_output_method_opengl);
    RUN_TEST(test_parse_output_method_opengl_es);
    RUN_TEST(test_parse_output_method_vulkan);
    RUN_TEST(test_parse_output_method_invalid);

    return UNITY_END();
}
