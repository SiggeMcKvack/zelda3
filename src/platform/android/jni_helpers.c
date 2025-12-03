// jni_helpers.c - JNI helper functions implementation
#include "jni_helpers.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <android/log.h>
#include <SDL.h>

#define LOG_TAG "Zelda3JNI"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// MainActivity class name
#define MAIN_ACTIVITY_CLASS "com/dishii/zelda3/MainActivity"

// ============================================================================
// Shared Gamepad Button Names
// ============================================================================

static const char *const kGamepadButtonNames[JNI_GAMEPAD_BUTTON_COUNT] = {
    "A", "B", "X", "Y", "Back", "Guide", "Start", "L3", "R3",
    "L1", "R1", "DpadUp", "DpadDown", "DpadLeft", "DpadRight", "L2", "R2"
};

const char* JniHelper_GetButtonName(int button_index) {
    if (button_index < 0 || button_index >= JNI_GAMEPAD_BUTTON_COUNT)
        return NULL;
    return kGamepadButtonNames[button_index];
}

// ============================================================================
// JNI Environment Management
// ============================================================================

JavaVM* JniHelper_GetJavaVM(void) {
    // SDL caches the JavaVM in SDL_android.c as a static variable
    // We can access it via SDL_AndroidGetJNIEnv which uses it internally
    JNIEnv *env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if (!env) {
        return NULL;
    }
    JavaVM *vm = NULL;
    (*env)->GetJavaVM(env, &vm);
    return vm;
}

JNIEnv* JniHelper_GetEnv(void) {
    JavaVM *vm = JniHelper_GetJavaVM();
    if (!vm) return NULL;

    JNIEnv *env = NULL;
    int getEnvStat = (*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6);

    if (getEnvStat == JNI_EDETACHED) {
        if ((*vm)->AttachCurrentThread(vm, &env, NULL) != 0) {
            return NULL;
        }
    } else if (getEnvStat != JNI_OK) {
        return NULL;
    }
    return env;
}

// ============================================================================
// Generic Static Method Callers
// ============================================================================

int JniHelper_CallStaticIntMethod_1S(const char *method_name, const char *arg) {
    JNIEnv *env = JniHelper_GetEnv();
    if (!env) {
        LOGD("%s: Failed to get JNIEnv", method_name);
        return -1;
    }

    jclass cls = (*env)->FindClass(env, MAIN_ACTIVITY_CLASS);
    if (!cls) {
        LOGD("%s: Failed to find MainActivity class", method_name);
        return -1;
    }

    jmethodID method = (*env)->GetStaticMethodID(env, cls, method_name, "(Ljava/lang/String;)I");
    if (!method) {
        LOGD("%s: Failed to find method", method_name);
        (*env)->DeleteLocalRef(env, cls);
        return -1;
    }

    jstring jarg = (*env)->NewStringUTF(env, arg);
    if (!jarg) {
        LOGD("%s: Failed to create Java string", method_name);
        (*env)->DeleteLocalRef(env, cls);
        return -1;
    }

    jint result = (*env)->CallStaticIntMethod(env, cls, method, jarg);

    (*env)->DeleteLocalRef(env, jarg);
    (*env)->DeleteLocalRef(env, cls);

    LOGD("%s: arg='%s', result=%d", method_name, arg, result);
    return (int)result;
}

bool JniHelper_CallStaticBoolMethod_1S(const char *method_name, const char *arg) {
    JNIEnv *env = JniHelper_GetEnv();
    if (!env) {
        LOGD("%s: Failed to get JNIEnv", method_name);
        return false;
    }

    jclass cls = (*env)->FindClass(env, MAIN_ACTIVITY_CLASS);
    if (!cls) {
        LOGD("%s: Failed to find MainActivity class", method_name);
        return false;
    }

    jmethodID method = (*env)->GetStaticMethodID(env, cls, method_name, "(Ljava/lang/String;)Z");
    if (!method) {
        LOGD("%s: Failed to find method", method_name);
        (*env)->DeleteLocalRef(env, cls);
        return false;
    }

    jstring jarg = (*env)->NewStringUTF(env, arg);
    if (!jarg) {
        LOGD("%s: Failed to create Java string", method_name);
        (*env)->DeleteLocalRef(env, cls);
        return false;
    }

    jboolean result = (*env)->CallStaticBooleanMethod(env, cls, method, jarg);

    (*env)->DeleteLocalRef(env, jarg);
    (*env)->DeleteLocalRef(env, cls);

    LOGD("%s: arg='%s', result=%d", method_name, arg, result);
    return result ? true : false;
}

