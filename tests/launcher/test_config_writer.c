/*
 * Unit tests for src/launcher/config_writer.c
 */

#include "unity.h"
#include "test_utils.h"

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
 * Tests for ConfigWriter_InitDefaults()
 * ===========================================================================*/

void test_init_defaults_general(void) {
    Config config;
    ConfigWriter_InitDefaults(&config);

    TEST_ASSERT_FALSE(config.autosave);
    TEST_ASSERT_FALSE(config.display_perf_title);
    TEST_ASSERT_FALSE(config.disable_frame_delay);
}

void test_init_defaults_graphics(void) {
    Config config;
    ConfigWriter_InitDefaults(&config);

    TEST_ASSERT_EQUAL_INT(0, config.window_width);
    TEST_ASSERT_EQUAL_INT(0, config.window_height);
    TEST_ASSERT_EQUAL_INT(3, config.window_scale);
    TEST_ASSERT_EQUAL_INT(0, config.fullscreen);
    TEST_ASSERT_FALSE(config.ignore_aspect_ratio);
    TEST_ASSERT_EQUAL_INT(kOutputMethod_SDL, config.output_method);
    TEST_ASSERT_FALSE(config.linear_filtering);
    TEST_ASSERT_TRUE(config.new_renderer);
    TEST_ASSERT_TRUE(config.enhanced_mode7);
    TEST_ASSERT_TRUE(config.no_sprite_limits);
    TEST_ASSERT_EQUAL_INT(1, config.extended_aspect_ratio);  /* 16:9 */
    TEST_ASSERT_TRUE(config.extend_y);
}

void test_init_defaults_sound(void) {
    Config config;
    ConfigWriter_InitDefaults(&config);

    TEST_ASSERT_TRUE(config.enable_audio);
    TEST_ASSERT_EQUAL_INT(44100, config.audio_freq);
    TEST_ASSERT_EQUAL_INT(2, config.audio_channels);
    TEST_ASSERT_EQUAL_INT(512, config.audio_samples);
    TEST_ASSERT_EQUAL_INT(0, config.enable_msu);
    TEST_ASSERT_TRUE(config.resume_msu);
    TEST_ASSERT_EQUAL_INT(100, config.msuvolume);
}

void test_init_defaults_features(void) {
    Config config;
    ConfigWriter_InitDefaults(&config);

    TEST_ASSERT_EQUAL_UINT32(0, config.features0);
}

void test_init_defaults_paths(void) {
    Config config;
    ConfigWriter_InitDefaults(&config);

    TEST_ASSERT_NULL(config.link_graphics);
    TEST_ASSERT_NULL(config.shader);
    TEST_ASSERT_NULL(config.msu_path);
    TEST_ASSERT_NULL(config.language);
}

/* ===========================================================================
 * Tests for ConfigWriter_Validate()
 * ===========================================================================*/

void test_validate_valid_config(void) {
    Config config;
    char error[256];
    ConfigWriter_InitDefaults(&config);

    TEST_ASSERT_TRUE(ConfigWriter_Validate(&config, error, sizeof(error)));
}

void test_validate_null_config(void) {
    char error[256];
    TEST_ASSERT_FALSE(ConfigWriter_Validate(NULL, error, sizeof(error)));
    TEST_ASSERT_STRING_CONTAINS(error, "NULL");
}

void test_validate_invalid_audio_freq(void) {
    Config config;
    char error[256];
    ConfigWriter_InitDefaults(&config);

    config.audio_freq = 12345;  /* Invalid frequency */
    TEST_ASSERT_FALSE(ConfigWriter_Validate(&config, error, sizeof(error)));
    TEST_ASSERT_STRING_CONTAINS(error, "audio frequency");
}

void test_validate_valid_audio_freqs(void) {
    Config config;
    char error[256];

    int valid_freqs[] = {11025, 22050, 32000, 44100, 48000};
    for (int i = 0; i < 5; i++) {
        ConfigWriter_InitDefaults(&config);
        config.audio_freq = valid_freqs[i];
        TEST_ASSERT_TRUE_MESSAGE(ConfigWriter_Validate(&config, error, sizeof(error)),
                                  "Valid frequency should pass");
    }
}

void test_validate_invalid_audio_channels(void) {
    Config config;
    char error[256];
    ConfigWriter_InitDefaults(&config);

    config.audio_channels = 0;
    TEST_ASSERT_FALSE(ConfigWriter_Validate(&config, error, sizeof(error)));
    TEST_ASSERT_STRING_CONTAINS(error, "channels");

    config.audio_channels = 3;
    TEST_ASSERT_FALSE(ConfigWriter_Validate(&config, error, sizeof(error)));
}

