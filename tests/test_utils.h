#ifndef TEST_UTILS_H
#define TEST_UTILS_H

/*
 * Common test utilities for Zelda3 unit tests
 * Include this header in test files for shared macros and helpers.
 */

/* Platform-specific includes FIRST to avoid redefinition issues */
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define mkstemp(template) _mktemp_s(template, strlen(template) + 1)
#define test_write _write
#define test_close _close
#else
#include <unistd.h>
#define test_write write
#define test_close close
#endif

#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===========================================================================
 * Temporary file helpers
 * ===========================================================================*/

/* Create a temporary file with given content, returns path that must be freed */
static inline char* test_create_temp_file(const char *content) {
    char template_path[] = "/tmp/zelda3_test_XXXXXX";
    int fd = mkstemp(template_path);
    if (fd == -1) return NULL;

    if (content && *content) {
        test_write(fd, content, strlen(content));
    }
    test_close(fd);

    return strdup(template_path);
}

/* Remove temporary file created by test_create_temp_file */
static inline void test_remove_temp_file(char *path) {
    if (path) {
        remove(path);
        free(path);
    }
}

/* Read entire file content, returns malloc'd string or NULL on error */
static inline char* test_read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = (char*)malloc(size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }

    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);

    return content;
}

/* ===========================================================================
 * String comparison helpers
 * ===========================================================================*/

/* Assert that a string contains a substring */
#define TEST_ASSERT_STRING_CONTAINS(str, substr) \
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(str, substr), "String should contain: " substr)

/* Assert string does not contain substring */
#define TEST_ASSERT_STRING_NOT_CONTAINS(str, substr) \
    TEST_ASSERT_NULL_MESSAGE(strstr(str, substr), "String should not contain: " substr)

/* ===========================================================================
 * Memory comparison helpers
 * ===========================================================================*/

/* Zero-initialize a struct */
#define TEST_ZERO_STRUCT(s) memset(&(s), 0, sizeof(s))

#endif /* TEST_UTILS_H */
