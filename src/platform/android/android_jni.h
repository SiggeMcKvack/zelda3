// android_jni.h - JNI interface for Android-specific functionality
#ifndef ANDROID_JNI_H
#define ANDROID_JNI_H

#ifdef __ANDROID__

/**
 * Opens an MSU file using Android SAF (Storage Access Framework).
 * Called from audio.c when loading MSU files on Android 13+.
 *
 * @param filename Relative filename like "ALttP-msu-Deluxe-1.pcm" or "alttp_msu-1.opuz"
 * @return File descriptor (>= 0) on success, -1 on failure
 */
int Android_OpenMsuFileDescriptor(const char *filename);

/**
 * Loads an asset file from the APK using Android AssetManager.
 * Called from vulkan.c when loading shader SPIR-V files.
 *
 * @param asset_path Relative path within assets, e.g. "shaders/vert.spv"
 * @param out_size Pointer to store the size of the loaded data
 * @return Pointer to allocated buffer containing asset data, or NULL on failure
 *         Caller must free() the returned buffer.
 */
void* Android_LoadAsset(const char *asset_path, int *out_size);

/**
 * Reads an external shader file using Android Storage Access Framework.
 * Called from glsl_shader.c when loading shader files from external storage.
 *
 * @param filename Full path to shader file
 * @param out_size Pointer to store the size of the file
 * @return Pointer to allocated buffer containing file contents, or NULL on failure
 *         Caller must free() the returned buffer.
 */
char* Android_ReadExternalShaderFile(const char *filename, size_t *out_size);

// Note: The actual JNI function declarations are in android_jni.c
// They are:
// - Java_com_dishii_zelda3_MainActivity_nativeSaveState(JNIEnv* env, jobject obj, jint slot)
// - Java_com_dishii_zelda3_MainActivity_nativeLoadState(JNIEnv* env, jobject obj, jint slot)
// - Java_com_dishii_zelda3_MainActivity_nativeGetScreenshotRGBA(JNIEnv* env, jobject obj)
// - Java_com_dishii_zelda3_MainActivity_nativeTogglePause(JNIEnv* env, jobject obj)
// - Java_com_dishii_zelda3_MainActivity_nativeIsPaused(JNIEnv* env, jobject obj)
// - Java_com_dishii_zelda3_MainActivity_nativeBindGamepadButton(JNIEnv* env, jobject obj, jstring buttonName, jobjectArray modifierNames, jint commandId)
// - Java_com_dishii_zelda3_MainActivity_nativeUnbindGamepadButton(JNIEnv* env, jobject obj, jstring buttonName, jobjectArray modifierNames)
// - Java_com_dishii_zelda3_MainActivity_nativeClearGamepadBindings(JNIEnv* env, jobject obj)
// - Java_com_dishii_zelda3_MainActivity_nativeGetGamepadBindings(JNIEnv* env, jobject obj)
// - Java_com_dishii_zelda3_MainActivity_nativeApplyDefaultGamepadBindings(JNIEnv* env, jobject obj)
// - Java_com_dishii_zelda3_MainActivity_nativeGetButtonForCommand(JNIEnv* env, jobject obj, jint cmdId)

#endif // __ANDROID__

#endif // ANDROID_JNI_H
