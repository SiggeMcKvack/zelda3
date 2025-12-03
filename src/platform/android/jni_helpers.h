// jni_helpers.h - JNI helper functions to reduce boilerplate
#ifndef ZELDA3_JNI_HELPERS_H_
#define ZELDA3_JNI_HELPERS_H_

#include <jni.h>
#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// JNI Environment Management
// ============================================================================

// Get cached JavaVM (from SDL)
JavaVM* JniHelper_GetJavaVM(void);

// Get JNIEnv for current thread, attaching if needed
JNIEnv* JniHelper_GetEnv(void);

// ============================================================================
// Generic Static Method Callers
// All target MainActivity class (com/dishii/zelda3/MainActivity)
// ============================================================================

// Call static int method(String) - returns -1 on failure
int JniHelper_CallStaticIntMethod_1S(const char *method_name, const char *arg);

// Call static boolean method(String) - returns false on failure
bool JniHelper_CallStaticBoolMethod_1S(const char *method_name, const char *arg);

// Call static boolean method(String, String) - returns false on failure
bool JniHelper_CallStaticBoolMethod_2S(const char *method_name,
                                        const char *arg1, const char *arg2);

// Call static int method(String, String) - returns -1 on failure
int JniHelper_CallStaticIntMethod_2S(const char *method_name,
                                      const char *arg1, const char *arg2);

// Call static void method(String)
void JniHelper_CallStaticVoidMethod_1S(const char *method_name, const char *arg);

// Call static byte[] method(String) - returns malloc'd buffer, caller must free
void* JniHelper_CallStaticByteArrayMethod_1S(const char *method_name,
                                              const char *arg, int *out_size);

// ============================================================================
// JSON Builder (simple key-value pairs, no external dependencies)
// ============================================================================

typedef struct JniJsonBuilder JniJsonBuilder;

// Create a JSON array or object builder
JniJsonBuilder* JniJson_CreateArray(void);
JniJsonBuilder* JniJson_CreateObject(void);

// Add values to object (key required) or array (key ignored/NULL)
void JniJson_AddString(JniJsonBuilder *b, const char *key, const char *value);
void JniJson_AddInt(JniJsonBuilder *b, const char *key, int value);
void JniJson_AddBool(JniJsonBuilder *b, const char *key, bool value);

// Add nested object to array
void JniJson_AddObject(JniJsonBuilder *array, JniJsonBuilder *object);

// Finalize and return JSON string (caller must free), destroys builder
char* JniJson_Finalize(JniJsonBuilder *b);

// Destroy builder without finalizing
void JniJson_Destroy(JniJsonBuilder *b);

// ============================================================================
// Gamepad Button Names (shared lookup table)
// ============================================================================

// Get button name by index (0-16), returns NULL if out of range
const char* JniHelper_GetButtonName(int button_index);

// Number of gamepad buttons
#define JNI_GAMEPAD_BUTTON_COUNT 17

#endif  // ZELDA3_JNI_HELPERS_H_
