// android_jni.c - JNI interface for Android hot-reload functionality
#include <jni.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>
#include <SDL.h>
#include "config.h"
#include "features.h"
#include "zelda_rtl.h"
#include "snes/ppu.h"

#define LOG_TAG "Zelda3JNI"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// Android-specific global state (desktop has these in main.c)
uint8_t g_paused = 0;

// External function declarations
void ParseConfigFile(const char *filename);
void ZeldaEnableMsu(uint8_t enable);
void ZeldaApuLock(void);
void ZeldaApuUnlock(void);
void SaveLoadSlot(int cmd, int which);

// JNI function to reload audio config without restarting the app
JNIEXPORT void JNICALL Java_com_dishii_zelda3_MainActivity_nativeReloadAudioConfig(
    JNIEnv* env, jobject obj, jint enable_msu, jint msu_vol, jint disable_low_health_beep) {

    LOGD("nativeReloadAudioConfig called with: enable_msu=%d, msu_volume=%d, disable_beep=%d",
         enable_msu, msu_vol, disable_low_health_beep);

    // Save old values
    uint8_t old_enable_msu = g_config.enable_msu;
    uint8_t old_msuvolume = g_config.msuvolume;
    uint32_t old_features0 = g_config.features0;

    LOGD("Old values: enable_msu=%u, msuvolume=%u, features0=%u", old_enable_msu, old_msuvolume, old_features0);

    // Update config directly (file will be updated by Kotlin side)
    g_config.enable_msu = (uint8_t)enable_msu;
    g_config.msuvolume = (uint8_t)msu_vol;

    // Update DisableLowHealthBeep feature flag
    if (disable_low_health_beep) {
        g_config.features0 |= kFeatures0_DisableLowHealthBeep;  // Set bit
    } else {
        g_config.features0 &= ~kFeatures0_DisableLowHealthBeep;  // Clear bit
    }
    g_wanted_zelda_features = g_config.features0;

    LOGD("New values: enable_msu=%u, msuvolume=%u, features0=%u",
         g_config.enable_msu, g_config.msuvolume, g_config.features0);

    // Apply MSU settings change
    if (old_enable_msu != g_config.enable_msu || old_msuvolume != g_config.msuvolume) {
        LOGD("MSU settings changed, calling ZeldaEnableMsu(%u)", g_config.enable_msu);
        ZeldaApuLock();
        ZeldaEnableMsu(g_config.enable_msu);
        ZeldaApuUnlock();
        LOGD("ZeldaEnableMsu completed");
    } else {
        LOGD("MSU settings unchanged, skipping ZeldaEnableMsu");
    }

    LOGD("Hot-reload complete");
}

/**
 * Get the JavaVM pointer from SDL's cached copy.
 * SDL's JNI_OnLoad stores this when libSDL2.so loads.
 */
