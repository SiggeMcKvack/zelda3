// android_jni.h - JNI interface for Android-specific functionality
#ifndef ANDROID_JNI_H
#define ANDROID_JNI_H

#ifdef __ANDROID__

/**
 * Opens an external file using Android SAF (Storage Access Framework).
 * Called from platform.c for files in user-selected Zelda3 folder.
 *
 * @param path Path like "MSU/track-1.pcm" or "shaders/crt.glsl"
 * @param mode File mode - "r" for read, "w" for write
 * @return File descriptor (>= 0) on success, -1 on failure
 */
int Android_OpenExternalFile(const char *path, const char *mode);

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

/**
 * Shows a Toast notification on Android.
 * Called from main.c when displaying user notifications.
 *
 * @param message The message to display in the Toast
 */
void Android_ShowToast(const char* message);

/**
 * Updates the renderer setting in zelda3.ini.
 * Called from main.c when falling back from Vulkan to OpenGL ES.
 *
 * @param renderer The renderer name ("SDL", "OpenGL ES", "Vulkan", etc.)
 */
void Android_UpdateRendererConfig(const char *renderer);

/**
 * Opens a save file for reading using Android SAF (Storage Access Framework).
 * Called from zelda_rtl.c when loading save files on Android.
 *
 * @param filename Relative filename like "sram.dat" or "save0.sav"
 * @return File descriptor (>= 0) on success, -1 on failure
 */
int Android_OpenSaveFileRead(const char *filename);

/**
 * Opens a save file for writing using Android SAF (Storage Access Framework).
 * Creates the file if it doesn't exist.
 * Called from zelda_rtl.c when saving on Android.
 *
 * @param filename Relative filename like "sram.dat" or "save0.sav"
 * @return File descriptor (>= 0) on success, -1 on failure
 */
int Android_OpenSaveFileWrite(const char *filename);

/**
 * Renames a save file in external storage using Android SAF.
 * Used for creating backup files (sram.dat -> sram.bak).
 *
 * @param old_name Current filename
 * @param new_name New filename
 * @return 1 on success, 0 on failure
 */
int Android_RenameSaveFile(const char *old_name, const char *new_name);

/**
 * Checks if a save file exists in external storage.
 *
 * @param filename Relative filename to check
 * @return 1 if file exists, 0 if not
 */
int Android_SaveFileExists(const char *filename);

/**
 * Deletes a save file in external storage using Android SAF.
 * Used for removing old backup files before renaming.
 *
 * @param filename Filename to delete
 * @return 1 on success, 0 on failure
 */
int Android_DeleteSaveFile(const char *filename);

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