bool JniHelper_CallStaticBoolMethod_2S(const char *method_name,
                                        const char *arg1, const char *arg2) {
    JNIEnv *env = JniHelper_GetEnv();
    if (!env) {
        LOGD("%s: Failed to get JNIEnv", method_name);
        return false;
    }

    jclass cls = (*env)->FindClass(env, MAIN_ACTIVITY_CLASS);
    if (!cls) {
        LOGD("%s: Failed to find MainActivity class", method_name);
        return false;
    }

    jmethodID method = (*env)->GetStaticMethodID(env, cls, method_name,
                                                  "(Ljava/lang/String;Ljava/lang/String;)Z");
    if (!method) {
        LOGD("%s: Failed to find method", method_name);
        (*env)->DeleteLocalRef(env, cls);
        return false;
    }

    jstring jarg1 = (*env)->NewStringUTF(env, arg1);
    jstring jarg2 = (*env)->NewStringUTF(env, arg2);
    if (!jarg1 || !jarg2) {
        LOGD("%s: Failed to create Java strings", method_name);
        if (jarg1) (*env)->DeleteLocalRef(env, jarg1);
        if (jarg2) (*env)->DeleteLocalRef(env, jarg2);
        (*env)->DeleteLocalRef(env, cls);
        return false;
    }

    jboolean result = (*env)->CallStaticBooleanMethod(env, cls, method, jarg1, jarg2);

    (*env)->DeleteLocalRef(env, jarg1);
    (*env)->DeleteLocalRef(env, jarg2);
    (*env)->DeleteLocalRef(env, cls);

    LOGD("%s: arg1='%s', arg2='%s', result=%d", method_name, arg1, arg2, result);
    return result ? true : false;
}

int JniHelper_CallStaticIntMethod_2S(const char *method_name,
                                      const char *arg1, const char *arg2) {
    JNIEnv *env = JniHelper_GetEnv();
    if (!env) {
        LOGD("%s: Failed to get JNIEnv", method_name);
        return -1;
    }

    jclass cls = (*env)->FindClass(env, MAIN_ACTIVITY_CLASS);
    if (!cls) {
        LOGD("%s: Failed to find MainActivity class", method_name);
        return -1;
    }

    jmethodID method = (*env)->GetStaticMethodID(env, cls, method_name,
                                                  "(Ljava/lang/String;Ljava/lang/String;)I");
    if (!method) {
        LOGD("%s: Failed to find method", method_name);
        (*env)->DeleteLocalRef(env, cls);
        return -1;
    }

    jstring jarg1 = (*env)->NewStringUTF(env, arg1);
    jstring jarg2 = (*env)->NewStringUTF(env, arg2);
    if (!jarg1 || !jarg2) {
        LOGD("%s: Failed to create Java strings", method_name);
        if (jarg1) (*env)->DeleteLocalRef(env, jarg1);
        if (jarg2) (*env)->DeleteLocalRef(env, jarg2);
        (*env)->DeleteLocalRef(env, cls);
        return -1;
    }

    jint result = (*env)->CallStaticIntMethod(env, cls, method, jarg1, jarg2);

    (*env)->DeleteLocalRef(env, jarg1);
    (*env)->DeleteLocalRef(env, jarg2);
    (*env)->DeleteLocalRef(env, cls);

    LOGD("%s: arg1='%s', arg2='%s', result=%d", method_name, arg1, arg2, result);
    return (int)result;
}

