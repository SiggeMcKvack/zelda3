// Round-trip test for the launcher_core FFI layer.
//
// Proves the JSON marshalling is lossless by comparing two ways of producing a
// canonical zelda3.ini from the same input:
//   direct path:  ConfigReader_Read -> ConfigWriter_Write
//   FFI path:     LauncherFfi_LoadConfigJson -> LauncherFfi_SaveConfigJson
// The two outputs must be byte-for-byte identical (replay compatibility).
#include "unity.h"
#include "launcher_ffi.h"
#include "config_reader.h"
#include "config_writer.h"
#include "../config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ZELDA3_INI_PATH
#error "ZELDA3_INI_PATH must be defined by the build"
#endif

static const char *kInputIni = ZELDA3_INI_PATH;
static const char *kRefIni = "ffi_ref.ini";   // direct C path output
static const char *kOutIni = "ffi_out.ini";   // FFI JSON path output

void setUp(void) {}
void tearDown(void) {}

// Read an entire file into a malloc'd buffer (NUL-terminated). NULL on failure.
static char *read_file(const char *path, size_t *len_out) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    if (len_out) *len_out = got;
    return buf;
}

// Report the first line where two buffers differ (for debugging failures).
static void report_first_diff(const char *a, const char *b) {
    int line = 1;
    const char *pa = a, *pb = b;
    while (*pa && *pb && *pa == *pb) {
        if (*pa == '\n') line++;
        pa++; pb++;
    }
    if (*pa != *pb)
        printf("  first difference at line %d: ref=%.40s... out=%.40s...\n", line, pa, pb);
}

static void test_ffi_roundtrip_matches_direct(void) {
    // Direct C path: read input INI, write canonical reference.
    Config cfg;
    TEST_ASSERT_TRUE_MESSAGE(ConfigReader_Read(kInputIni, &cfg),
                             "ConfigReader_Read failed on input INI");
    TEST_ASSERT_TRUE_MESSAGE(ConfigWriter_Write(kRefIni, &cfg),
                             "ConfigWriter_Write failed");

    // FFI JSON path: load -> JSON -> save canonical output.
    char *json = LauncherFfi_LoadConfigJson(kInputIni);
    TEST_ASSERT_NOT_NULL_MESSAGE(json, "LauncherFfi_LoadConfigJson returned NULL");

    char *save_result = LauncherFfi_SaveConfigJson(kOutIni, json);
    TEST_ASSERT_NOT_NULL(save_result);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(save_result, "\"ok\":true"),
                                 "LauncherFfi_SaveConfigJson did not report ok");

    // Compare the two canonical INIs byte-for-byte.
    size_t ref_len = 0, out_len = 0;
    char *ref = read_file(kRefIni, &ref_len);
    char *out = read_file(kOutIni, &out_len);
    TEST_ASSERT_NOT_NULL(ref);
    TEST_ASSERT_NOT_NULL(out);

    if (ref_len != out_len || memcmp(ref, out, ref_len) != 0)
        report_first_diff(ref, out);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(ref_len, out_len, "INI lengths differ");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, memcmp(ref, out, ref_len),
                                  "FFI INI differs from direct INI");

    free(ref);
    free(out);
    LauncherFfi_FreeString(json);
    LauncherFfi_FreeString(save_result);
}

// Saving the JSON twice must be idempotent (no drift through load/save cycles).
static void test_ffi_save_is_idempotent(void) {
    char *json1 = LauncherFfi_LoadConfigJson(kOutIni);
    TEST_ASSERT_NOT_NULL(json1);
    char *res = LauncherFfi_SaveConfigJson("ffi_out2.ini", json1);
    LauncherFfi_FreeString(res);

    size_t a = 0, b = 0;
    char *first = read_file(kOutIni, &a);
    char *second = read_file("ffi_out2.ini", &b);
    TEST_ASSERT_EQUAL_size_t(a, b);
    TEST_ASSERT_EQUAL_INT(0, memcmp(first, second, a));

    free(first);
    free(second);
    LauncherFfi_FreeString(json1);
}

// The restool wrappers must return well-formed JSON and not crash on bad input
// (a full ROM-based test needs a copyrighted ROM, so isn't run here).
static void test_restool_ffi_shapes(void) {
    // Identifying this source file (not a ROM) must not crash and report invalid.
    char *id = LauncherFfi_IdentifyRom(kInputIni, NULL);
    TEST_ASSERT_NOT_NULL(id);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(id, "\"valid\":false"), "non-ROM reported valid");
    LauncherFfi_FreeString(id);

    // NULL path is handled.
    char *id_null = LauncherFfi_IdentifyRom(NULL, NULL);
    TEST_ASSERT_NOT_NULL(id_null);
    TEST_ASSERT_NOT_NULL(strstr(id_null, "\"read\":false"));
    LauncherFfi_FreeString(id_null);

    // An .ips without a usable base ROM is read but not valid.
    char *id_ips = LauncherFfi_IdentifyRom("missing.ips", NULL);
    TEST_ASSERT_NOT_NULL(id_ips);
    TEST_ASSERT_NOT_NULL(strstr(id_ips, "\"valid\":false"));
    LauncherFfi_FreeString(id_ips);

    // No DAT in /tmp -> exists:false with an (empty) languages array.
    char *st = LauncherFfi_DatStatus("/tmp");
    TEST_ASSERT_NOT_NULL(st);
    TEST_ASSERT_NOT_NULL(strstr(st, "\"exists\":false"));
    TEST_ASSERT_NOT_NULL(strstr(st, "\"languages\":[]"));
    LauncherFfi_FreeString(st);

    // CreateDat with a bogus base ROM fails cleanly (no crash, ok:false).
    char *cd = LauncherFfi_CreateDat("{\"us_rom_path\":\"/no/such.sfc\",\"output_dir\":\"/tmp\"}");
    TEST_ASSERT_NOT_NULL(cd);
    TEST_ASSERT_NOT_NULL(strstr(cd, "\"ok\":false"));
    LauncherFfi_FreeString(cd);
}

// Opus wrapper: must not crash and report a clean error when there's nothing to
// encode (a real encode test needs MSU .pcm files, so isn't run here).
static void test_opus_ffi_shape(void) {
    char *r = LauncherFfi_EncodeMsuToOpus("/tmp", "zz_no_such_prefix_", NULL, NULL);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(r, "\"ok\":false"), "expected ok:false for empty input");
    LauncherFfi_FreeString(r);

    char *r2 = LauncherFfi_EncodeMsuToOpus(NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(r2);
    TEST_ASSERT_NOT_NULL(strstr(r2, "\"ok\":false"));
    LauncherFfi_FreeString(r2);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ffi_roundtrip_matches_direct);
    RUN_TEST(test_ffi_save_is_idempotent);
    RUN_TEST(test_restool_ffi_shapes);
    RUN_TEST(test_opus_ffi_shape);
    return UNITY_END();
}