static JavaVM* GetJavaVM() {
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

/**
 * Opens an MSU file using Android SAF (Storage Access Framework).
 * Called from audio.c when loading MSU files on Android 13+.
 *
 * @param filename Relative filename like "ALttP-msu-Deluxe-1.pcm" or "alttp_msu-1.opuz"
 * @return File descriptor (>= 0) on success, -1 on failure
 */
int Android_OpenMsuFileDescriptor(const char *filename) {
    JavaVM *vm = GetJavaVM();
    if (!vm) {
        LOGD("Android_OpenMsuFileDescriptor: JavaVM not initialized");
        return -1;
    }

    JNIEnv *env = NULL;
    int getEnvStat = (*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6);

    // If GetEnv fails, we need to attach the current thread
    if (getEnvStat == JNI_EDETACHED) {
        LOGD("Android_OpenMsuFileDescriptor: Attaching thread to JavaVM");
        if ((*vm)->AttachCurrentThread(vm, &env, NULL) != 0) {
            LOGD("Android_OpenMsuFileDescriptor: Failed to attach thread");
            return -1;
        }
    } else if (getEnvStat != JNI_OK) {
        LOGD("Android_OpenMsuFileDescriptor: Failed to get JNIEnv");
        return -1;
    }

    // Find MainActivity class
    jclass mainActivityClass = (*env)->FindClass(env, "com/dishii/zelda3/MainActivity");
    if (!mainActivityClass) {
        LOGD("Android_OpenMsuFileDescriptor: Failed to find MainActivity class");
        return -1;
    }

    // Find the static openMsuFile method
    jmethodID openMsuFileMethod = (*env)->GetStaticMethodID(env, mainActivityClass,
                                                             "openMsuFile",
                                                             "(Ljava/lang/String;)I");
    if (!openMsuFileMethod) {
        LOGD("Android_OpenMsuFileDescriptor: Failed to find openMsuFile method");
        (*env)->DeleteLocalRef(env, mainActivityClass);
        return -1;
    }

    // Convert C string to Java string
    jstring jfilename = (*env)->NewStringUTF(env, filename);
    if (!jfilename) {
        LOGD("Android_OpenMsuFileDescriptor: Failed to create Java string");
        (*env)->DeleteLocalRef(env, mainActivityClass);
        return -1;
    }

    // Call Java method to get file descriptor
    jint fd = (*env)->CallStaticIntMethod(env, mainActivityClass, openMsuFileMethod, jfilename);

    // Clean up local references
    (*env)->DeleteLocalRef(env, jfilename);
    (*env)->DeleteLocalRef(env, mainActivityClass);

    LOGD("Android_OpenMsuFileDescriptor: filename='%s', fd=%d", filename, fd);
    return (int)fd;
}

/**
 * Loads an asset file from the APK using Android AssetManager.
 * Called from vulkan.c when loading shader SPIR-V files.
 *
 * @param asset_path Relative path within assets, e.g. "shaders/vert.spv"
 * @param out_size Pointer to store the size of the loaded data
 * @return Pointer to allocated buffer containing asset data, or NULL on failure
 *         Caller must free() the returned buffer.
 */
void* Android_LoadAsset(const char *asset_path, int *out_size) {
    JavaVM *vm = GetJavaVM();
    if (!vm) {
        LOGD("Android_LoadAsset: JavaVM not initialized");
        return NULL;
    }

    JNIEnv *env = NULL;
    int getEnvStat = (*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6);

    // If GetEnv fails, we need to attach the current thread
    if (getEnvStat == JNI_EDETACHED) {
        LOGD("Android_LoadAsset: Attaching thread to JavaVM");
        if ((*vm)->AttachCurrentThread(vm, &env, NULL) != 0) {
            LOGD("Android_LoadAsset: Failed to attach thread");
            return NULL;
        }
    } else if (getEnvStat != JNI_OK) {
        LOGD("Android_LoadAsset: Failed to get JNIEnv");
        return NULL;
    }

    // Find MainActivity class
    jclass mainActivityClass = (*env)->FindClass(env, "com/dishii/zelda3/MainActivity");
    if (!mainActivityClass) {
        LOGD("Android_LoadAsset: Failed to find MainActivity class");
        return NULL;
    }

    // Find the static loadAsset method
    jmethodID loadAssetMethod = (*env)->GetStaticMethodID(env, mainActivityClass,
                                                           "loadAsset",
                                                           "(Ljava/lang/String;)[B");
    if (!loadAssetMethod) {
        LOGD("Android_LoadAsset: Failed to find loadAsset method");
        (*env)->DeleteLocalRef(env, mainActivityClass);
        return NULL;
    }

    // Convert C string to Java string
    jstring jassetPath = (*env)->NewStringUTF(env, asset_path);
    if (!jassetPath) {
        LOGD("Android_LoadAsset: Failed to create Java string");
        (*env)->DeleteLocalRef(env, mainActivityClass);
        return NULL;
    }

    // Call Java method to get asset data
    jbyteArray jdata = (jbyteArray)(*env)->CallStaticObjectMethod(env, mainActivityClass,
                                                                   loadAssetMethod, jassetPath);

    (*env)->DeleteLocalRef(env, jassetPath);
    (*env)->DeleteLocalRef(env, mainActivityClass);

    if (!jdata) {
        LOGD("Android_LoadAsset: loadAsset returned null for '%s'", asset_path);
        return NULL;
    }

    // Get array length and copy data
    jsize length = (*env)->GetArrayLength(env, jdata);
    if (length <= 0) {
        LOGD("Android_LoadAsset: Empty or invalid array for '%s'", asset_path);
        (*env)->DeleteLocalRef(env, jdata);
        return NULL;
    }

    // Allocate C buffer and copy data
    void *buffer = malloc(length);
    if (!buffer) {
        LOGD("Android_LoadAsset: malloc failed for %d bytes", length);
        (*env)->DeleteLocalRef(env, jdata);
        return NULL;
    }

    jbyte *data = (*env)->GetByteArrayElements(env, jdata, NULL);
    if (!data) {
        LOGD("Android_LoadAsset: GetByteArrayElements failed");
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

    LOGD("Android_LoadAsset: Loaded '%s' (%d bytes)", asset_path, length);
    return buffer;
}

/**
 * Saves the current game state to the specified slot.
 * Called from MainActivity when user selects a save slot.
 *
 * @param slot Save slot number (0-9, where 0 = Quick Save)
 */
JNIEXPORT void JNICALL Java_com_dishii_zelda3_MainActivity_nativeSaveState(
    JNIEnv* env, jobject obj, jint slot) {
    LOGD("nativeSaveState: slot=%d", slot);
    SaveLoadSlot(0, (int)slot);  // 0 = kSaveLoad_Save
}

/**
 * Loads the game state from the specified slot.
 * Called from MainActivity when user selects a load slot.
 *
 * @param slot Save slot number (0-9, where 0 = Quick Save)
 */
JNIEXPORT void JNICALL Java_com_dishii_zelda3_MainActivity_nativeLoadState(
    JNIEnv* env, jobject obj, jint slot) {
    LOGD("nativeLoadState: slot=%d", slot);
    SaveLoadSlot(1, (int)slot);  // 1 = kSaveLoad_Load
}

/**
 * Gets the current frame buffer as RGBA data for thumbnail generation.
 * Returns 256x224 RGBA image (229,376 bytes).
 *
 * @return byte array containing RGBA data, or null if not available
 */
JNIEXPORT jbyteArray JNICALL Java_com_dishii_zelda3_MainActivity_nativeGetScreenshotRGBA(
    JNIEnv* env, jobject obj) {

    uint8_t *buffer = NULL;
    int width = 0, height = 0, pitch = 0;

    // Get frame buffer from PPU
    PpuGetFrameBuffer(g_zenv.ppu, &buffer, &width, &height, &pitch);

    if (!buffer || width == 0 || height == 0) {
        LOGD("nativeGetScreenshotRGBA: No frame buffer available");
        return NULL;
    }

    LOGD("nativeGetScreenshotRGBA: width=%d, height=%d, pitch=%d", width, height, pitch);

    // Calculate total size: width * height * 4 bytes per pixel (RGBA)
    int totalSize = width * height * 4;

    // Create Java byte array
    jbyteArray result = (*env)->NewByteArray(env, totalSize);
    if (!result) {
        LOGD("nativeGetScreenshotRGBA: Failed to allocate byte array");
        return NULL;
    }

    // Copy pixel data row by row (to handle pitch) and convert BGRA to RGBA
    jbyte *resultData = (*env)->GetByteArrayElements(env, result, NULL);
    if (!resultData) {
        LOGD("nativeGetScreenshotRGBA: Failed to get byte array elements");
        (*env)->DeleteLocalRef(env, result);
        return NULL;
    }

    // PPU outputs BGRA format, but Android Bitmap expects RGBA
    // We need to swap B and R channels
    // Also need to account for extraLeftRight offset in the buffer
    int extraLeftRight = g_zenv.ppu->extraLeftRight;
    LOGD("nativeGetScreenshotRGBA: extraLeftRight=%d", extraLeftRight);

    for (int y = 0; y < height; y++) {
        // Skip extraLeftRight pixels on the left
        uint8_t *src = buffer + (y * pitch) + (extraLeftRight * 4);
        uint8_t *dst = (uint8_t*)resultData + (y * width * 4);

        for (int x = 0; x < width; x++) {
            dst[x * 4 + 0] = src[x * 4 + 2];  // R from B
            dst[x * 4 + 1] = src[x * 4 + 1];  // G stays G
            dst[x * 4 + 2] = src[x * 4 + 0];  // B from R
            dst[x * 4 + 3] = 0xFF;             // Alpha (PPU outputs 0, we want opaque)
        }
    }

    (*env)->ReleaseByteArrayElements(env, result, resultData, 0);

    LOGD("nativeGetScreenshotRGBA: Returning %d bytes (converted BGRA->RGBA)", totalSize);
    return result;
}

/**
 * Toggles the game pause state.
 * Called from MainActivity when user selects "Pause" from nav drawer.
 */
JNIEXPORT void JNICALL Java_com_dishii_zelda3_MainActivity_nativeTogglePause(
    JNIEnv* env, jobject obj) {
    extern uint8 g_paused;
    g_paused = !g_paused;
    LOGD("nativeTogglePause: g_paused=%d", g_paused);
}

/**
 * Gets the current pause state.
 * Called from MainActivity to query pause status.
 *
 * @return true if game is paused, false otherwise
 */
JNIEXPORT jboolean JNICALL Java_com_dishii_zelda3_MainActivity_nativeIsPaused(
    JNIEnv* env, jobject obj) {
    extern uint8 g_paused;
    return (jboolean)g_paused;
}

/**
 * Binds a gamepad button (with optional modifiers) to a command.
 * Called from MainActivity's controller settings dialog.
 *
 * @param buttonName Name of button (e.g., "DpadUp", "A", "Start")
 * @param modifierNames Array of modifier button names (e.g., ["Start", "Select"]), can be null
 * @param commandId Command ID (e.g., 1 for Up, 2 for Down, etc.)
 * @return true if binding succeeded, false otherwise
 */
JNIEXPORT jboolean JNICALL Java_com_dishii_zelda3_MainActivity_nativeBindGamepadButton(
    JNIEnv* env, jobject obj, jstring buttonName, jobjectArray modifierNames, jint commandId) {

    // Convert button name to C string
    const char *btnStr = (*env)->GetStringUTFChars(env, buttonName, NULL);
    if (!btnStr) {
        LOGD("nativeBindGamepadButton: Failed to get button name");
        return JNI_FALSE;
    }

    // Parse button name to button ID
    const char *btnPtr = btnStr;
    int button = ParseGamepadButtonName(&btnPtr);
    (*env)->ReleaseStringUTFChars(env, buttonName, btnStr);

    if (button == -1) {  // kGamepadBtn_Invalid
        LOGD("nativeBindGamepadButton: Invalid button name");
        return JNI_FALSE;
    }

    // Parse modifiers (if any)
    uint32 modifiers = 0;
    if (modifierNames != NULL) {
        jsize modCount = (*env)->GetArrayLength(env, modifierNames);
        for (jsize i = 0; i < modCount; i++) {
            jstring modStr = (jstring)(*env)->GetObjectArrayElement(env, modifierNames, i);
            const char *modCStr = (*env)->GetStringUTFChars(env, modStr, NULL);
            if (modCStr) {
                const char *modPtr = modCStr;
                int modBtn = ParseGamepadButtonName(&modPtr);
                if (modBtn != -1) {
                    modifiers |= (1 << modBtn);
                }
                (*env)->ReleaseStringUTFChars(env, modStr, modCStr);
            }
            (*env)->DeleteLocalRef(env, modStr);
        }
    }

    // Validate command ID (should be in range 0 to kKeys_Total)
    int cmd = (int)commandId;
    if (cmd < 0 || cmd >= 127) {  // kKeys_Total (from config.h:37)
        LOGD("nativeBindGamepadButton: Invalid command ID %d", cmd);
        return JNI_FALSE;
    }

    // Add binding
    GamepadMap_Add(button, modifiers, (uint16)cmd);
    LOGD("nativeBindGamepadButton: Bound button %d (modifiers=%u) to cmd %d", button, modifiers, cmd);
    return JNI_TRUE;
}

/**
 * Unbinds a specific gamepad button+modifier combination.
 * Called from MainActivity's controller settings dialog.
 *
 * @param buttonName Name of button to unbind
 * @param modifierNames Array of modifier button names, can be null
 * @return true if unbinding succeeded, false otherwise
 */
JNIEXPORT jboolean JNICALL Java_com_dishii_zelda3_MainActivity_nativeUnbindGamepadButton(
    JNIEnv* env, jobject obj, jstring buttonName, jobjectArray modifierNames) {

    // TODO: Implement unbind logic (requires modifying config.c to add GamepadMap_Remove)
    LOGD("nativeUnbindGamepadButton: Not yet implemented");
    return JNI_FALSE;
}

/**
 * Clears all gamepad bindings.
 * Called from MainActivity's controller settings dialog.
 */
JNIEXPORT void JNICALL Java_com_dishii_zelda3_MainActivity_nativeClearGamepadBindings(
    JNIEnv* env, jobject obj) {
    GamepadMap_Clear();
    LOGD("nativeClearGamepadBindings: Cleared all gamepad bindings");
}

/**
 * Gets all current gamepad bindings as a JSON string.
 * Called from MainActivity to populate controller settings dialog.
 *
 * @return JSON string with format: [{"commandName":"Up","binding":"DpadUp"},...]
 */
JNIEXPORT jstring JNICALL Java_com_dishii_zelda3_MainActivity_nativeGetGamepadBindings(
    JNIEnv* env, jobject obj) {

    // Button name lookup table
    static const char *const kGamepadButtonNames[] = {
        "A", "B", "X", "Y", "Back", "Guide", "Start", "L3", "R3",
        "L1", "R1", "DpadUp", "DpadDown", "DpadLeft", "DpadRight", "L2", "R2"
    };

    // Build JSON manually (simple approach, no external libs)
    char json[8192] = "[";  // Start JSON array
    int first = 1;

    // Iterate through all possible commands and check if they're bound
    // This covers: Controls (1-12), Save (33-42), Load (13-22), Pause (121), Turbo (123), DisplayPerf (125), Cheats (113-116)
    const int cmdIds[] = {
        1,2,3,4,5,6,7,8,9,10,11,12,  // Controls
        33,34,35,36,37,38,39,40,41,42,  // Save
        13,14,15,16,17,18,19,20,21,22,  // Load
        121,  // Pause
        123,  // Turbo
        125,  // DisplayPerf
        113,114,115,116  // Cheats
    };

    for (int i = 0; i < sizeof(cmdIds)/sizeof(cmdIds[0]); i++) {
        int cmdId = cmdIds[i];
        uint32 modifiers = 0;
        int button = GamepadMap_GetBindingForCommand(cmdId, &modifiers);

        if (button != -1) {
            // Found a binding - get command name
            const char *cmdName = FindCmdName(cmdId);
            if (!cmdName) continue;  // Skip if no name found

            // Build button combo string (e.g., "L2" or "L2+R3")
            char buttonCombo[128] = "";
            snprintf(buttonCombo, sizeof(buttonCombo), "%s", kGamepadButtonNames[button]);

            // Add modifiers
            for (int mod = 0; mod < 17; mod++) {
                if (modifiers & (1 << mod)) {
                    char temp[128];
                    snprintf(temp, sizeof(temp), "%s+%s", buttonCombo, kGamepadButtonNames[mod]);
                    snprintf(buttonCombo, sizeof(buttonCombo), "%s", temp);
                }
            }

            // Add to JSON
            if (!first) strcat(json, ",");
            first = 0;

            char entry[256];
            snprintf(entry, sizeof(entry), "{\"commandName\":\"%s\",\"binding\":\"%s\"}", cmdName, buttonCombo);
            strcat(json, entry);
        }
    }

    strcat(json, "]");  // End JSON array

    LOGD("nativeGetGamepadBindings: Returning JSON: %s", json);
    return (*env)->NewStringUTF(env, json);
}

/**
 * Applies default gamepad bindings.
 * Called from MainActivity when "Reset to defaults" button is clicked,
 * or automatically when a controller is detected with no existing bindings.
 *
 * Applies 13 default bindings:
 * - 12 standard SNES controls (DpadUp/Down/Left/Right, Back/Start, B/A/Y/X, L1/R1)
 * - 1 Turbo binding (L3 → Turbo)
 */
JNIEXPORT void JNICALL Java_com_dishii_zelda3_MainActivity_nativeApplyDefaultGamepadBindings(
    JNIEnv* env, jobject obj) {

    // Apply 12 standard controls
    // Order: Up, Down, Left, Right, Select, Start, A, B, X, Y, L, R
    for (int i = 0; i < 12; i++) {
        GamepadMap_Add(kDefaultGamepadCmds[i], 0, kKeys_Controls + i);
    }

    // Apply Turbo binding (L3 → Turbo)
    GamepadMap_Add(kGamepadBtn_L3, 0, kKeys_Turbo);

    LOGD("nativeApplyDefaultGamepadBindings: Applied 13 default bindings");
}

/**
 * Gets the gamepad button name for a specific command.
 * Returns the button name (e.g., "DpadUp", "A", "L1") or combo (e.g., "L2+R3") or null if not bound.
 *
 * @param cmdId The command ID (e.g., kKeys_Controls+0 for Up)
 * @return Button name string or null
 */
JNIEXPORT jstring JNICALL Java_com_dishii_zelda3_MainActivity_nativeGetButtonForCommand(
    JNIEnv* env, jobject obj, jint cmdId) {

    // Button name lookup table
    static const char *const kGamepadButtonNames[] = {
        "A", "B", "X", "Y", "Back", "Guide", "Start", "L3", "R3",
        "L1", "R1", "DpadUp", "DpadDown", "DpadLeft", "DpadRight", "L2", "R2"
    };

    // Get the binding from config.c
    uint32 modifiers = 0;
    int button = GamepadMap_GetBindingForCommand(cmdId, &modifiers);

    if (button == -1) {
        // Not bound
        LOGD("nativeGetButtonForCommand: cmd=%d -> not bound", cmdId);
        return NULL;
    }

    // Build the button combo string
    static char result[256];
    strcpy(result, kGamepadButtonNames[button]);

    // Add modifier buttons (if any)
    if (modifiers) {
        for (int mod_btn = 0; mod_btn < kGamepadBtn_Count; mod_btn++) {
            if (modifiers & (1 << mod_btn)) {
                strcat(result, "+");
                strcat(result, kGamepadButtonNames[mod_btn]);
            }
        }
    }

    LOGD("nativeGetButtonForCommand: cmd=%d -> button=%s", cmdId, result);
    return (*env)->NewStringUTF(env, result);
}

// Show a Toast notification to the user
void Android_ShowToast(const char* message) {
    JNIEnv *env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if (!env) {
        LOGD("Android_ShowToast: Failed to get JNI environment");
        return;
    }

    // Get MainActivity class
    jclass activityClass = (*env)->FindClass(env, "com/dishii/zelda3/MainActivity");
    if (!activityClass) {
        LOGD("Android_ShowToast: Failed to find MainActivity class");
        return;
    }

    // Get showToast static method
    jmethodID showToastMethod = (*env)->GetStaticMethodID(env, activityClass,
                                                           "showToast",
                                                           "(Ljava/lang/String;)V");
    if (!showToastMethod) {
        LOGD("Android_ShowToast: Failed to find showToast method");
        (*env)->DeleteLocalRef(env, activityClass);
        return;
    }

    // Convert C string to Java string
    jstring jMessage = (*env)->NewStringUTF(env, message);
    if (!jMessage) {
        LOGD("Android_ShowToast: Failed to create Java string");
        (*env)->DeleteLocalRef(env, activityClass);
        return;
    }

    // Call static method
    (*env)->CallStaticVoidMethod(env, activityClass, showToastMethod, jMessage);

    // Cleanup
    (*env)->DeleteLocalRef(env, jMessage);
    (*env)->DeleteLocalRef(env, activityClass);

    LOGD("Android_ShowToast: Successfully showed toast: %s", message);
}

// Update renderer setting in zelda3.ini
void Android_UpdateRendererConfig(const char *renderer) {
    JNIEnv *env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if (!env) {
        LOGD("Android_UpdateRendererConfig: Failed to get JNI environment");
        return;
    }

    // Get MainActivity class
    jclass activityClass = (*env)->FindClass(env, "com/dishii/zelda3/MainActivity");
    if (!activityClass) {
        LOGD("Android_UpdateRendererConfig: Failed to find MainActivity class");
        return;
    }

    // Get updateRendererSetting static method
    jmethodID updateMethod = (*env)->GetStaticMethodID(env, activityClass,
                                                        "updateRendererSetting",
                                                        "(Ljava/lang/String;)V");
    if (!updateMethod) {
        LOGD("Android_UpdateRendererConfig: Failed to find updateRendererSetting method");
        (*env)->DeleteLocalRef(env, activityClass);
        return;
    }

    // Convert C string to Java string
    jstring jRenderer = (*env)->NewStringUTF(env, renderer);
    if (!jRenderer) {
        LOGD("Android_UpdateRendererConfig: Failed to create Java string");
        (*env)->DeleteLocalRef(env, activityClass);
        return;
    }

    // Call static method
    (*env)->CallStaticVoidMethod(env, activityClass, updateMethod, jRenderer);

    // Cleanup
    (*env)->DeleteLocalRef(env, jRenderer);
    (*env)->DeleteLocalRef(env, activityClass);

    LOGD("Android_UpdateRendererConfig: Successfully updated renderer to: %s", renderer);
}