void JniHelper_CallStaticVoidMethod_1S(const char *method_name, const char *arg) {
    JNIEnv *env = JniHelper_GetEnv();
    if (!env) {
        LOGD("%s: Failed to get JNIEnv", method_name);
        return;
    }

    jclass cls = (*env)->FindClass(env, MAIN_ACTIVITY_CLASS);
    if (!cls) {
        LOGD("%s: Failed to find MainActivity class", method_name);
        return;
    }

    jmethodID method = (*env)->GetStaticMethodID(env, cls, method_name, "(Ljava/lang/String;)V");
    if (!method) {
        LOGD("%s: Failed to find method", method_name);
        (*env)->DeleteLocalRef(env, cls);
        return;
    }

    jstring jarg = (*env)->NewStringUTF(env, arg);
    if (!jarg) {
        LOGD("%s: Failed to create Java string", method_name);
        (*env)->DeleteLocalRef(env, cls);
        return;
    }

    (*env)->CallStaticVoidMethod(env, cls, method, jarg);

    (*env)->DeleteLocalRef(env, jarg);
    (*env)->DeleteLocalRef(env, cls);

    LOGD("%s: arg='%s' completed", method_name, arg);
}

void* JniHelper_CallStaticByteArrayMethod_1S(const char *method_name,
                                              const char *arg, int *out_size) {
    JNIEnv *env = JniHelper_GetEnv();
    if (!env) {
        LOGD("%s: Failed to get JNIEnv", method_name);
        return NULL;
    }

    jclass cls = (*env)->FindClass(env, MAIN_ACTIVITY_CLASS);
    if (!cls) {
        LOGD("%s: Failed to find MainActivity class", method_name);
        return NULL;
    }

    jmethodID method = (*env)->GetStaticMethodID(env, cls, method_name,
                                                  "(Ljava/lang/String;)[B");
    if (!method) {
        LOGD("%s: Failed to find method", method_name);
        (*env)->DeleteLocalRef(env, cls);
        return NULL;
    }

    jstring jarg = (*env)->NewStringUTF(env, arg);
    if (!jarg) {
        LOGD("%s: Failed to create Java string", method_name);
        (*env)->DeleteLocalRef(env, cls);
        return NULL;
    }

    jbyteArray jdata = (jbyteArray)(*env)->CallStaticObjectMethod(env, cls, method, jarg);

    (*env)->DeleteLocalRef(env, jarg);
    (*env)->DeleteLocalRef(env, cls);

    if (!jdata) {
        LOGD("%s: Method returned null for '%s'", method_name, arg);
        return NULL;
    }

    jsize length = (*env)->GetArrayLength(env, jdata);
    if (length <= 0) {
        LOGD("%s: Empty or invalid array for '%s'", method_name, arg);
        (*env)->DeleteLocalRef(env, jdata);
        return NULL;
    }

    void *buffer = malloc(length);
    if (!buffer) {
        LOGD("%s: malloc failed for %d bytes", method_name, length);
        (*env)->DeleteLocalRef(env, jdata);
        return NULL;
    }

    jbyte *data = (*env)->GetByteArrayElements(env, jdata, NULL);
    if (!data) {
        LOGD("%s: GetByteArrayElements failed", method_name);
        free(buffer);
        (*env)->DeleteLocalRef(env, jdata);
        return NULL;
    }

    memcpy(buffer, data, length);
    (*env)->ReleaseByteArrayElements(env, jdata, data, JNI_ABORT);
    (*env)->DeleteLocalRef(env, jdata);

    if (out_size) {
        *out_size = (int)length;
    }

    LOGD("%s: Loaded '%s' (%d bytes)", method_name, arg, length);
    return buffer;
}

// ============================================================================
// JSON Builder Implementation
// ============================================================================

#define JSON_INITIAL_CAPACITY 256

struct JniJsonBuilder {
    char *buffer;
    size_t length;
    size_t capacity;
    int count;       // Number of elements added
    bool is_array;   // true = array, false = object
};

static void JniJson_EnsureCapacity(JniJsonBuilder *b, size_t additional) {
    size_t needed = b->length + additional + 1;  // +1 for null terminator
    if (needed > b->capacity) {
        size_t new_capacity = b->capacity * 2;
        if (new_capacity < needed) new_capacity = needed;
        b->buffer = realloc(b->buffer, new_capacity);
        b->capacity = new_capacity;
    }
}

static void JniJson_Append(JniJsonBuilder *b, const char *str) {
    size_t len = strlen(str);
    JniJson_EnsureCapacity(b, len);
    memcpy(b->buffer + b->length, str, len);
    b->length += len;
    b->buffer[b->length] = '\0';
}