void test_validate_invalid_audio_samples(void) {
    Config config;
    char error[256];
    ConfigWriter_InitDefaults(&config);

    /* Not a power of 2 */
    config.audio_samples = 300;
    TEST_ASSERT_FALSE(ConfigWriter_Validate(&config, error, sizeof(error)));
    TEST_ASSERT_STRING_CONTAINS(error, "samples");

    /* Too small */
    config.audio_samples = 64;
    TEST_ASSERT_FALSE(ConfigWriter_Validate(&config, error, sizeof(error)));

    /* Too large */
    config.audio_samples = 8192;
    TEST_ASSERT_FALSE(ConfigWriter_Validate(&config, error, sizeof(error)));
}

void test_validate_valid_audio_samples(void) {
    Config config;
    char error[256];

    int valid_samples[] = {128, 256, 512, 1024, 2048, 4096};
    for (int i = 0; i < 6; i++) {
        ConfigWriter_InitDefaults(&config);
        config.audio_samples = valid_samples[i];
        TEST_ASSERT_TRUE_MESSAGE(ConfigWriter_Validate(&config, error, sizeof(error)),
                                  "Valid sample size should pass");
    }
}

void test_validate_invalid_fullscreen(void) {
    Config config;
    char error[256];
    ConfigWriter_InitDefaults(&config);

    config.fullscreen = 3;  /* Only 0, 1, 2 are valid */
    TEST_ASSERT_FALSE(ConfigWriter_Validate(&config, error, sizeof(error)));
    TEST_ASSERT_STRING_CONTAINS(error, "fullscreen");
}

void test_validate_invalid_output_method(void) {
    Config config;
    char error[256];
    ConfigWriter_InitDefaults(&config);

    config.output_method = 99;
    TEST_ASSERT_FALSE(ConfigWriter_Validate(&config, error, sizeof(error)));
    TEST_ASSERT_STRING_CONTAINS(error, "output method");
}

void test_validate_invalid_aspect_ratio(void) {
    Config config;
    char error[256];
    ConfigWriter_InitDefaults(&config);

    config.extended_aspect_ratio = 10;
    TEST_ASSERT_FALSE(ConfigWriter_Validate(&config, error, sizeof(error)));
    TEST_ASSERT_STRING_CONTAINS(error, "aspect ratio");
}

void test_validate_custom_aspect_ratio_missing_dimensions(void) {
    Config config;
    char error[256];
    ConfigWriter_InitDefaults(&config);

    config.extended_aspect_ratio = 4;  /* Custom */
    config.custom_aspect_w = 0;
    config.custom_aspect_h = 0;
    TEST_ASSERT_FALSE(ConfigWriter_Validate(&config, error, sizeof(error)));
    TEST_ASSERT_STRING_CONTAINS(error, "width and height");
}

void test_validate_custom_aspect_ratio_valid(void) {
    Config config;
    char error[256];
    ConfigWriter_InitDefaults(&config);

    config.extended_aspect_ratio = 4;  /* Custom */
    config.custom_aspect_w = 25;
    config.custom_aspect_h = 14;
    TEST_ASSERT_TRUE(ConfigWriter_Validate(&config, error, sizeof(error)));
}

void test_validate_invalid_msu_volume(void) {
    Config config;
    char error[256];
    ConfigWriter_InitDefaults(&config);

    config.msuvolume = 150;  /* Max is 100 */
    TEST_ASSERT_FALSE(ConfigWriter_Validate(&config, error, sizeof(error)));
    TEST_ASSERT_STRING_CONTAINS(error, "volume");
}

/* ===========================================================================
 * Test runner
 * ===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* ConfigWriter_InitDefaults tests */
    RUN_TEST(test_init_defaults_general);
    RUN_TEST(test_init_defaults_graphics);
    RUN_TEST(test_init_defaults_sound);
    RUN_TEST(test_init_defaults_features);
    RUN_TEST(test_init_defaults_paths);

    /* ConfigWriter_Validate tests */
    RUN_TEST(test_validate_valid_config);
    RUN_TEST(test_validate_null_config);
    RUN_TEST(test_validate_invalid_audio_freq);
    RUN_TEST(test_validate_valid_audio_freqs);
    RUN_TEST(test_validate_invalid_audio_channels);
    RUN_TEST(test_validate_invalid_audio_samples);
    RUN_TEST(test_validate_valid_audio_samples);
    RUN_TEST(test_validate_invalid_fullscreen);
    RUN_TEST(test_validate_invalid_output_method);
    RUN_TEST(test_validate_invalid_aspect_ratio);
    RUN_TEST(test_validate_custom_aspect_ratio_missing_dimensions);
    RUN_TEST(test_validate_custom_aspect_ratio_valid);
    RUN_TEST(test_validate_invalid_msu_volume);

    return UNITY_END();
}