static void JniJson_AppendEscaped(JniJsonBuilder *b, const char *str) {
    // Append a JSON-escaped string (without surrounding quotes)
    for (const char *p = str; *p; p++) {
        char c = *p;
        switch (c) {
            case '"':  JniJson_Append(b, "\\\""); break;
            case '\\': JniJson_Append(b, "\\\\"); break;
            case '\b': JniJson_Append(b, "\\b"); break;
            case '\f': JniJson_Append(b, "\\f"); break;
            case '\n': JniJson_Append(b, "\\n"); break;
            case '\r': JniJson_Append(b, "\\r"); break;
            case '\t': JniJson_Append(b, "\\t"); break;
            default: {
                JniJson_EnsureCapacity(b, 1);
                b->buffer[b->length++] = c;
                b->buffer[b->length] = '\0';
            }
        }
    }
}

JniJsonBuilder* JniJson_CreateArray(void) {
    JniJsonBuilder *b = malloc(sizeof(JniJsonBuilder));
    if (!b) return NULL;
    b->buffer = malloc(JSON_INITIAL_CAPACITY);
    if (!b->buffer) { free(b); return NULL; }
    b->capacity = JSON_INITIAL_CAPACITY;
    b->length = 0;
    b->count = 0;
    b->is_array = true;
    b->buffer[0] = '\0';
    return b;
}

JniJsonBuilder* JniJson_CreateObject(void) {
    JniJsonBuilder *b = JniJson_CreateArray();
    if (b) b->is_array = false;
    return b;
}

void JniJson_AddString(JniJsonBuilder *b, const char *key, const char *value) {
    if (!b || !value) return;

    // Add comma separator if not first element
    if (b->count > 0) {
        JniJson_Append(b, ",");
    }

    // For objects, add key
    if (!b->is_array && key) {
        JniJson_Append(b, "\"");
        JniJson_AppendEscaped(b, key);
        JniJson_Append(b, "\":");
    }

    // Add quoted value
    JniJson_Append(b, "\"");
    JniJson_AppendEscaped(b, value);
    JniJson_Append(b, "\"");

    b->count++;
}

void JniJson_AddInt(JniJsonBuilder *b, const char *key, int value) {
    if (!b) return;

    if (b->count > 0) {
        JniJson_Append(b, ",");
    }

    if (!b->is_array && key) {
        JniJson_Append(b, "\"");
        JniJson_AppendEscaped(b, key);
        JniJson_Append(b, "\":");
    }

    char num[32];
    snprintf(num, sizeof(num), "%d", value);
    JniJson_Append(b, num);

    b->count++;
}

void JniJson_AddBool(JniJsonBuilder *b, const char *key, bool value) {
    if (!b) return;

    if (b->count > 0) {
        JniJson_Append(b, ",");
    }

    if (!b->is_array && key) {
        JniJson_Append(b, "\"");
        JniJson_AppendEscaped(b, key);
        JniJson_Append(b, "\":");
    }

    JniJson_Append(b, value ? "true" : "false");

    b->count++;
}

void JniJson_AddObject(JniJsonBuilder *array, JniJsonBuilder *object) {
    if (!array || !object || !array->is_array) return;

    // Finalize the object first (without destroying it)
    char *obj_str = JniJson_Finalize(object);
    if (!obj_str) return;

    if (array->count > 0) {
        JniJson_Append(array, ",");
    }

    JniJson_Append(array, obj_str);
    array->count++;

    free(obj_str);
}

char* JniJson_Finalize(JniJsonBuilder *b) {
    if (!b) return NULL;

    // Calculate final size: opening bracket + content + closing bracket
    size_t final_size = b->length + 3;  // [ or { + content + ] or } + null
    char *result = malloc(final_size);
    if (!result) {
        JniJson_Destroy(b);
        return NULL;
    }

    result[0] = b->is_array ? '[' : '{';
    if (b->length > 0) {
        memcpy(result + 1, b->buffer, b->length);
    }
    result[b->length + 1] = b->is_array ? ']' : '}';
    result[b->length + 2] = '\0';

    JniJson_Destroy(b);
    return result;
}

void JniJson_Destroy(JniJsonBuilder *b) {
    if (b) {
        free(b->buffer);
        free(b);
    }
}
